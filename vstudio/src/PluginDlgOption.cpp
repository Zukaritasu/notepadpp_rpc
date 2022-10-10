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
#include "PluginUtil.h"

#include <string>
#include <CommCtrl.h>
#ifdef _DEBUG
#include <cstdio>
#endif // _DEBUG

#pragma warning(disable: 4996)

extern NppData nppData;
extern HINSTANCE hPlugin;
extern PluginConfig config;

static bool CreateTooltipInfo(HWND hWnd)
{
	// A tooltip is created to assign it to each component or option window
	// control that requires it
	HWND toolTip = CreateWindow(TOOLTIPS_CLASS,
								nullptr,
								TTS_ALWAYSTIP | TTS_NOPREFIX | TTS_USEVISUALSTYLE | WS_POPUP,
								0, 0, 0, 0,
								hWnd,
								(HMENU)0,
								nullptr,
								nullptr);
	if (toolTip == nullptr)
		return false;	// Failed to create tooltip
	
	TOOLINFO toolInfo{};
	toolInfo.cbSize = sizeof(toolInfo);
	toolInfo.hinst  = hPlugin;
	toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	toolInfo.hwnd   = hWnd;
	
	SendMessage(toolTip, TTM_SETMAXTIPWIDTH, 0, 340);

	auto SetToolTip = [&](unsigned idc, unsigned id_tip)
	{
		toolInfo.lpszText = GetRCString(id_tip);
		toolInfo.uId      = reinterpret_cast<UINT_PTR>(GetDlgItem(hWnd, idc));

		SendMessage(toolTip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&toolInfo));
	};

	// Its respective tool tip is established in each control of the options window.
	SetToolTip(IDC_EDIT_CID,          IDS_CLIENTID);
	SetToolTip(IDC_ENABLE,            IDS_ENABLERPC);
	SetToolTip(IDC_HIDE_STATE,        IDS_HIDE_STATE);
	SetToolTip(IDC_SHOW_LANGICON,     IDS_SHOWLANGICON);
	SetToolTip(IDC_SHOW_ELAPSED_TIME, IDS_SHOWELAPSEDTIME);
	SetToolTip(IDC_HIDE_DETAILS,      IDS_HIDE_DETAILS);
	SetToolTip(IDC_RESET,             IDS_BTNRESET);
	SetToolTip(IDC_VARIABLES,         IDS_ALLVARIABLESTIP);
	SetToolTip(IDC_DETAILS,           IDS_DETAILSTIP);
	SetToolTip(IDC_STATE,             IDS_STATETIP);
	SetToolTip(IDC_CREATEAPP,         IDS_CREATEAPPTIP);
	
	return true;
}

static bool GetDiscordApplicationID(HWND hWnd, __int64& id)
{
	int length = GetWindowTextLength(GetDlgItem(hWnd, IDC_EDIT_CID));
	// The number of digits of the ID must be 18
	if (length == 18)
	{
		TCHAR sclient_id[20]{};
		GetDlgItemText(hWnd, IDC_EDIT_CID, sclient_id, 20);
		if ((id = _ttoi64(sclient_id)) >= 1E17)
		{
			return true;
		}
	}
	else if (length == 0)
	{
		// The number of digits is invalid, the default ID is assigned
		id = DEF_APPLICATION_ID;
		return true;
	}

	ShowErrorMessage(GetRCString(IDS_INVALIDID), hWnd);
	return false;
}

static void SetControlText(HWND hWnd, unsigned id, const char* text)
{
#ifdef UNICODE
	wchar_t buf[128] = { 0 };
	MultiByteToWideChar(CP_UTF8, 0, text, -1, buf, 128);
	SetDlgItemText(hWnd, id, buf);
#else
	SetDlgItemText(hWnd, id, text);
#endif // UNICODE
}

static bool InitializeControls(HWND hWnd, const PluginConfig& pConfig, bool initDlg = true)
{
	if (initDlg && !CreateTooltipInfo(hWnd))
		return false;

	// The ID is converted to a string and if the ID is equal to the default ID,
	// the text box will be empty
#ifdef UNICODE
	std::wstring number = std::to_wstring(pConfig._client_id);
#else
	std::string number = std::to_string(pConfig._client_id);
#endif // UNICODE
	if (number != _T(DEF_APPLICATION_ID_STR))
		SendDlgItemMessage(hWnd, IDC_EDIT_CID, WM_SETTEXT, 0, (LPARAM)number.c_str());
	
	// Maximum 18 digits
	SendDlgItemMessage(hWnd, IDC_EDIT_CID, EM_SETLIMITTEXT, 18, 0);

	auto SetButtonCheck = [&hWnd](unsigned id, bool check)
	{
		SendDlgItemMessage(hWnd, id, BM_SETCHECK,
			check ? BST_CHECKED : BST_UNCHECKED, 0);
	};

	SetButtonCheck(IDC_ENABLE,            pConfig._enable);
	SetButtonCheck(IDC_HIDE_STATE,        pConfig._hide_state);
	SetButtonCheck(IDC_SHOW_LANGICON,     pConfig._lang_image);
	SetButtonCheck(IDC_SHOW_ELAPSED_TIME, pConfig._elapsed_time);
	SetButtonCheck(IDC_HIDE_DETAILS,      pConfig._hide_details);

	// The available tags that will be displayed in the combobox
	TCHAR* tags[] = 
	{ 
		_T("%(file)"), _T("%(line)"), _T("%(column)"), _T("%(size)"),
		_T("%(line_count)"), _T("%(extension)")
	};

	for (size_t i = 0; i < ARRAYSIZE(tags); i++)
	{
		SendDlgItemMessage(hWnd, IDC_VARIABLES, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(tags[i]));
	}
	SendDlgItemMessage(hWnd, IDC_VARIABLES, CB_SETCURSEL, 0, 0);

	// The corresponding formats are shown in the text boxes
	SetControlText(hWnd, IDC_DETAILS, pConfig._details_format);
	SetControlText(hWnd, IDC_STATE, pConfig._state_format);
	
	return true;
}

