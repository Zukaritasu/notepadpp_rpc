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

#include <cstdlib>
#include <cstdio>
#include <mutex>
#include <thread>
#include <Shlwapi.h>
#include <tchar.h>
#include <time.h>

#pragma warning(disable: 4996)

#define DEFAULT_LANGIMAGE "favicon"
#define NPP_NAME          "Notepad++"

struct FileData
{
	char  name[MAX_PATH];
	TCHAR path[MAX_PATH];
	char  extension[MAX_PATH];
};

namespace 
{
	FileData        filedata{};
	DiscordActivity rpc{};
	IDiscordCore*   core = nullptr;
	HANDLE          thread = nullptr;
	volatile bool   loop_while = false;
}

extern PluginConfig config;
extern NppData      nppData;
extern std::mutex   mutex;

static void SetDefaultValues(DiscordActivity& activity)
{
	ZeroMemory(&activity, sizeof(DiscordActivity));
	activity.type             = DiscordActivityType_Playing;
	activity.timestamps.start = _time64(nullptr);
}

static __int64 GetFileSize(TCHAR* filename)
{
	if (*filename && PathFileExists(filename))
	{
		struct _stat64i32 stats {};
		if (_tstat(filename, &stats) == 0) return stats.st_size;
	}
	return -1;
}

static const char* GetLanguageLargeImage(const char* extension)
{
	LangType current_lang = L_EXTERNAL;
	SendMessage(nppData._nppHandle, NPPM_GETCURRENTLANGTYPE, 0, (LPARAM)&current_lang);
	switch (current_lang)
	{
	case L_JAVA:          return "java";
	case L_JAVASCRIPT: // Javascript and typescript
	case L_JS:            
		if (strcmp(extension, "ts") == 0 || strcmp(extension, "tsx") == 0)
			return "typescript";
		return "javascript";
	case L_C:             return "c";
	case L_CPP:           return "cpp";
	case L_CS:            return "csharp";
	case L_CSS:           return "css";
	case L_HASKELL:       return "haskell";
	case L_HTML:          return "html";
	case L_PHP:           return "php";
	case L_PYTHON:        return "python";
	case L_RUBY:          return "ruby";
	case L_XML:           return "xml";
	case L_VB:            return "visualbasic";
	case L_BASH:
	case L_BATCH:         return "cmd";
	case L_LUA:           return "lua";
	case L_CMAKE:         return "cmake";
	case L_PERL:          return "perl";
	case L_JSON:          return "json";
	case L_YAML:          return "yaml";
	case L_OBJC:          return "objectivec";
	case L_RUST:          return "rust";
	case L_LISP:          return "lisp";
	case L_R:             return "r";
	case L_SWIFT:         return "swift";
	case L_FORTRAN:       return "fortran";
	case L_ERLANG:        return "erlang";
	case L_COFFEESCRIPT:  return "coffeescript";
	default:
		if (strcmp(extension, "gitignore") == 0)
			return "git";
		break;
	}
	return nullptr;
}

static void UpdatePresence(FileData data = FileData())
{
	// Crear strings
	*rpc.details = *rpc.state = *rpc.assets.small_image = *rpc.assets.small_text = '\0';

	if (*data.name)
	{
		sprintf(rpc.details, "Editing: %s", data.name);
		if (config._show_sz)
		{
			__int64 file_size = GetFileSize(data.path);
			if (file_size != -1)
			{
				char buf[128];
				StrFormatByteSize64A(file_size, buf, sizeof(buf));
				sprintf(rpc.state, "Size: %s", buf);
			}
		}
	}

	if (*data.extension)
		sprintf(rpc.assets.large_text, "Editing a %s file", strupr(data.extension));
	else
		strcpy(rpc.assets.large_text, NPP_NAME);

	const char* large_image;
	if (*data.path && config._show_lang && 
		(large_image = GetLanguageLargeImage(strlwr(data.extension))))
	{
		strcpy(rpc.assets.large_image, large_image);
		strcpy(rpc.assets.small_image, DEFAULT_LANGIMAGE);
		strcpy(rpc.assets.small_text, NPP_NAME);
	}
	else
		strcpy(rpc.assets.large_image, DEFAULT_LANGIMAGE);

#ifdef _DEBUG
	printf("\n-> Details: %s\n-> State: %s\n-> Large text: %s\n",
		rpc.details, rpc.state, rpc.assets.large_text);
#endif // _DEBUG

	if (loop_while)
	{
		mutex.lock();
		IDiscordActivityManager* manager = core->get_activity_manager(core);
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
		mutex.unlock();
	}
}

