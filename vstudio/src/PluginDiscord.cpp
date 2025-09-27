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

#include "PluginError.h"

#pragma warning(disable: 4996)

namespace
{
	const char* NPP_NAME = "Notepad++";
}

extern PluginConfig config;


BasicMutex mutex;
volatile bool isClosed = false;
volatile int currentIdleTime = 0;




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

void RichPresence::Update(bool updateLook) noexcept
{
	if (isClosed) return;

	format.LoadEditorStatus();
	mutex.Lock();

	_p.enableButtonRepository = config._button_repository;
	_p.details = _p.state = _p.repositoryUrl =  "";

	if (!format.IsTextEditorIdle())
		currentIdleTime = 0;
	if (currentIdleTime >= config._idle_time)
	{
		if (config._hide_idle_status)
			currentIdleTime = 0;
		else
		{
			_p.details = "Idle";
			_p.smallText = _p.smallImage = "";
			_p.largeText = NPP_NAME;
			_p.largeImage = NPP_IDLEIMAGE;
			_drp.SetPresence(_p, [](const std::string& error) {
				printf(" > Discord SetPresence Error: %s\n", error.c_str());
				});
			mutex.Unlock();
			return;
		}
	}

	// If the current file is private and the option to hide the presence
	// when it is private is enabled, the presence will be closed
	if (config._hide_if_private && !format.IsFileInfoEmpty() && format.IsCurrentFilePrivate())
	{
		_p.details = "Private File";
		_p.smallText = _p.smallImage = "";
		_p.largeText = NPP_NAME;
		_p.largeImage = NPP_DEFAULTIMAGE;
		_drp.SetPresence(_p, [](const std::string& error) {
			printf(" > Discord SetPresence Error: %s\n", error.c_str());
			});
		mutex.Unlock();
		return;
	}

	if (updateLook)
		UpdateAssets();
	if (config._button_repository)
		_p.repositoryUrl = format.GetCurrentRepositoryUrl();
	if (!format.IsFileInfoEmpty())
	{
		if (!config._hide_details)
			format.WriteFormat(_p.details, config._details_format);
		if (!config._hide_state)
			format.WriteFormat(_p.state, config._state_format);
	}

	_drp.SetPresence(_p, [](const std::string& error) {
		printf(" > Discord SetPresence Error: %s\n", error.c_str());
	});
	
	mutex.Unlock();
}

void RichPresence::Close() noexcept
{
	isClosed = true;
	if (_status != nullptr)
	{
		mutex.Lock();
		_status->Stop();
		delete _status;
		_status = nullptr;
		mutex.Unlock();
	}

	if (_callbacks != nullptr)
	{
		if (_active)
			_callbacks->Stop();
		else
			_callbacks->Kill();

		delete _callbacks;
		_callbacks = nullptr;

		mutex.Lock();

		_drp.Close();

		mutex.Unlock();
	}

	if (_idleTimer != nullptr)
	{
		_idleTimer->Stop();
		::Sleep(60); // Give it some time to close properly
		delete _idleTimer;
		_idleTimer = nullptr;
	}
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
	else if (!format.IsFileInfoEmpty())
	{
		_p.largeImage = format.GetLanguageInfo()._large_image;
		format.WriteFormat(_p.largeText, config._large_text_format);
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

void RichPresence::Connect() noexcept
{
reconnect:
	if (!_drp.Connect(config._client_id, [](const std::string& error) {
		printf(" > Discord Connect Error: %s\n", error.c_str());
	}))
	{
		printf(" > Retrying connection in 2 seconds...\n");
		::Sleep(2000);
		goto reconnect;
	}
}

void RichPresence::CallBacks(void* data, volatile bool* keepRunning) noexcept
{
	RichPresence* rpc = reinterpret_cast<RichPresence*>(data);
	auto& drp = rpc->_drp;
reconnect:
	rpc->Connect();

	rpc->_active = true;

	auto now = std::chrono::system_clock::now();
	auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
	rpc->_p.startTime = timestamp;

	isClosed = false;

	rpc->Update();

	while (rpc->_active)
	{
		if (!(*keepRunning))
		{
			rpc->_active = false;
			break;
		}

		mutex.Lock();

		drp.Update([](const std::string& error) {
			printf(" > Discord Update Error: %s\n", error.c_str());
		});
		if (!drp.IsConnected() || !drp.CheckConnection([](const std::string& error) {
			printf(" > Discord Connection Error: %s\n", error.c_str());
		}))
		{
			rpc->_active = false;
			mutex.Unlock();
			goto reconnect;
		}
		mutex.Unlock();
		::Sleep(RPC_UPDATE_TIME / 60);
	}
}

void RichPresence::Status(void* data, volatile bool* keepRunning) noexcept
{
	const unsigned refreshTime = config._refreshTime;

	while (*keepRunning)
	{
		::Sleep(refreshTime);
		if (!*keepRunning)
			break;
		reinterpret_cast<RichPresence*>(data)->Update();
	}
}

void RichPresence::IdleTimer(void*, volatile bool* keepRunning) noexcept
{
	int milliseconds = 0;
	while (*keepRunning)
	{
		// 50 ms interval for better precision and responsiveness for stopping the timer
		milliseconds += 50;
		if (milliseconds >= 1000)
		{
			currentIdleTime++;
			milliseconds = 0;
		}
		::Sleep(50);
	}
}
