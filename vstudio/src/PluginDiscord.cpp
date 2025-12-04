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
	const char* NPP_NAME = "Notepad++";

	// Incremental counter of the time elapsed since the last idle in the code editor
	std::atomic<int>  currentIdleTime = 0;
}

extern PluginConfig config;
extern NppData nppData;

// NOTE: called from the PluginDlgOption.cpp file
BasicMutex mutex;


void RichPresence::Init()
{
	if (_callbacks == nullptr && config._enable)
	{
		try
		{
			_callbacks = new BasicThread(RichPresence::CallBacks, this);
			_status = new BasicThread(RichPresence::Status, this);
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
	if (!_drp.IsConnected())
		return;

	_format.LoadEditorStatus();
	mutex.Lock();

	_p.enableButtonRepository = config._button_repository;
	_p.details = _p.state = _p.repositoryUrl =  "";

	if (!_format.IsTextEditorIdle())
		currentIdleTime.store(0);

	if (currentIdleTime.load() >= config._idle_time)
	{
		if (config._hide_idle_status)
			currentIdleTime.store(0);
		else
		{
			_p.details = "Idle";
			_p.smallText = _p.smallImage = "";
			_p.largeText = NPP_NAME;
			_p.largeImage = NPP_IDLEIMAGE;

			_drp.SetPresence(_p);

			mutex.Unlock();
			return;
		}
	}

	// If the current file is private and the option to hide the presence
	// when it is private is enabled, the presence will be closed
	if (config._hide_if_private && !_format.IsFileInfoEmpty() && _format.IsCurrentFilePrivate())
	{
		_p.details = "Private File";
		_p.smallText = _p.smallImage = "";
		_p.largeText = NPP_NAME;
		_p.largeImage = NPP_DEFAULTIMAGE;
		_drp.SetPresence(_p);
		
		mutex.Unlock();
		return;
	}

	UpdateAssets();
	if (config._button_repository)
		_p.repositoryUrl = _format.GetCurrentRepositoryUrl();
	if (!_format.IsFileInfoEmpty())
	{
		if (!config._hide_details)
			_format.WriteFormat(_p.details, config._details_format);
		if (!config._hide_state)
			_format.WriteFormat(_p.state, config._state_format);
	}

	_drp.SetPresence(_p);
	mutex.Unlock();
}

static void SafeStopAndDelete(BasicThread*& thread) noexcept
{
	if (thread != nullptr)
	{
		thread->Stop();
		thread->Wait();
		delete thread;
		thread = nullptr;
	}
}

void RichPresence::Close() noexcept
{
	mutex.Lock();
	SafeStopAndDelete(_callbacks);
	SafeStopAndDelete(_status);
	SafeStopAndDelete(_idleTimer);

	_drp.Close();
	mutex.Unlock();
}

void RichPresence::UpdateAssets() noexcept
{
	_p.smallText = _p.smallImage = 
	_p.largeText = _p.largeImage = "";

	if (!config._lang_image)
	{
		_p.largeImage = NPP_DEFAULTIMAGE;
		_p.largeText = NPP_NAME;
	}
	else if (!_format.IsFileInfoEmpty())
	{
		_p.largeImage = _format.GetLanguageInfo()._large_image;
		_format.WriteFormat(_p.largeText, config._large_text_format);
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
		if (_drp.Connect(config._client_id))
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

		auto now = std::chrono::system_clock::now();
		auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
		// Set the start time to the current time
		rpc->_p.startTime = timestamp;
		rpc->Update();

		while (*keepRunning)
		{
			mutex.Lock();

			drp.Update();
			if (!*keepRunning || !drp.IsConnected())
			{
				mutex.Unlock();
				break;
			}
			mutex.Unlock();
			BasicThread::Sleep(keepRunning, RPC_UPDATE_TIME / 60);
		}
	}
}

void RichPresence::Status(void* data, volatile bool* keepRunning) noexcept
{
	const unsigned refreshTime = config._refreshTime;
	while (*keepRunning)
	{
		BasicThread::Sleep(keepRunning, refreshTime);
		if (!*keepRunning)
			break;
		reinterpret_cast<RichPresence*>(data)->Update();
	}
}

void RichPresence::IdleTimer(void*, volatile bool* keepRunning) noexcept
{
	currentIdleTime.store(0);
	while (*keepRunning)
	{
		BasicThread::Sleep(keepRunning, 1000);
		currentIdleTime.fetch_add(1);
	}
}
