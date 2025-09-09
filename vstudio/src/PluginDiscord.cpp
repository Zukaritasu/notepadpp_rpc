// Copyright (C) 2022 Zukaritasu
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
volatile bool isClosing = false;

// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ //

void RichPresence::Init()
{
	if (_callbacks == nullptr && config._enable)
	{
		try
		{
			_callbacks = new BasicThread(RichPresence::CallBacks, this);
			_status = new BasicThread(RichPresence::Status, this);
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
	if (isClosing)
	{
		return;
	}

	format.LoadEditorStatus();
	mutex.Lock();

	ResetElapsedTime();

	_rpc.details[0] = _rpc.state[0] = '\0';
	if (updateLook)
	{
		UpdateAssets();
	}
	if (!format.IsFileInfoEmpty())
	{
		if (!config._hide_details)
			format.WriteFormat(_rpc.details, config._details_format);
		if (!config._hide_state)
			format.WriteFormat(_rpc.state, config._state_format);
#ifdef _DEBUG
		printf("\n\n Updating...\n - %s\n - %s\n - %s\n", _rpc.details, _rpc.state, _rpc.assets.large_text);
#endif // _DEBUG
	}

	if (_core)
	{
		IDiscordActivityManager* manager = _core->get_activity_manager(_core.get());
		manager->update_activity(manager, &_rpc, nullptr,
#ifdef _DEBUG
			[](void*, EDiscordResult result) {
			if (result != DiscordResult_Ok)
				printf("[RPC] Update presence | %d\n", static_cast<int>(result));
		}
#else
			nullptr
#endif // _DEBUG
		);
	}

	mutex.Unlock();
}

void RichPresence::Close() noexcept
{
	isClosing = true;
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

		if (_core)
		{
			// The rich presence is closed and the discord core is destroyed
			auto activity_manager = _core->get_activity_manager(_core.get());
			activity_manager->clear_activity(activity_manager, nullptr, [](void*, EDiscordResult result) {
				if (result != DiscordResult_Ok) {
					std::wstring message = L"An error occurred while clearing the activity\nError code: " + std::to_wstring(static_cast<int>(result));
					wprintf(L"%ws\n", message.c_str());
				} else {
					printf("[RPC] Activity has been cleared\n");
				}
			});
			_core->run_callbacks(_core.get());
			CoreDestroy();
		}
		// The elapsed time is reset in case of a new reconnection  
		_rpc.timestamps.start = 0;
#ifdef _DEBUG
		printf(" > Presence has been closed\n");
#endif // _DEBUG
		mutex.Unlock();
	}
}


void RichPresence::UpdateAssets() noexcept
{
	DiscordActivityAssets& assets = _rpc.assets;
	assets.small_image[0] = assets.small_text[0] = 
		assets.large_text[0] = assets.large_image[0] = '\0';

	if (!config._lang_image)
	{
		strcpy(assets.large_image, NPP_DEFAULTIMAGE);
		strcpy(assets.large_text, NPP_NAME);
	}
	else if (!format.IsFileInfoEmpty())
	{
		strcpy(assets.large_image, format.GetLanguageInfo()._large_image);
		format.WriteFormat(assets.large_text, config._large_text_format);
		if (strcmp(format.GetLanguageInfo()._large_image, NPP_DEFAULTIMAGE) != 0)
		{
			strcpy(assets.small_image, NPP_DEFAULTIMAGE);
			strcpy(assets.small_text, NPP_NAME);
		}
	}
}

bool RichPresence::Connect(IDiscordCore** core) noexcept
{
	DiscordCreateParams params;
	DiscordCreateParamsSetDefault(&params);

	params.client_id = config._client_id;
	params.flags     = DiscordCreateFlags_NoRequireDiscord;

reconnect:
	EDiscordResult result = ::DiscordCreate(DISCORD_VERSION, &params, core);
#ifdef _DEBUG
	printf(" > Connect result code: %d\n", static_cast<int>(result));
#endif // _DEBUG

	if (result == DiscordResult_Ok)
		return true;
	else if (result == DiscordResult_InternalError || result == DiscordResult_NotRunning)
	{
#ifdef _DEBUG
		printf(" > reconnecting...\n");
#endif // _DEBUG
		Sleep(2000);
		goto reconnect;
	}
	::ShowDiscordError(result);
	return false;
}

void RichPresence::ResetElapsedTime() noexcept
{
	if (!config._elapsed_time)
		_rpc.timestamps.start = 0;
	else if (_rpc.timestamps.start == 0)
	{
		_rpc.type = DiscordActivityType_Playing;
		_rpc.timestamps.start = _time64(nullptr);
	}
}

void RichPresence::CoreDestroy() noexcept
{
	if (_core)
	{
		_core->destroy(_core.get());
		_core.release();
	}
}

void RichPresence::CallBacks(void* data, volatile bool* keepRunning) noexcept
{
	RichPresence* rpc = reinterpret_cast<RichPresence*>(data);
	IDiscordCore* discord_core = nullptr;
reconnect:
	if (!rpc->Connect(&discord_core) || discord_core == nullptr)
		return;
	rpc->_core.reset(discord_core);

#ifdef _DEBUG
	mutex.Lock();
	rpc->_core->set_log_hook(rpc->_core.get(), DiscordLogLevel_Debug, nullptr,
		[](void*, EDiscordLogLevel level, const char* message)
	{
		printf("[LOG] Level: %d | Message: %s\n", static_cast<int>(level), message);
	});
	mutex.Unlock();
#endif // _DEBUG

	rpc->ResetElapsedTime();
	rpc->_active = true;
	rpc->Update();

	while (rpc->_active)
	{
		if (!(*keepRunning))
		{
			rpc->_active = false;
			break;
		}

		mutex.Lock();
		auto result = rpc->_core->run_callbacks(rpc->_core.get());
		if (result != DiscordResult_Ok)
		{
#ifdef _DEBUG
			printf("[RUN] - %d\n", static_cast<int>(result));
#endif // _DEBUG
			rpc->_active = false;
			rpc->CoreDestroy();
			mutex.Unlock();
			goto reconnect;
		}
		mutex.Unlock();
		::Sleep(RPC_UPDATE_TIME / 60);
	}
}

#pragma warning(disable: 4127) // while (true) const expression

void RichPresence::Status(void* data, volatile bool* keepRunning) noexcept
{
	const unsigned refreshTime = config._refreshTime;

	while (*keepRunning)
	{
		::Sleep(refreshTime);
		if (!*keepRunning)
			break;
		reinterpret_cast<RichPresence*>(data)->Update(false);
	}
#ifdef _DEBUG
	printf(" > status finished!\n");
#endif // _DEBUG
}
