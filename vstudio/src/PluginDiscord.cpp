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
#include "PluginError.h"
#include "PluginConfig.h"
#include "discord_game_sdk.h"
#include "PluginInterface.h"
#include "PluginRPCFormat.h"
#include "PluginUtil.h"

#include <cstdlib>
#include <cstdio>
#include <mutex>
#include <thread>
#include <Shlwapi.h>
#include <tchar.h>
#include <time.h>
#include <string>
#include <strsafe.h>

#pragma warning(disable: 4996)

#define NPP_NAME          "Notepad++"


extern PluginConfig config;
extern NppData      nppData;
std::mutex          mutex;

namespace 
{
	// the rich presence of the current file
	DiscordActivity rpc{};
	std::unique_ptr<IDiscordCore> _core;
	// thread that keeps active the rich presence
	HANDLE          run_callbacks  = nullptr; 
	// Updates the rich presence every so often by
	// changes in the editor, including the number of lines in the file
	HANDLE          sci_status     = nullptr;
	volatile bool   rpc_active     = false;
}

// Size buffer (MAX_PATH)
static void GetEditorProperty(char* buf, int prop)
{
	buf[0] = '\0';
#ifndef UNICODE
	SNDMSG(nppData._nppHandle, prop, MAX_PATH, reinterpret_cast<LPARAM>(buf));
#else
	wchar_t buffer[MAX_PATH] = { L'\0' };
	SNDMSG(nppData._nppHandle, prop, MAX_PATH, reinterpret_cast<LPARAM>(buffer));
	WideCharToMultiByte(CP_UTF8, 0, buffer, -1, buf, MAX_PATH, nullptr, FALSE);
#endif // UNICODE
}

static void InitializeElapsedTime()
{
	if (!config._elapsed_time) 
		rpc.timestamps.start = 0;
	else if (rpc.timestamps.start == 0) 
	{	
		rpc.type = DiscordActivityType_Playing;
		rpc.timestamps.start = _time64(nullptr);
	}
}

static void UpdateLookRichPresence(DiscordActivityAssets& look, FileInfo& info)
{
	look.small_image[0] = look.small_text[0] = look.large_text[0] = look.large_image[0] = '\0';
	
	// The default large image is used
	if (!config._lang_image)
	{
		strcpy(look.large_image, NPP_DEFAULTIMAGE);
		strcpy(look.large_text, NPP_NAME);
	}
	// When Notepad++ starts, the title of the window is "Notepad++", that means
	// that a file is not yet open or Scintilla has not yet been started with a
	// default file, in that case it cannot continue
	else if (info.name[0] != '\0')
	{
		LanguageInfo lang_info = GetLanguageInfo(strlwr(info.extension));
		strcpy(look.large_image, lang_info.large_image);
		ProcessFormat(look.large_text, config._large_text_format, &info, &lang_info);
		
		// If the language is the default, the large image is left and the
		// small image is not established.
		if (strcmp(lang_info.large_image, NPP_DEFAULTIMAGE) != 0)
		{
			strcpy(look.small_image, NPP_DEFAULTIMAGE);
			strcpy(look.small_text, NPP_NAME);
		}
	}
}

