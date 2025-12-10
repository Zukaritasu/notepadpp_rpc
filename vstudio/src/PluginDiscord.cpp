// Copyright (C) 2022 - 2025 Zukaritasu
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

#pragma warning(disable: 4996)

namespace
{
	constexpr char* NPP_NAME = "Notepad++";
}

extern ConfigManager configManager;
extern NppData nppData;

void RichPresence::InitializePresence()
{
	if (!_callbacks && configManager.GetConfig()._enable)
	{
		try
		{
			_callbacks = new BasicThread(RichPresence::CallBacks, this);
			_idleTimer = new BasicThread(RichPresence::IdleTimer, this);
		}
		catch (const std::exception& exception)
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
	_p.details = _p.state = _p.repositoryUrl =  "";

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
			_drp.SetPresence(_p);
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
		_drp.SetPresence(_p);
	}
}

static void SafeStopAndDelete(BasicThread*& thread) noexcept
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

	_drp.Close();
}

void RichPresence::UpdateAssets() noexcept
{
	_p.smallText = _p.smallImage = 
	_p.largeText = _p.largeImage = "";

	if (!configManager.GetConfig()._lang_image)
	{
		_p.largeImage = NPP_DEFAULTIMAGE;
		_p.largeText = NPP_NAME;
	}
	else if (!_editorInfo.IsFileInfoEmpty())
	{
		_p.largeImage = _editorInfo.GetLanguageInfo()._large_image;
		_editorInfo.WriteFormat(_p.largeText, configManager.GetConfig()._large_text_format);
		if (_p.largeImage != NPP_DEFAULTIMAGE)
		{
			_p.smallImage = NPP_DEFAULTIMAGE;
			_p.smallText = NPP_NAME;
		}
	}
	else
	{
		_p.largeImage = NPP_DEFAULTIMAGE;
		_p.largeText = NPP_NAME;
	}
}

void RichPresence::Connect(volatile bool* keepRunning) noexcept
{
	while (keepRunning != nullptr && *keepRunning)
	{
		if (_drp.Connect(configManager.GetConfig()._client_id))
			break;
		BasicThread::Sleep(keepRunning, RPC_TIME_RECONNECTION);
	}
}

/////////
// Threads --------------------------
////////

void RichPresence::CallBacks(void* data, volatile bool* keepRunning) noexcept
{
	RichPresence* rpc = reinterpret_cast<RichPresence*>(data);
	DiscordRichPresence& drp = rpc->_drp;
	
	while (*keepRunning)
	{
		rpc->Connect(keepRunning);
		if (!(*keepRunning))
			return;

		// Set the start time to the current time
		rpc->_p.startTime = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		
		Presence pTemp = rpc->_pTemp;
		pTemp.startTime = rpc->_p.startTime;

		drp.SetPresence(pTemp);
		while (*keepRunning)
		{
			drp.Update();
			if (!*keepRunning || !drp.IsConnected())
				break;
			BasicThread::Sleep(keepRunning, RPC_UPDATE_TIME);
		}
	}
}

void RichPresence::IdleTimer(void* data, volatile bool* keepRunning) noexcept
{
	RichPresence* rpc = reinterpret_cast<RichPresence*>(data);

	bool isIdle = false;
	
	while (*keepRunning)
	{
		BasicThread::Sleep(keepRunning, 1000);
		if (configManager.GetConfig()._hide_idle_status)
			continue;
		
		if (rpc->_drp.LastUpdateTimeElapsed(configManager.GetConfig()._idle_time))
		{
			if (isIdle) continue;
			isIdle = true;

			Presence p;

			p.details = "Idle";
			p.largeText = NPP_NAME;
			p.largeImage = NPP_IDLEIMAGE;
			p.startTime = rpc->_p.startTime;

			rpc->_drp.SetIdleStatus(p);
		}
		else
		{
			isIdle = false;
		}
	}
}
