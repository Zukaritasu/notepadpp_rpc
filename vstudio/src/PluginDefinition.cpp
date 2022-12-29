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

#ifdef _DEBUG
#include <cstdio>
#endif // _DEBUG

#include <tchar.h>

#pragma warning(disable: 4100 4996)

FuncItem     funcItem[nbFunc];
NppData      nppData;

RichPresence rpc;
PluginConfig config{};
HINSTANCE    hPlugin = nullptr;

void commandMenuInit()
{
	LoadConfig(config);
	rpc.Init();

	setCommand(0, _T("Options"), OptionsPlugin);
	setCommand(1, nullptr,       nullptr);
	setCommand(2, _T("About"),   About);
}

void commandMenuCleanUp()
{
	rpc.Close();
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
