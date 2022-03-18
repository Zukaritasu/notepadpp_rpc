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

#include "PluginDlgOption.h"
#include "PluginDefinition.h"
#include "PluginResources.h"
#include "PluginError.h"
#include "PluginDiscord.h"

#include <CommCtrl.h>
#ifdef _DEBUG
#include <cstdio>
#endif // _DEBUG

#pragma warning(disable: 4996)

extern NppData nppData;
extern HINSTANCE hPlugin;
extern PluginConfig config;

// GetProp / SetProp
#define PROP_OPTION_DATA TEXT("OPTION_DATA")

static bool CreateTooltipInfo(HWND hDlg)
{
	HWND toolTip = CreateWindow(TOOLTIPS_CLASS,
								nullptr,
								TTS_ALWAYSTIP | TTS_NOPREFIX | TTS_USEVISUALSTYLE | WS_POPUP,
								0, 0, 0, 0,
								hDlg,
								(HMENU)0,
								nullptr,
								nullptr);
	if (toolTip == nullptr)
		return false;
	
	SendMessage(toolTip, TTM_SETTITLE, TTI_NONE, (LPARAM)TEXT("Description"));

	TOOLINFO toolInfo{};
	toolInfo.cbSize = sizeof(toolInfo);
	toolInfo.hinst  = hPlugin;
	toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	toolInfo.hwnd   = hDlg;

	auto SetToolTip = [&](unsigned idc, unsigned id_tip)
	{
		TCHAR buffer[4097]{};
		LoadString(hPlugin, id_tip, buffer, ARRAYSIZE(buffer));
		toolInfo.lpszText = buffer;
		toolInfo.uId      = reinterpret_cast<UINT_PTR>(GetDlgItem(hDlg, idc));

		SendMessage(toolTip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&toolInfo));
	};

	SetToolTip(IDC_EDIT_CID,      IDS_CLIENTID);
	SetToolTip(IDC_ENABLE,        IDS_ENABLERPC);
	SetToolTip(IDC_SHOW_FILESZ,   IDS_SHOWSIZE);
	SetToolTip(IDC_SHOW_LANGICON, IDS_SHOWLANGICON);

	return true;
}

static bool GetClientID(HWND hDlg, __int64& id)
{
	int length = GetWindowTextLength(GetDlgItem(hDlg, IDC_EDIT_CID));
	if (length == 18)
	{
		TCHAR sclient_id[20]{};
		GetDlgItemText(hDlg, IDC_EDIT_CID, sclient_id, 20);
		if ((id = _ttoi64(sclient_id)) >= MIN_CLIENT_ID)
			return true;
	}

	ShowErrorMessage(TEXT("This ID is invalid"), hDlg);
	return false;
}

static bool InitializeControls(HWND hWnd, const PluginConfig& config, bool initDlg = true)
{
	if (initDlg && !CreateTooltipInfo(hWnd))
		return false;

	TCHAR number[20]{};
	_stprintf(number, TEXT("%lld"), config._client_id);
	SendDlgItemMessage(hWnd, IDC_EDIT_CID, WM_SETTEXT, 0, (LPARAM)number);

	auto SetButtonCheck = [&hWnd](unsigned id, bool check)
	{
		SendDlgItemMessage(hWnd, id, BM_SETCHECK,
			check ? BST_CHECKED : BST_UNCHECKED, 0);
	};

	SetButtonCheck(IDC_ENABLE, config._enable);
	SetButtonCheck(IDC_SHOW_FILESZ, config._show_sz);
	SetButtonCheck(IDC_SHOW_LANGICON, config._show_lang);

	return true;
}

static bool ProcessCommand(HWND hDlg)
{
	__int64 client_id = 0;
	if (!GetClientID(hDlg, client_id))
		return false;
	auto IsButtonChecked = [&hDlg](unsigned id)
	{
		return SendDlgItemMessage(hDlg, id, BM_GETCHECK, 0, 0)
			== BST_CHECKED;
	};

	PluginConfig copy = config;

	copy._client_id = client_id;
	copy._enable    = IsButtonChecked(IDC_ENABLE);
	copy._show_sz   = IsButtonChecked(IDC_SHOW_FILESZ);
	copy._show_lang = IsButtonChecked(IDC_SHOW_LANGICON);

	if (memcmp(&copy, &config, sizeof(PluginConfig)) != 0)
	{
#ifdef _DEBUG
		printf("\n > New configuration:"
			   "\n-> client id: %lld\n-> enable: %d\n-> show file size: %d\n-> show lang icon: %d\n", 
			   copy._client_id, copy._enable, copy._show_sz, copy._show_lang);
#endif // _DEBUG
		memcpy(&config, &copy, sizeof(PluginConfig));
		if (!copy._enable)
			DiscordClosePresence();
		else
		{
			if (copy._client_id != client_id)
				DiscordClosePresence();

			DiscordUpdatePresence(nppData._nppHandle);
			DiscordInitPresence();
		}

		SavePluginConfig(copy);
	}
	return true;
}

static INT_PTR CALLBACK OptionsProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
	{
		if (!InitializeControls(hDlg, config))
			EndDialog(hDlg, FALSE);
	}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
		case IDCANCEL:
			if (LOWORD(wParam) == IDOK && !ProcessCommand(hDlg))
				break;
			EndDialog(hDlg, TRUE);
			break;
		case IDC_RESET:
		{
			PluginConfig cfg;
			LoadDefaultConfig(cfg);
			InitializeControls(hDlg, cfg, false);
			break;
		}
		default:
			break;
		}

		return TRUE;
	}
	return FALSE;
}

void ShowPluginDlgOption()
{
	if (!DialogBox(hPlugin, MAKEINTRESOURCE(IDD_PLUGIN_OPTIONS),
		nppData._nppHandle, OptionsProc))
	{
		ShowLastError();
	}
}
