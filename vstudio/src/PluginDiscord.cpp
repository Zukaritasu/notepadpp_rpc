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
#include <string>
#include <strsafe.h>

#pragma warning(disable: 4996)

#define NPP_DEFAULTIMAGE  "favicon"
#define NPP_NAME          "Notepad++"

struct FileInfo
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
	FileInfo        filedata{};
	DiscordActivity rpc{};
	IDiscordCore*   core = nullptr;
	HANDLE          thread = nullptr;
	HANDLE          monitor = nullptr;
	volatile bool   loop_while = false;
	std::mutex      sci_status;
}

extern PluginConfig config;
extern NppData      nppData;
extern std::mutex   mutex;

#define BUFFERSIZE sizeof(DiscordActivity::details)

// Return details of RPC, example: "Editing: %file [%line,%column]"
static char* GetDetails(/*128*/ char* buf, const char* format, const FileInfo& info);
static char* GetState(/*128*/ char* buf, const char* format);

static HWND GetCurrentScintilla()
{
	int which = -1;
	SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
	if (which == -1)
		return nullptr;
	return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
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
		if (strcmp(extension, ".gitignore") == 0)
			return               { "GIT", "git" };
		// In dark mode it is L_USER but it is ignored 
		if (strcmp(extension, ".md") == 0 || strcmp(extension, ".markdown") == 0)
			return               { "MARKDOWN", "markdown" };
		break;
	}
	return { "TEXT", nullptr };
}

static void UpdateRPCScintillaStatus(const FileInfo& info)
{
	sci_status.lock();
	if (*info.name)
	{
		if (!config._hide_details)
			GetDetails(rpc.details, config._details_format, info);
		if (!config._hide_state)
			GetState(rpc.state, config._state_format);
	}
	
	mutex.lock();
	if (core != nullptr) // Possible null value 
	{
		IDiscordActivityManager* manager = core->get_activity_manager(core);
		manager->update_activity(manager, &rpc, nullptr, nullptr);
	}
	mutex.unlock();
	sci_status.unlock();
}

static void UpdatePresence(FileInfo info = FileInfo())
{
	ValidateElapsedTime();
	// Clean strings
	*rpc.details = *rpc.state = *rpc.assets.small_image = *rpc.assets.small_text = '\0';

	UpdateRPCScintillaStatus(info);

	LanguageInfo lang_info{};
	
	if (*info.extension)
	{
		lang_info = GetLanguageInfo(strlwr(info.extension));
		sprintf(rpc.assets.large_text, "Editing a %s file", lang_info.name);
	}
	else
		strcpy(rpc.assets.large_text, NPP_NAME);
	
	if (*info.path && config._lang_image && lang_info.large_image)
	{
		strcpy(rpc.assets.large_image, lang_info.large_image);
		strcpy(rpc.assets.small_image, NPP_DEFAULTIMAGE);
		strcpy(rpc.assets.small_text, NPP_NAME);
	}
	else
		strcpy(rpc.assets.large_image, NPP_DEFAULTIMAGE);

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
	if (core != nullptr)
	{
		core->destroy(core);
		core = nullptr;
	}
}

static DWORD WINAPI ScintillaStatus(LPVOID)
{
	// exit using WaitForSingleObject
	while (true)
	{
		Sleep(10000);
		FileInfo info = filedata;
		UpdateRPCScintillaStatus(info);
	}
	return ERROR_SUCCESS;
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

	loop_while = true;
	UpdatePresence(filedata);
	
	while (loop_while)
	{
		mutex.lock();
		if ((result = core->run_callbacks(core)) != DiscordResult_Ok)
		{
#ifdef _DEBUG
			printf("[RUN] - %d\n", static_cast<int>(result));
#endif // _DEBUG
			loop_while = false;
			DiscordCoreDestroy();
			mutex.unlock();
			goto reconnect;
		}
		mutex.unlock();
		Sleep(1000 / 60);
	}

	return DiscordResult_Ok;
}

static FileInfo& GetFileInfo(FileInfo& info, const TCHAR* title)
{
	ZeroMemory(&info, sizeof(FileInfo));
	// The prefix indicating save changes
	int       start = title[0] == '*' ? 1 : 0;
	const int end = lstrlen(title) - (sizeof(" - Notepad++") - 1);

	int i = 0;
	while (start < end)
		info.path[i++] = title[start++];
	info.path[i] = '\0';

#ifdef UNICODE
	char path[MAX_PATH * 2];
	const int cbytes = WideCharToMultiByte(CP_UTF8, 0, info.path, -1,
					   path, sizeof(path), nullptr, FALSE);
	if (cbytes == 0)
	{
		ShowLastError();
		return info;
	}
#else
	const char* path = data._path;
#endif // UNICODE

	if (*path)
	{
		strcpy(info.name, PathFindFileNameA(path));

		char* extension = PathFindExtensionA(info.name);
		if (strlen(extension) != 0)
		{
			strncpy(info.extension, extension, MAX_PATH);
		}
	}
	return info;
}

void DiscordInitPresence()
{
	if (thread == nullptr && config._enable)
	{
		thread = CreateThread(nullptr, 0, RunCallBacks, nullptr, 0, nullptr);
		if (thread == nullptr)
			ShowLastError();
		else
		{
			monitor = CreateThread(nullptr, 0, ScintillaStatus, nullptr, 0, nullptr);
			if (monitor == nullptr)
			{
				ShowLastError();
				DiscordClosePresence();
			}
		}
	}
}