#define ERR_FORMAT_LONG(field) \
	"The format length of the " #field " text field is too long, it must have a maximum of 127 characters"

// Code copied from https://www.techiedelight.com/trim-string-cpp-remove-leading-trailing-spaces/
// and modified for use in this source code
static std::basic_string<TCHAR> StringTrim(const std::basic_string<TCHAR>& s)
{
    auto start = s.begin();
    while (start != s.end() && isspace(*start)) 
	{
        start++;
    }
 
    auto end = s.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && isspace(*end));
 
    return std::basic_string<TCHAR>(start, end + 1);
}

static bool GetEditTextField(HWND hWnd, char* buffer, const char* empty, int id, const char* default_value)
{
	int count = GetWindowTextLength(GetDlgItem(hWnd, id));
	if (count == 0)
		strcpy(buffer, default_value);
	else
	{
		std::basic_string<TCHAR> text(++count, _T('\0'));
		GetDlgItemText(hWnd, id, &text[0], count);
		text.resize(count - 1);
#ifdef UNICODE
		for (auto c : text)
		{
			if (c > 255)
			{
				MessageBox(hWnd, GetRCString(IDS_ANCIICHARS), L"", MB_OK | MB_ICONWARNING);
				return false;
			}
		}
#endif // UNICODE
		if ((text = StringTrim(text)).empty())
		{
			MessageBoxA(hWnd, empty, "", MB_OK | MB_ICONWARNING);
			return false;
		}
#ifdef UNICODE
		WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, buffer, 128, nullptr, false);
#else
		strcpy(buffer, text.c_str());
#endif // UNICODE
	}

	return true;
}

static bool ProcessCommand(HWND hDlg)
{
	__int64 client_id = 0;
	if (!GetDiscordApplicationID(hDlg, client_id))
		return false;
	auto IsButtonChecked = [&hDlg](unsigned id) -> bool
	{
		return SendDlgItemMessage(hDlg, id, BM_GETCHECK, 0, 0)
			== BST_CHECKED;
	};
	
	PluginConfig copy;

	copy._client_id    = client_id;
	copy._enable       = IsButtonChecked(IDC_ENABLE);
	copy._hide_state   = IsButtonChecked(IDC_HIDE_STATE);
	copy._lang_image   = IsButtonChecked(IDC_SHOW_LANGICON);
	copy._elapsed_time = IsButtonChecked(IDC_SHOW_ELAPSED_TIME);
	copy._hide_details = IsButtonChecked(IDC_HIDE_DETAILS);

	// The new formats are obtained but first they are validated
	if (!GetEditTextField(hDlg, copy._details_format, GetRCStringA(IDS_DETAILSEMPTY), IDC_DETAILS, "Editing: %file") || 
		!GetEditTextField(hDlg, copy._state_format, GetRCStringA(IDS_STATEEMPTY), IDC_STATE, "Size: %size"))
	{
		return false;
	}

	if (memcmp(&copy, &config, sizeof(PluginConfig)) != 0)
	{
		__int64 oldAppID = config._client_id;
		memcpy(&config, &copy, sizeof(PluginConfig));
		if (!copy._enable)
			DiscordClosePresence();
		else
		{
			// If the new ID is different from the previous one, then the presence is
			// closed and a new one is created with the new ID
			if (oldAppID != client_id)
				DiscordClosePresence();

			DiscordUpdatePresence(nppData._nppHandle);
			DiscordInitPresence();
		}

		SaveConfig(copy);
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
		return TRUE;
	}
	case WM_NOTIFY:
	{
		LPNMHDR notify = reinterpret_cast<LPNMHDR>(lParam);
		if (notify->idFrom == IDC_CREATEAPP)
		{
			switch (notify->code)
			{
			case NM_CLICK:
			case NM_RETURN:
				{
					// The page to create a new discord application opens
					NMLINK* link = (NMLINK*)notify;
					ShellExecute(NULL, L"open", link->item.szUrl, NULL, NULL, SW_SHOW);
					return TRUE;
				}
			}
		}
		break;
	}
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
				// The default configuration is assigned
				PluginConfig cfg;
				GetDefaultConfig(cfg);
				InitializeControls(hDlg, cfg, false);
				break;
			}
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


