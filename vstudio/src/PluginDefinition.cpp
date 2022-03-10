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

#include "PluginDefinition.h"
#include "PluginError.h"
#include "PluginResources.h"
#include "PluginConfig.h"
#include "PluginDlgOption.h"

#include <discord_game_sdk.h>
#include <CommCtrl.h>
#include <tchar.h>
#include <ctime>
#include <cstdio>
#include <sys/stat.h>
#include <strsafe.h>

#include <Shlwapi.h>

#pragma warning(disable: 4100)
#pragma warning(disable: 4996)

// ==================================================================
// TEMPLATE VARIABLES
// ==================================================================

FuncItem funcItem[nbFunc];

NppData nppData;

// ==================================================================
// PLUGIN VARIABLES
// ==================================================================

struct FileData
{
	char  name[MAX_PATH];
	TCHAR path[MAX_PATH];
	char  extension[MAX_PATH];
};

namespace {
	DiscordActivity rpc{};
	IDiscordCore*   core       = nullptr;
	HANDLE          thread     = nullptr;
	volatile bool   loop_while = false;
	PluginConfig    config{};
	FileData        filedata{};
}

HINSTANCE hPlugin; // extern symbol

#define DEFAULT_LANGIMAGE "favicon"
#define NPP_NAME          "Notepad++"

static void SetDefaultValues(DiscordActivity& activity)
{
	ZeroMemory(&activity, sizeof(DiscordActivity));
	activity.type             = DiscordActivityType_Playing;
	activity.timestamps.start = _time64(nullptr);

	strcpy(activity.assets.large_image, DEFAULT_LANGIMAGE);
}

static __int64 GetFileSize(TCHAR* filename)
{
	if (filename[0] && PathFileExists(filename))
	{
		struct _stat64i32 stats{};
		if (_tstat(filename, &stats) == 0) return stats.st_size;
	}
	return -1;
}

static HWND GetScintillaHandle()
{

}

static const char* GetLanguageLargeImage()
{
	LangType current_lang = L_EXTERNAL;
	SendMessage(nppData._nppHandle, NPPM_GETCURRENTLANGTYPE, 0, (LPARAM)&current_lang);
	switch (current_lang)
	{
	case L_JAVA:    return "java";
	case L_JAVASCRIPT: // Javascript and typescript
	case L_JS:      return "javascript";
	case L_C:       return "c";
	case L_CPP:     return "cpp";
	case L_CS:      return "csharp";
	case L_CSS:     return "css";
	case L_HASKELL: return "haskell";
	case L_HTML:    return "html";
	case L_PHP:     return "php";
	case L_PYTHON:  return "python";
	case L_RUBY:    return "ruby";
	default:
		break;
	}
	return nullptr;
}

static void UpdatePresence(FileData data = FileData())
{
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
	if (config._show_lang && (large_image = GetLanguageLargeImage()))
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

	if (core)
	{
		IDiscordActivityManager* manager = core->get_activity_manager(core);
		manager->update_activity(manager, &rpc, nullptr,
#ifdef _DEBUG
		[](void*, EDiscordResult result) {
			if (result != DiscordResult_Ok)
			printf("[RPC] Update presence | %d", static_cast<int>(result));
		}
#else
		nullptr
#endif // _DEBUG
		);
	}
}

static DWORD WINAPI RunCallBacks(LPVOID config)
{
	DiscordCreateParams params;
	DiscordCreateParamsSetDefault(&params);

	params.client_id = ((PluginConfig*)config)->_client_id;
	params.flags     = DiscordCreateFlags_NoRequireDiscord;

	delete config;
	
#ifdef _DEBUG
	bool reconnecting = false;
#endif // _DEBUG

reconnect:
	EDiscordResult result = DiscordCreate(DISCORD_VERSION, &params, &core);
	if (result == DiscordResult_InternalError || 
		result == DiscordResult_NotRunning)
	{
#ifdef _DEBUG
		printf(" > Reconnecting...");
		reconnecting = true;
#endif // _DEBUG

		Sleep(20000);
		goto reconnect;
	}

#ifdef _DEBUG
	core->set_log_hook(core, DiscordLogLevel_Debug, nullptr,
		[](void*, EDiscordLogLevel level, const char* message)
	{
		printf("[LOG] Level: %d | Message: %s", static_cast<int>(level), message);
	});
#endif // _DEBUG

#ifdef _DEBUG
	if (reconnecting)
		printf(" > Successful reconnection!");
#endif // _DEBUG

	loop_while = true;
	SetDefaultValues(rpc);
	UpdatePresence(filedata);

	while (loop_while) 
	{
#ifdef _DEBUG
		auto result = core->run_callbacks(core);
		if (result != DiscordResult_Ok)
			printf("[RUN] > Error code: %d", static_cast<int>(result));
#else
		core->run_callbacks(core);
#endif // _DEBUG
		Sleep(1000 / 60);
	}

	return 0;
}

