// Copyright (C) 2022 - 2026 Zukaritasu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "PluginDiscord.h"
#include "PluginInterface.h"
#include "PluginResources.h"
#include "PluginDefinition.h"

#include <cstdlib>
#include <cstdio>
#include <Shlwapi.h>
#include <tchar.h>
#include <time.h>
#include <string>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <chrono>

#include "PluginError.h"

static constexpr DWORD THREAD_UPDATE_INTERVAL_MS = 1000;
static constexpr const char *NPP_NAME = "Notepad++";

extern ConfigManager configManager;
extern NppData nppData;

/**
 * @brief Callback for Discord errors
 * @param message Error message
 */
static void DiscordErrorCallback(const std::string &message) noexcept
{
	printf("Discord Error: %s\n", message.c_str());
}

void RichPresence::InitializePresence()
{
	if (!_callbacks && configManager.GetConfig()._enable)
	{
		try
		{
			_callbacks = new BasicThread(RichPresence::CallBacks, this);
			_idleTimer = new BasicThread(RichPresence::IdlingTimer, this);
		}
		catch (const std::exception &exception)
		{
			if (_callbacks != nullptr)
				Close();
			throw exception;
		}
	}
}

void RichPresence::Update() noexcept
{
	_editorInfo.LoadEditorStatus();

	const PluginConfig config = configManager.GetConfig();

	_p.enableButtonRepository = config._button_repository;
	_p.details = _p.state = _p.repositoryUrl = "";

	// If the current file is private and the option to hide the presence
	// when it is private is enabled, the presence will be closed
	if (config._hide_if_private && !_editorInfo.IsFileInfoEmpty() && _editorInfo.IsCurrentFilePrivate())
	{
		_p.details = "Private File";
		_p.smallText = "";
		_p.largeText = NPP_NAME;
		_p.largeImage = NPP_DEFAULTIMAGE;

		_pTemp = _p;
		if (_drp.IsConnected())
		{
			_drp.SetPresence(_p, _editorInfo.IsTextEditorIdling(),
				DiscordErrorCallback);
		}
		return;
	}

	UpdateAssets();
	if (config._button_repository)
		_p.repositoryUrl = _editorInfo.GetCurrentRepositoryUrl();
	if (!_editorInfo.IsFileInfoEmpty())
	{
		if (!config._hide_details)
			_editorInfo.WriteFormat(_p.details, config._details_format);
		if (!config._hide_state)
			_editorInfo.WriteFormat(_p.state, config._state_format);
	}

	_pTemp = _p;
	if (_drp.IsConnected())
	{
		_drp.SetPresence(_p, _editorInfo.IsTextEditorIdling(),
			DiscordErrorCallback);
	}
}

static void SafeStopAndDelete(BasicThread *&thread) noexcept
{
	if (thread)
	{
		thread->Stop();
		thread->Wait();
		delete thread;
		thread = nullptr;
	}
}

void RichPresence::Close() noexcept
{
	SafeStopAndDelete(_callbacks);
	SafeStopAndDelete(_idleTimer);

	_drp.Close(DiscordErrorCallback);
}

void RichPresence::UpdateAssets() noexcept
{
	_p.smallText = _p.smallImage =
		_p.largeText = _p.largeImage = "";

	bool isFileEmpty = _editorInfo.IsFileInfoEmpty();
	if (!configManager.GetConfig()._lang_image || isFileEmpty)
	{
		_p.largeImage = NPP_DEFAULTIMAGE;
		_p.largeText = NPP_NAME;
	}
	else
	{
		_p.largeImage = _editorInfo.GetLanguageInfo()._large_image;
		_editorInfo.WriteFormat(_p.largeText, configManager.GetConfig()._large_text_format);
		if (_p.largeImage != NPP_DEFAULTIMAGE)
		{
			_p.smallImage = NPP_DEFAULTIMAGE;
			_p.smallText = NPP_NAME;
		}
	}
}

void RichPresence::Connect(volatile bool *keepRunning) noexcept
{
	while (keepRunning && *keepRunning)
	{
		if (_drp.Connect(configManager.GetConfig()._client_id,
				DiscordErrorCallback))
			break;
		BasicThread::Sleep(keepRunning, RPC_TIME_RECONNECTION);
	}
}

// Static thread callback

void RichPresence::CallBacks(void *data, volatile bool *keepRunning) noexcept
{
	try
	{
		RichPresence *rpc = reinterpret_cast<RichPresence *>(data);
		DiscordRichPresence &drp = rpc->_drp;
		// Determines whether a new start timestamp should be generated for Discord presence.
		// Set to false after the first connection to maintain persistence during reconnections
		bool shouldInitializeTime = true;

		while (*keepRunning)
		{
			rpc->Connect(keepRunning);
			if (!(*keepRunning))
				return;

			Presence pTemp = rpc->_pTemp;

			if (shouldInitializeTime)
			{
				AutoUnlock lock(rpc->_mutex);
				// Set the start time to the current time
				rpc->_p.startTime = std::chrono::duration_cast<std::chrono::seconds>(
										std::chrono::system_clock::now().time_since_epoch())
										.count();
				pTemp.startTime = rpc->_p.startTime;
				shouldInitializeTime = false;
			}

			drp.SetPresence(pTemp, false, DiscordErrorCallback);

			while (*keepRunning)
			{
				drp.Update(DiscordErrorCallback);
				if (!*keepRunning || !drp.IsConnected())
					break;
				BasicThread::Sleep(keepRunning, RPC_UPDATE_TIME);
			}
		}
	}
	catch (const std::exception &e)
	{
		QueueErrorMessage(e.what());
		return;
	}
}

void RichPresence::IdlingTimer(void *data, volatile bool *keepRunning) noexcept
{
	try
	{
		RichPresence *rpc = reinterpret_cast<RichPresence *>(data);

		bool isIdling = false;

		while (*keepRunning)
		{
			BasicThread::Sleep(keepRunning, THREAD_UPDATE_INTERVAL_MS);
			if (configManager.GetConfig()._hide_idle_status)
				continue;

			if (rpc->_drp.LastUpdateTimeElapsed(configManager.GetConfig()._idle_time))
			{
				if (isIdling)
					continue;
				isIdling = true;

				Presence p;
				p.details = "Idling";
				p.largeText = NPP_NAME;
				p.largeImage = NPP_IDLEIMAGE;

				{
					AutoUnlock lock(rpc->_mutex);
					p.startTime = rpc->_p.startTime;
				}

				rpc->_drp.SetIdleStatus(&p, DiscordErrorCallback);
			}
			else if (isIdling)
			{
				isIdling = false;
				rpc->_drp.SetIdleStatus(nullptr, DiscordErrorCallback);
			}
		}
	}
	catch (const std::exception &e)
	{
		QueueErrorMessage(e.what());
		return;
	}
}
