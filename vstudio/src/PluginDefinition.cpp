// Copyright (C) 2022-2026 Zukaritasu
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
#include "PluginUtil.h"
#include "TextEditorInfo.h"
#include <vector>
#include <mutex>
#include <string>
#include <atomic>

#ifdef _DEBUG
#include <cstdio>
#endif // _DEBUG

#include <tchar.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <algorithm>
#pragma comment(lib, "Comctl32.lib")

#pragma warning(disable : 4100 4996)

////////////////////////////////////////////

FuncItem funcItem[nbFunc];
NppData nppData;

RichPresence rpc;
ConfigManager configManager;
HINSTANCE hPlugin = nullptr;

static std::mutex g_errorMutex;
static std::string g_errorMessage;
static std::atomic<bool> g_hasError{false};

void QueueErrorMessage(const std::string &msg) noexcept
{
	try
	{
		std::lock_guard<std::mutex> lock(g_errorMutex);
		g_errorMessage = msg;
		g_hasError.store(true, std::memory_order_release);
	}
	catch (...) { /* swallow - best effort */ }
}

void ShowQueuedErrorIfAny() noexcept
{
	if (!g_hasError.load(std::memory_order_acquire))
		return;

	std::string msg;
	{
		std::lock_guard<std::mutex> lock(g_errorMutex);
		msg.swap(g_errorMessage);
		g_hasError.store(false, std::memory_order_release);
	}

	if (!msg.empty())
	{
		::MessageBoxA(nppData._nppHandle, msg.c_str(), "Discord Rich Presence Error",
					  MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
	}
}

///////////////////////////////////////////

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData)
{
	nppData = notpadPlusData;
	
	configManager.LoadConfig();
	
	setCommand(0, L"Options", OpenPluginOptionsDialog);
	setCommand(1, nullptr, nullptr);
	setCommand(2, L"Edit configuration file", OpenConfigurationFile);
	setCommand(3, L"About", About);

	rpc.InitializePresence();
}

extern "C" __declspec(dllexport) const TCHAR *getName()
{
	return NPP_PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem *getFuncsArray(int *nbF)
{
	*nbF = nbFunc;
	return funcItem;
}

bool setCommand(size_t index, const TCHAR *cmdName, PFUNCPLUGINCMD pFunc,
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

extern "C" __declspec(dllexport) void beNotified(SCNotification *notifyCode)
{
	ShowQueuedErrorIfAny();

	switch (notifyCode->nmhdr.code)
	{
	case NPPN_BUFFERACTIVATED:
	case NPPN_FILERENAMED:
	case NPPN_LANGCHANGED:
	case SCN_UPDATEUI:
		rpc.Update();
		break;
	case NPPN_SHUTDOWN:
		rpc.Close();
		break;
	default:
		break;
	}
}

extern "C" __declspec(dllexport) BOOL isUnicode()
{
	return TRUE;
}

extern "C" __declspec(dllexport) LRESULT messageProc(
	UINT /*message*/,
	WPARAM /*wParam*/,
	LPARAM /*lParam*/)
{
	return TRUE;
}

////////////////////////////////////////////

void OpenPluginOptionsDialog()
{
	ShowPluginDlgOption();
}

void OpenConfigurationFile()
{
	auto configDir = TextEditorInfo::GetEditorTextPropertyW(NPPM_GETPLUGINSCONFIGDIR);
	if (configDir.empty())
		return;
	
	configDir.append(L"\\").append(_T(PLUGIN_CONFIG_FILENAME));

	if (!PathFileExists(configDir.c_str()))
		if (!configManager.SaveConfig()) return;
	NppSendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)configDir.c_str());
}

////////////////////////////////////////////

BOOL APIENTRY DllMain(HANDLE hModule, DWORD reasonForCall, LPVOID)
{
	try
	{
		switch (reasonForCall)
		{
		case DLL_PROCESS_ATTACH:
		{
#if defined(_DEBUG) || NPP_ENABLE_CONSOLE
			AllocConsole();

			FILE *fp = freopen("CONOUT$", "w", stdout);
			if (!fp)
				fprintf(stderr, "Error redirecting stdout to the console.\n");
			printf(" > The plugin has been started\n");
#endif // _DEBUG
			hPlugin = reinterpret_cast<HINSTANCE>(hModule);
		}
		break;

		case DLL_PROCESS_DETACH:
			break;
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
			break;
		}
	}
	catch (...)
	{
		return FALSE;
	}

	return TRUE;
}
