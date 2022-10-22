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
#include "PluginResources.h"
#include "PluginConfig.h"
#include "PluginDlgOption.h"
#include "PluginDiscord.h"
#include "PluginError.h"

#include <CommCtrl.h>
#include <cstdio>

#pragma warning(disable: 4100 4996)

// ==================================================================
// TEMPLATE VARIABLES
// ==================================================================

FuncItem funcItem[nbFunc];

NppData nppData;

// ==================================================================
// PLUGIN VARIABLES
// ==================================================================


PluginConfig config{};
HINSTANCE    hPlugin = nullptr;


static LRESULT CALLBACK SubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, 
	LPARAM lParam, UINT_PTR, DWORD_PTR)
{
	switch (uMsg) 
	{
	case WM_SETTEXT:
		DiscordUpdatePresence(hWnd, lParam);
		break;
	case WM_CLOSE:
		DiscordClosePresence();
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
	LoadConfig(config);
	DiscordInitPresence();

	SetWindowSubclass(nppData._nppHandle, SubClassProc,
		ID_SUB_CLSPROC, 0);

	setCommand(0, _T("Options"), OptionsPlugin);
	setCommand(1, nullptr,       nullptr);
	setCommand(2, _T("About"),   About);
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

	printf(" > The plugin has been started\n");
#endif // _DEBUG
	hPlugin = reinterpret_cast<HINSTANCE>(handle);
}

void pluginCleanUp()
{
	DiscordClosePresence();
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
	ShowPluginDlgOption();
}
