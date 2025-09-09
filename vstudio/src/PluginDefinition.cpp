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
#include "PluginDlgOption.h"
#include "PluginDiscord.h"
#include "PluginConfig.h"

#ifdef _DEBUG
#include <cstdio>
#endif // _DEBUG

#include <tchar.h>
#include <shlwapi.h>
#include <commctrl.h>
#pragma comment(lib, "Comctl32.lib")


#pragma warning(disable: 4100 4996)

FuncItem     funcItem[nbFunc];
NppData      nppData;

RichPresence rpc;
PluginConfig config{};
HINSTANCE    hPlugin = nullptr;

// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ //

LRESULT CALLBACK SubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	switch (message) {
	case WM_CLOSE:
		rpc.Close();
	default:
		return DefSubclassProc(hwnd, message, wParam, lParam);
	}
	return 0;
}

void commandMenuInit()
{
	LoadConfig(config);
	rpc.Init();

	setCommand(0, _T("Options"), OptionsPlugin);
	setCommand(1, nullptr,       nullptr);
	setCommand(2, _T("Edit configuration file"), OpenConfigurationFile);
	setCommand(3, _T("About"),   About);

	SetWindowSubclass(nppData._nppHandle, SubclassProc, 29, 0);
}

void commandMenuCleanUp()
{
}

void pluginInit(HANDLE handle)
{
#if defined(_DEBUG) || NPP_ENABLE_CONSOLE // Console is enabled for debugging 
	AllocConsole();
	freopen("CONOUT$", "w", stdout);

	printf(" > The plugin has been started\n");
#endif // _DEBUG
	hPlugin = reinterpret_cast<HINSTANCE>(handle);
}

void pluginCleanUp() {}

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
	MessageBox(nppData._nppHandle, PLUGIN_ABOUT, _T(TITLE_MBOX_DRPC),
			   MB_ICONINFORMATION | MB_OK);
}

void ScintillaNotify(SCNotification* notifyCode)
{
	switch (notifyCode->nmhdr.code)
	{
	case NPPN_BUFFERACTIVATED:
	case NPPN_FILERENAMED:
	case NPPN_LANGCHANGED:
		rpc.Update();
		break;
	default:
		break;
	}
}

void OptionsPlugin()
{
	ShowPluginDlgOption();
}

void OpenConfigurationFile()
{
	TCHAR strBuffer[MAX_PATH]{};
	SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)strBuffer);
	_tcsncat(strBuffer, _T("\\" PLUGIN_CONFIG_FILENAME), MAX_PATH - lstrlen(strBuffer));
	if (strBuffer[0] != 0)
	{
		if (!PathFileExists(strBuffer))
			SaveConfig(config);
		if (PathFileExists(strBuffer))
			SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)strBuffer);
	}
}