static DWORD WINAPI RunCallBacks(LPVOID)
{
	DiscordCreateParams params;
	DiscordCreateParamsSetDefault(&params);

	params.client_id = config._client_id;
	params.flags = DiscordCreateFlags_NoRequireDiscord;

#ifdef _DEBUG
	bool reconnecting = false;
#endif // _DEBUG

reconnect:
	EDiscordResult result = DiscordCreate(DISCORD_VERSION, &params, &core);
	if (result != DiscordResult_Ok)
	{
		switch (result)
		{
		case DiscordResult_InternalError:
		case DiscordResult_NotRunning:
#ifdef _DEBUG
			printf(" > Reconnecting...\n");
			reconnecting = true;
#endif // _DEBUG

			Sleep(2000);
			goto reconnect;
		default:
			ShowDiscordError(result);
			return static_cast<DWORD>(result);
		}
	}

#ifdef _DEBUG
	mutex.lock();
	core->set_log_hook(core, DiscordLogLevel_Debug, nullptr,
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

	loop_while = true;
	SetDefaultValues(rpc);
	UpdatePresence(filedata);

	while (loop_while)
	{
		mutex.lock();
#ifdef _DEBUG
		auto result = core->run_callbacks(core);
		if (result != DiscordResult_Ok)
			printf("[RUN] - %d\n", static_cast<int>(result));
#else
		core->run_callbacks(core);
#endif // _DEBUG
		mutex.unlock();
		Sleep(1000 / 60);
	}

	return 0;
}

#define NPP_SUFFIX_LEN (sizeof(" - Notepad++") - 1)

static FileData& GetFileData(FileData& data, const TCHAR* title)
{
	ZeroMemory(&data, sizeof(FileData));
	int       start = title[0] == '*' ? 1 : 0;
	const int end = lstrlen(title) - NPP_SUFFIX_LEN;

	int i = 0;
	while (start < end)
		data.path[i++] = title[start++];
	data.path[i] = '\0';

#ifdef UNICODE
	char path[MAX_PATH * 2];
	const int cbytes = WideCharToMultiByte(CP_UTF8, 0, data.path, -1,
					   path, sizeof(path), nullptr, FALSE);
	if (cbytes == 0)
	{
		ShowLastError();
		return data;
	}
#else
	const char* path = data._path;
#endif // UNICODE

	if (*path)
	{
		strcpy(data.name, PathFindFileNameA(path));

		char* extension = PathFindExtensionA(data.name);
		if (extension && (*extension))
			memcpy(data.extension, extension + 1, lstrlenA(extension));
	}
	return data;
}

void DiscordInitPresence()
{
	if (!thread && config._enable)
	{
		thread = CreateThread(nullptr, 0, RunCallBacks, nullptr, 0, nullptr);
		if (!thread)
			ShowLastError();
	}
}

#define TITLE_BUFFER_COUNT (NPP_SUFFIX_LEN + MAX_PATH + 1)

void DiscordUpdatePresence(HWND hWnd, LPARAM lParam)
{
	if (lParam != 0)
		// When the message arrives WM_SETTEXT lParam contains the text
		UpdatePresence(GetFileData(filedata, reinterpret_cast<LPCTSTR>(lParam)));
	else if (GetWindowTextLength(hWnd) > 0)
	{
		TCHAR buffer[TITLE_BUFFER_COUNT];
		GetWindowText(hWnd, buffer, TITLE_BUFFER_COUNT);
		UpdatePresence(GetFileData(filedata, buffer));
	}
	else // Does the window have no title? 
		UpdatePresence();
}

void DiscordClosePresence()
{
	if (thread)
	{
		if (!loop_while) // The cooldown for reconnection is active 
			WaitForSingleObject(thread, 0);
		else // It waits until exiting the loop to finish the process 
		{
			loop_while = false;
			WaitForSingleObject(thread, INFINITE);
		}

		CloseHandle(thread);
		thread = nullptr;
		if (core)
		{
			auto activity_manager = core->get_activity_manager(core);
			activity_manager->clear_activity(activity_manager, nullptr, nullptr);
			core->destroy(core);
			core = nullptr;
		}

#ifdef _DEBUG
		printf(" > Presence has been closed\n");
#endif // _DEBUG
	}
}
