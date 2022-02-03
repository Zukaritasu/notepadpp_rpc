/*
 * Copyright (C) 2022 Zukaritasu
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "PluginDefinition.h"
#include "menuCmdID.h"

#include <discord_game_sdk.h>
#include <CommCtrl.h>
#include <time.h>
#include <tchar.h>

#pragma warning(disable: 4100)

FuncItem funcItem[nbFunc];

NppData nppData;

#define TITLE_MBOX_DRPC TEXT("Discord RPC")

static IDiscordCore* core       = NULL;
static HANDLE thread            = NULL;
static volatile bool loop_while = true;


void ShowError(DWORD code = 0);

bool DiscordError(EDiscordResult result);


static void UpdatePresence(const char* file)
{
	if (core)
	{
		static DiscordActivity rpc{};
		static bool isInitialize = false;

		if (!isInitialize) {
			rpc.type = EDiscordActivityType::DiscordActivityType_Playing;
			rpc.timestamps.start = _time64(0);

			strcpy(rpc.assets.large_image, "favicon");
			strcpy(rpc.assets.large_text, "Notepad++");

			isInitialize = true;
		}

		if (file) {
			strcpy(rpc.details, "Editing: ");
			strcat(rpc.details, file);
		} else {
			memset(rpc.details, 0, sizeof(rpc.details));
		}

		IDiscordActivityManager* manager = core->get_activity_manager(core);
		manager->update_activity(manager, &rpc, NULL, 
			[](void*, EDiscordResult result) {
			DiscordError(result);
		});
	}
}

static DWORD WINAPI RunCallBacks(LPVOID lpParam)
{
	UpdatePresence(NULL);
	while (loop_while) {
		if (DiscordError(core->run_callbacks(core)))
			break;
		Sleep(1000 / 60);
	}
	return 0;
}

static void InitPresence()
{
	DiscordCreateParams params;
	DiscordCreateParamsSetDefault(&params);

	params.client_id = 938157386068279366;
	params.flags = DiscordCreateFlags_Default;

	if (!DiscordError(DiscordCreate(DISCORD_VERSION, &params, &core))) {
		thread = CreateThread(NULL, 0, RunCallBacks, NULL, 0, NULL);
		/* Se muestra el error ocurrido en CreateThread */
		if (thread == NULL) {
			ShowError();
		}
	}
}

static void ClosePresence()
{
	if (core) {
		loop_while = false;
		if (thread)
		{
			Sleep(100);
		}
		core->destroy(core);
		core   = NULL;
		thread = NULL;
	}
}

#define NPPMNC ((int)sizeof(" - Notepad++"))

static char* GetFileName(char* out, const char* in)
{
	int length = lstrlenA(in);
	int pos = length - 1 - NPPMNC;
	
	bool delim = false;
	
	do {
		if (in[pos] == '\\' || in[pos] == '*') {
			delim = true;
			break;
		}
	} while (--pos >= 0);
	
	pos += delim || pos < 0 ? 1 : 0;
	
	int i = pos, j = 0;
	for (; i < length - NPPMNC + 1; i++) {
		out[j++] = in[i];
	}

	out[j] = '\0';
	
	return out;
}

static void ResolveTitleWindow(const TCHAR* title_win)
{
	int length = (int)lstrlen(title_win);
	if (length > NPPMNC) {
		char title[1024]{};
#ifdef UNICODE
		int mbyte_len = WideCharToMultiByte(CP_UTF8, 0, title_win, length,
						title, sizeof(title), NULL, FALSE);
		if (mbyte_len > 0) {
			UpdatePresence(GetFileName(title, title));
			return;
		}
#else
		UpdatePresence(GetFileName(title, title_win));
#endif /* UNICODE */
	}
	
	UpdatePresence(NULL);
}

LRESULT CALLBACK SubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
	LPARAM lParam, UINT_PTR, DWORD_PTR)
{
	switch (uMsg) {
		case WM_SETTEXT:
			ResolveTitleWindow((TCHAR*)lParam);
			break;
		case WM_CLOSE:
			ClosePresence();
			break;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

#define ID_SUB_CLSPROC 1560

void commandMenuInit()
{
	if (IsWindow(nppData._nppHandle)) {
		SetWindowSubclass(nppData._nppHandle, SubClassProc, 
			ID_SUB_CLSPROC, 0);
	}
	setCommand(0, TEXT("About"), About);
}

void commandMenuCleanUp()
{
	if (IsWindow(nppData._nppHandle)) {
		RemoveWindowSubclass(nppData._nppHandle, SubClassProc, 
			ID_SUB_CLSPROC);
	}
}

void pluginInit(HANDLE)
{
	InitPresence();
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

void About()
{
	::MessageBox(nppData._nppHandle,

		TEXT("Author: Zukaritasu\n\n" )
		TEXT("Copyright: (c) 2022\n\n")
		TEXT("License: GPL v3\n\n"    )
		TEXT("Version: 1.1"           ),

		TITLE_MBOX_DRPC, MB_ICONINFORMATION);
}

void ShowError(DWORD code)
{
	DWORD error = code == 0 ? GetLastError() : code;
	if (error != 0) {
		TCHAR* msg  = NULL;
		DWORD count = ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
									  FORMAT_MESSAGE_ALLOCATE_BUFFER,
									  NULL, error, 0, (TCHAR*)&msg, 0, NULL);
		if (msg != NULL && count > 0) {
			::MessageBox(nppData._nppHandle, msg, TITLE_MBOX_DRPC, MB_ICONERROR);
			LocalFree((HLOCAL)msg);
		}
	}
}

#define DISCORD_ERROR_FORMAT \
		TEXT("An error has occurred in Discord. \n\nError code: 0x000000%X")

bool DiscordError(EDiscordResult result)
{
	if (result != EDiscordResult::DiscordResult_Ok) {
		TCHAR error[260]{};
		_stprintf(error, DISCORD_ERROR_FORMAT, (int)result);
		::MessageBox(nppData._nppHandle, error, TITLE_MBOX_DRPC, MB_ICONERROR);
		return true;
	}
	return false;
}