static LPTSTR GetNotepadWindowTitle(HWND hWnd)
{
	TCHAR* buffer = nullptr;
	int length = GetWindowTextLength(hWnd);
	if (length > 0)
	{
		buffer = (TCHAR*)LocalAlloc(LPTR, sizeof(TCHAR) * (++length));
		if (buffer != nullptr)
			GetWindowText(hWnd, buffer, length);
		else
			ShowLastError();
	}
	return buffer;
}

void DiscordUpdatePresence(HWND hWnd, LPARAM lParam)
{
	if (lParam != 0) // LPARAM from WM_SETTEXT
		UpdatePresence(GetFileInfo(filedata, reinterpret_cast<LPCTSTR>(lParam)));
	else
	{
		LPTSTR nppWinTitle = GetNotepadWindowTitle(hWnd);
		if (nppWinTitle != nullptr)
			UpdatePresence(); // clean rich presence
		else
		{
			UpdatePresence(GetFileInfo(filedata, nppWinTitle));
			LocalFree(nppWinTitle);
		}
	}
}

void DiscordClosePresence()
{
	if (monitor != nullptr)
	{
		sci_status.lock();
		WaitForSingleObject(monitor, 0);
		sci_status.unlock();
		CloseHandle(monitor);
	}
	if (thread != nullptr)
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
		if (core != nullptr)
		{
			auto activity_manager = core->get_activity_manager(core);
			activity_manager->clear_activity(activity_manager, nullptr, nullptr);
			core->run_callbacks(core);

			DiscordCoreDestroy();
		}
		
		// The elapsed time is reset in case of a new reconnection  
		rpc.timestamps.start = 0;
#ifdef _DEBUG
		printf(" > Presence has been closed\n");
#endif // _DEBUG
	}
}

static bool ConstainsTag(const char* format, const char* tag, size_t pos)
{
	for (size_t i = 0; tag[i] != '\0'; i++)
		if (format[pos] == '\0' || format[pos++] != tag[i])
			return false;
	return true;
}

// Returns true if the tag was found, in this case 'bpos' is incremented
// by the number of characters written
template<typename _Type>
static void ProcessFormat(char* buf, const char* tag,
	size_t& bpos, size_t& fpos, const char* type, _Type value)
{
	StringCchPrintfA(buf + bpos, BUFFERSIZE - bpos, type, value);
	bpos += strlen(buf + bpos);
	fpos += strlen(tag) - 1;
}

static char* GetDetails(char* buf, const char* format, const FileInfo& info)
{
	int line, column, line_count = 0;
	HWND curScintilla = GetCurrentScintilla();
	if (curScintilla != nullptr)
	{
		line_count = static_cast<int>(SendMessage(curScintilla, SCI_GETLINECOUNT, 0, 0));
	}
	
	line = static_cast<int>(SendMessage(nppData._nppHandle, NPPM_GETCURRENTLINE, 0, 0)) + 1;
	column = static_cast<int>(SendMessage(nppData._nppHandle, NPPM_GETCURRENTCOLUMN, 0, 0)) + 1;
	
	for (size_t i = 0, j = 0; format[i] != 0 && j < (BUFFERSIZE - 1); i++)
	{
		if (format[i] == '%' && format[i + 1] != '\0')
		{
			// format[i + 1] is '(', format[i + 2] is letter
			switch (format[i + 2])
			{
			case 'f':
				if (ConstainsTag(format, "%(file)", i))
				{
					ProcessFormat(buf, "%(file)", j, i, "%s", info.name);
					continue;
				}
				break;
			case 'l':
				if (ConstainsTag(format, "%(line_count)", i))
				{
					ProcessFormat(buf, "%(line_count)", j, i, "%d", line_count);
					continue;
				}
				else if (ConstainsTag(format, "%(line)", i))
				{
					ProcessFormat(buf, "%(line)", j, i, "%d", line);
					continue;
				}
				break;
			case 'c':
				if (ConstainsTag(format, "%(column)", i))
				{
					ProcessFormat(buf, "%(column)", j, i, "%d", column);
					continue;
				}
				break;
			case 'e':
				if (ConstainsTag(format, "%(extension)", i))
				{
					ProcessFormat(buf, "%(extension)", j, i, "%s", info.extension);
					continue;
				}
				break;
			}
		}
		
		buf[j++] = format[i];
		buf[j] = 0;
	}
	return buf;
}

static char* GetState(char* buf, const char* format)
{
	char file_size_buf[48] = { 0 };
	HWND curScintilla = GetCurrentScintilla();
	if (curScintilla != nullptr)
	{
		// The size of the file is extracted from the editor and not from the file system
		StrFormatByteSize64A(SendMessage(curScintilla, SCI_GETLENGTH, 0, 0), 
			file_size_buf, sizeof file_size_buf);
	}

	for (size_t i = 0, j = 0; format[i] != 0 && j < (BUFFERSIZE + 1); i++)
	{
		if (format[i] == '%' && format[i + 1] != '\0')
		{
			// format[i + 1] is '(', format[i + 2] is letter
			switch (format[i + 2])
			{
			case 's':
				if (ConstainsTag(format, "%(size)", i))
				{
					ProcessFormat(buf, "%(size)", j, i, "%s", file_size_buf);
					continue;
				}
				break;
			}
		}
		buf[j++] = format[i];
		buf[j] = 0;
	}
	return buf;
}
