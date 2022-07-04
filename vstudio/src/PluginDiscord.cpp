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

struct LanguageInfo
{
	const char* name;
	const char* large_image;
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

static __int64 GetFileSize(TCHAR* filename)
{
	if (*filename && PathFileExists(filename))
	{
		struct _stat64i32 stats {};
		if (_tstat(filename, &stats) == 0) return stats.st_size;
	}
	return -1;
}

// Turns elapsed time on or off depending on the 
// PluginConfig::_elapsed_time value. It is not used to reset the time!!
// because 'config._elapsed_time' might be true and prevent reset
static void ValidateElapsedTime()
{
	if (config._elapsed_time) 
	{
		if (rpc.timestamps.start == 0) 
		{
			rpc.type = DiscordActivityType_Playing;
			rpc.timestamps.start = _time64(nullptr);
		}
	} 
	else if (rpc.timestamps.start != 0)
	{
		rpc.timestamps.start = 0;
	}
}

static LanguageInfo GetLanguageInfo(const char* extension)
{
	LangType current_lang = L_TEXT;
	SendMessage(nppData._nppHandle, NPPM_GETCURRENTLANGTYPE, 0, (LPARAM)&current_lang);
#ifdef _DEBUG
	printf("\n > Current LangType: %d\n", static_cast<int>(current_lang));
#endif // _DEBUG

	switch (current_lang)
	{
	case L_JAVA:          return { "JAVA", "java" };
	case L_JAVASCRIPT: // Javascript and typescript
	case L_JS:
		if (strcmp(extension, "ts") == 0 || strcmp(extension, "tsx") == 0)
			return               { "TYPESCRIPT", "typescript" };
		return                   { "JAVASCRIPT", "javascript" };
	case L_C:             return { "C", "c" };
	case L_CPP:           return { "C++", "cpp" };
	case L_CS:            return { "C#", "csharp"};
	case L_CSS:           return { "CSS", "css" };
	case L_HASKELL:       return { "HASKELL", "haskell" };
	case L_HTML:          return { "HTML", "html" };
	case L_PHP:           return { "PHP", "php" };
	case L_PYTHON:        return { "PYTHON", "python" };
	case L_RUBY:          return { "RUBY", "ruby" };
	case L_XML:           return { "XML", "xml" };
	case L_VB:            return { "VISUALBASIC", "visualbasic" };
	case L_BASH:
	case L_BATCH:         return { "BATCH", "cmd" };
	case L_LUA:           return { "LUA", "lua" };
	case L_CMAKE:         return { "CMAKE", "cmake" };
	case L_PERL:          return { "PERL", "perl" };
	case L_JSON:          return { "JSON", "json" };
	case L_YAML:          return { "YAML", "yaml" };
	case L_OBJC:          return { "OBJECTIVE-C", "objectivec" };
	case L_RUST:          return { "RUST", "rust" };
	case L_LISP:          return { "LISP", "lisp" };
	case L_R:             return { "R", "r" };
	case L_SWIFT:         return { "SWIFT", "swift" };
	case L_FORTRAN:       return { "FORTRAN", "fortran" };
	case L_ERLANG:        return { "ERLANG", "erlang" };
	case L_COFFEESCRIPT:  return { "COFFEESCRIPT", "coffeescript" };
	case L_RC:            return { "RESOURCE", nullptr };
	default:
		if (strcmp(extension, "gitignore") == 0)
			return               { "GITIGNORE", "git" };
		// In dark mode it is L_USER but it is ignored 
		if (strcmp(extension, "md") == 0 || strcmp(extension, "markdown") == 0)
			return               { "MARKDOWN", "markdown" };
		break;
	}
	return { "TEXT", nullptr };
}

static void UpdatePresence(FileData data = FileData())
{
	ValidateElapsedTime();
	// Clean strings
	*rpc.details = *rpc.state = *rpc.assets.small_image = *rpc.assets.small_text = '\0';

	if (*data.name)
	{
		if (config._current_file)
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

	LanguageInfo lang_info{};
	
	if (*data.extension)
	{
		lang_info = GetLanguageInfo(strlwr(data.extension));
		sprintf(rpc.assets.large_text, "Editing a %s file", lang_info.name);
	}
	else
		strcpy(rpc.assets.large_text, NPP_NAME);
	
	if (*data.path && config._show_lang && lang_info.large_image)
	{
		strcpy(rpc.assets.large_image, lang_info.large_image);
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
		if (core != nullptr) // Possible null value 
		{
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
		}
		mutex.unlock();
	}
}

static void DiscordCoreDestroy()
{
	core->destroy(core);
	core = nullptr;
}

static DWORD WINAPI RunCallBacks(LPVOID)
{
	DiscordCreateParams params;
	DiscordCreateParamsSetDefault(&params);

	params.client_id = config._client_id;
	params.flags     = DiscordCreateFlags_NoRequireDiscord;

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
			reconnecting = true;
			printf(" > Reconnecting...\n");
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

	ValidateElapsedTime();
	/*// To avoid resetting the elapsed time on each reconnection 
	if (rpc.timestamps.start == 0) 
	{
		rpc.type             = DiscordActivityType_Playing;
		rpc.timestamps.start = _time64(nullptr);
	}*/
	
	loop_while = true;
	UpdatePresence(filedata);
	
	while (loop_while)
	{
		mutex.lock();
#ifdef _DEBUG
		auto result = core->run_callbacks(core);
		if (result != DiscordResult_Ok)
			printf("[RUN] - %d\n", static_cast<int>(result));
#else
		if (core->run_callbacks(core) != DiscordResult_Ok)
		{
			loop_while = false;
			DiscordCoreDestroy();
			mutex.unlock();
			goto reconnect;
		}
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
		{
			mutex.lock();
			WaitForSingleObject(thread, 0);
			mutex.unlock();
		}
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
			DiscordCoreDestroy();
		}
		
		// The elapsed time is reset in case of a new reconnection  
		rpc.timestamps.start = 0;
#ifdef _DEBUG
		printf(" > Presence has been closed\n");
#endif // _DEBUG
	}
}