static void ClosePresence()
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
			// Ignore EDiscordResult
			activity_manager->clear_activity(activity_manager, nullptr, nullptr);
			core->destroy(core);
			core = nullptr;
		}

#ifdef _DEBUG
		printf(" > Presence has been closed\n");
#endif // _DEBUG
	}
}

static void InitPresence(const PluginConfig& config)
{
	if (core == nullptr && config._enable) {
		thread = CreateThread(nullptr, 0, RunCallBacks, 
							  new PluginConfig(config), 0, nullptr);
		if (!thread)
		{
			ShowLastError();
		}
	}
}

#define NPP_SUFFIX_LEN (sizeof(" - Notepad++") - 1)

static FileData& GetFileData(FileData& data, const TCHAR* title)
{
	ZeroMemory(&data, sizeof(FileData));
	int       start = title[0] == '*' ? 1 : 0;
	const int end   = lstrlen(title) - NPP_SUFFIX_LEN;

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

	if (!(*path)) return data;

	strcpy(data.name, PathFindFileNameA(path));

	char* extension = PathFindExtensionA(data.name);
	if (extension && (*extension))
		memcpy(data.extension, extension + 1, lstrlenA(extension));
	return data;
}

static void UpdatePresenceFromWindowTitle(HWND hWnd, LPARAM lParam = 0)
{
	if (lParam != 0) 
		// When the message arrives WM_SETTEXT lParam contains the text
		UpdatePresence(GetFileData(filedata, reinterpret_cast<LPCTSTR>(lParam)));
	else
	{
		int length = GetWindowTextLength(hWnd);
		if (length > 0)
		{
			DWORD num_bytes = sizeof(TCHAR) * (length + 1);
			TCHAR* buffer   = (TCHAR*)VirtualAlloc(nullptr, num_bytes, 
							  MEM_COMMIT, PAGE_READWRITE);
			if (buffer)
			{
				GetWindowText(hWnd, buffer, length + 1);
				UpdatePresence(GetFileData(filedata, buffer));
				VirtualFree((void*)buffer, num_bytes, MEM_RELEASE);
				return;
			}
		}
		
		// GetWindowTextLength or VirtualAlloc
		ShowLastError();
		UpdatePresence();
	}
}

static LRESULT CALLBACK SubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, 
	LPARAM lParam, UINT_PTR, DWORD_PTR)
{
	switch (uMsg) {
	case WM_SETTEXT:
		if (loop_while) // Do not update presence 
			UpdatePresenceFromWindowTitle(hWnd, lParam);
		break;
	case WM_CLOSE:
		ClosePresence();
		break;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// ==================================================================
// IMPLEMENTATION OF TEMPLATE FUNCTIONS 
// ==================================================================

#define ID_SUB_CLSPROC 1560

void commandMenuInit()
{
	SetWindowSubclass(nppData._nppHandle, SubClassProc,
		ID_SUB_CLSPROC, 0);

	setCommand(0, TEXT("Options"), OptionsPlugin);
	setCommand(1, nullptr, nullptr);
	setCommand(2, TEXT("About"),   About);
}

void commandMenuCleanUp()
{
	RemoveWindowSubclass(nppData._nppHandle, SubClassProc,
		ID_SUB_CLSPROC);
}

void pluginInit(HANDLE handle)
{
#ifdef _DEBUG // Console is enabled for debugging 
	AllocConsole();
	freopen("CONOUT$", "w", stdout);

	printf(" > Plugin init\n");
#endif // _DEBUG

	hPlugin = reinterpret_cast<HINSTANCE>(handle);
	LoadPluginConfig(config);
	InitPresence(config);
}

void pluginCleanUp()
{
	ClosePresence();
}

bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc,
				ShortcutKey *sk, bool check0nInit)
{
    if (index >= nbFunc)
        return false;

    if (!pFunc)
        return false;

    lstrcpy(funcItem[index]._itemName, cmdName);
    funcItem[index]._pFunc = pFunc;
    funcItem[index]._init2Check = check0nInit;
    funcItem[index]._pShKey = sk;

    return true;
}


// ==================================================================
// IMPLEMENTATION OF PLUGIN FUNCTIONS 
// ==================================================================

void About()
{
	MessageBox(nppData._nppHandle, PLUGIN_ABOUT, TITLE_MBOX_DRPC,
			   MB_ICONINFORMATION | MB_OK);
}

void OptionsPlugin()
{
	DlgOptionData data;
	data._config = &config;
	data._close  = ClosePresence;
	data._init   = InitPresence;
	data._update = UpdatePresenceFromWindowTitle;

	ShowPluginDlgOption(data);
}