static void DiscordUpdatePresence(bool updateLook)
{
	FileInfo info;
	GetEditorProperty(info.name, NPPM_GETFILENAME); /* filename */
	GetEditorProperty(info.extension, NPPM_GETEXTPART); /* extension */
	
	mutex.lock();
	
	InitializeElapsedTime();
	
	rpc.details[0] = rpc.state[0] = '\0';
	if (updateLook)
		UpdateLookRichPresence(rpc.assets, info);
	if (info.name[0] != '\0')
	{
		if (!config._hide_details)
			ProcessFormat(rpc.details, config._details_format, &info);
		if (!config._hide_state)
			ProcessFormat(rpc.state, config._state_format, &info);
#ifdef _DEBUG
		printf("\n\n Updating...\n - %s\n - %s\n - %s\n", rpc.details, rpc.state, rpc.assets.large_text);
#endif // _DEBUG
	}
	
	if (_core)
	{
		IDiscordActivityManager* manager = _core->get_activity_manager(_core.get());
		manager->update_activity(manager, &rpc, nullptr,
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

	mutex.unlock();
}

void DiscordUpdatePresence()
{
	DiscordUpdatePresence(true);
}

static void DiscordCoreDestroy()
{
	if (_core)
	{
		_core->destroy(_core.get());
		_core.release();
	}
}

#pragma warning(disable: 4127) // while (true) const expression

static DWORD WINAPI ScintillaStatus(LPVOID)
{
	while (true)
	{
		Sleep(10000);
		DiscordUpdatePresence(false);
	}
	return ERROR_SUCCESS;
}

static DWORD WINAPI RunCallBacks(LPVOID)
{
	IDiscordCore*       discord_core = nullptr;
	DiscordCreateParams params;
	DiscordCreateParamsSetDefault(&params);

	params.client_id = config._client_id;
	params.flags     = DiscordCreateFlags_NoRequireDiscord;
	
#ifdef _DEBUG
	bool reconnecting = false;
#endif // _DEBUG

reconnect:
	EDiscordResult result = DiscordCreate(DISCORD_VERSION, &params, &discord_core);
	if (result != DiscordResult_Ok)
	{
		if (result == DiscordResult_InternalError || result == DiscordResult_NotRunning)
		{
#ifdef _DEBUG
			reconnecting = true;
			printf(" > Reconnecting...\n");
#endif // _DEBUG
			Sleep(2000);
			goto reconnect;
		}
		else
		{
			ShowDiscordError(result);
			return static_cast<DWORD>(result);
		}
	}

	_core.reset(discord_core);
#ifdef _DEBUG
	mutex.lock();
	_core->set_log_hook(_core.get(), DiscordLogLevel_Debug, nullptr,
		[](void*, EDiscordLogLevel level, const char* message)
	{
		printf("[LOG] Level: %d | Message: %s\n", static_cast<int>(level), message);
	});
	mutex.unlock();
#endif // _DEBUG

#ifdef _DEBUG
	if (reconnecting)
		printf(" > Successful reconnection!\n");
#endif // _DEBUG

	InitializeElapsedTime();

	rpc_active = true;
	DiscordUpdatePresence(true);
	
	while (rpc_active)
	{
		mutex.lock();
		if ((result = _core->run_callbacks(_core.get())) != DiscordResult_Ok)
		{
#ifdef _DEBUG
			printf("[RUN] - %d\n", static_cast<int>(result));
#endif // _DEBUG
			rpc_active = false;
			DiscordCoreDestroy();
			mutex.unlock();
			goto reconnect;
		}
		mutex.unlock();
		Sleep(1000 / 60);
	}

	return DiscordResult_Ok;
}

void DiscordInitPresence()
{
	if (run_callbacks == nullptr && config._enable)
	{
		run_callbacks = CreateThread(nullptr, 0, RunCallBacks, nullptr, 0, nullptr);
		if (run_callbacks == nullptr)
			ShowLastError();
		else
		{
			sci_status = CreateThread(nullptr, 0, ScintillaStatus, nullptr, 0, nullptr);
			if (sci_status == nullptr)
			{
				ShowLastError();
				DiscordClosePresence();
			}
		}
	}
}

void DiscordClosePresence()
{
	// The thread that keeps monitoring the state of Scintilla is closed
	if (sci_status != nullptr)
	{
		mutex.lock();
		WaitForSingleObject(sci_status, 0);
		TerminateThread(sci_status, EXIT_SUCCESS);
		mutex.unlock();
		CloseHandle(sci_status);
		sci_status = nullptr;
	}
	
	// The thread that keeps the rich presence active closes
	if (run_callbacks != nullptr)
	{
		// If the variable 'rpc_active' is false it means that the rich presence
		// has not yet been activated or is in reconnection state
		if (rpc_active)
		{
			rpc_active = false;
			WaitForSingleObject(run_callbacks, INFINITE);
		}
		else
			WaitForSingleObject(run_callbacks, 0);
		
		CloseHandle(run_callbacks);
		run_callbacks = nullptr;
		
		mutex.lock();
		if (_core)
		{
			// The rich presence is closed and the discord core is destroyed
			auto activity_manager = _core->get_activity_manager(_core.get());
			activity_manager->clear_activity(activity_manager, nullptr, nullptr);
			_core->run_callbacks(_core.get());
			DiscordCoreDestroy();
		}
		// The elapsed time is reset in case of a new reconnection  
		rpc.timestamps.start = 0;
#ifdef _DEBUG
		printf(" > Presence has been closed\n");
#endif // _DEBUG
		mutex.unlock();
	}
}
