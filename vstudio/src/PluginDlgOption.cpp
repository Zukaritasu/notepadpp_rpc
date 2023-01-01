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

#include "PluginInterface.h"
#include "PluginDlgOption.h"
#include "PluginResources.h"
#include "PluginDiscord.h"
#include "PluginUtil.h"
#include "PluginThread.h"

#include <string>
#include <mutex>
#include <CommCtrl.h>
#include <limits.h>
#include <errno.h>
#include <tchar.h>

#include "PluginError.h"
#ifdef _DEBUG
#include <cstdio>
#endif // _DEBUG

#pragma warning(disable: 4996)

#define TEXT_BOX_EMPTY(field) \
	"The text box for " #field " cannot be empty"

extern NppData      nppData;
extern HINSTANCE    hPlugin;
extern PluginConfig config;

#ifdef __MINGW32__
#define TTS_USEVISUALSTYLE 0x100
#define TTM_SETMAXTIPWIDTH WM_USER + 24
#endif

#ifndef ARRAYSIZE
#define ARRAYSIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

static bool CreateTooltipInfo(HWND hWnd)
{
	// A tooltip is created to assign it to each component or option window
	// control that requires it
	HWND toolTip = CreateWindowEx(0, TOOLTIPS_CLASS,
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
	SetToolTip(IDC_LARGETEXT,         IDS_LARGETEXTTIP);
	SetToolTip(IDC_DOC_HELP,          IDS_DOC_HELP);
	
	return true;
}

static bool GetDiscordApplicationID(HWND hWnd, __int64& client_id)
{
	int length = GetWindowTextLength(GetDlgItem(hWnd, IDC_EDIT_CID));
	if (length >= 18)
	{
		TCHAR sclient_id[48] = { '\0' };
		GetDlgItemText(hWnd, IDC_EDIT_CID, sclient_id, 48);
		errno = 0;
		client_id = _ttoi64(sclient_id);
		if (errno == ERANGE) // the number is very large
		{
			ShowErrorMessage(_T("The application ID is a very large number. Enter a valid ID"), hWnd);
			return false;
		} else if (client_id >= MIN_CLIENT_ID)
			return true;
	}
	else if (length == 0)
	{
		// The number of digits is invalid, the default ID is assigned
		client_id = DEF_APPLICATION_ID;
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
	SetDlgItemTextW(hWnd, id, buf);
#else
	SetDlgItemTextA(hWnd, id, text);
#endif // UNICODE
}

static bool InitializeControls(HWND hWnd, const PluginConfig& pConfig, bool initDlg = true)
{
	if (initDlg && !CreateTooltipInfo(hWnd))
		return false;

	// The ID is converted to a string and if the ID is equal to the default ID,
	// the text box will be empty
	TCHAR number[48] = { '\0' };
	_stprintf(number, _T("%I64d"), pConfig._client_id);
	if (_tcscmp(number, _T(DEF_APPLICATION_ID_STR)) != 0)
	{
		SendDlgItemMessage(hWnd, IDC_EDIT_CID, WM_SETTEXT, 0, (LPARAM)number);
	}
	
	// Maximum 19 digits
	SendDlgItemMessage(hWnd, IDC_EDIT_CID, EM_SETLIMITTEXT, 19, 0);

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
		_T("%(line_count)"), _T("%(extension)"), _T("%(lang)"), _T("%(Lang)"),
		_T("%(LANG)")
	};

	for (size_t i = 0; i < ARRAYSIZE(tags); i++)
	{
		SendDlgItemMessage(hWnd, IDC_VARIABLES, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(tags[i]));
	}
	SendDlgItemMessage(hWnd, IDC_VARIABLES, CB_SETCURSEL, 0, 0);

	// The corresponding formats are shown in the text boxes
	SetControlText(hWnd, IDC_DETAILS, pConfig._details_format);
	SetControlText(hWnd, IDC_STATE, pConfig._state_format);
	SetControlText(hWnd, IDC_LARGETEXT, pConfig._large_text_format);

	SetFocus(GetDlgItem(hWnd, IDCANCEL));

	return true;
}

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

static bool GetEditTextField(HWND hWnd, char* buffer /* length 128 */,
		const char* empty, int id, const char* default_value)
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
				MessageBox(hWnd, GetRCString(IDS_ANCIICHARS), _T(""), MB_OK | MB_ICONWARNING);
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
		WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, buffer, 128, nullptr, FALSE);
#else
		strcpy(buffer, text.c_str());
#endif // UNICODE
	}

	return true;
}

extern BasicMutex mutex;
extern RichPresence rpc;

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
	if (!GetEditTextField(hDlg, copy._details_format, TEXT_BOX_EMPTY(details), IDC_DETAILS, DEF_DETAILS_FORMAT) ||
		!GetEditTextField(hDlg, copy._state_format, TEXT_BOX_EMPTY(state), IDC_STATE, DEF_STATE_FORMAT) || 
		!GetEditTextField(hDlg, copy._large_text_format, TEXT_BOX_EMPTY(large text), IDC_LARGETEXT, DEF_LARGE_TEXT_FORMAT))
	{
		return false;
	}

	if (memcmp(&copy, &config, sizeof(PluginConfig)) != 0)
	{
		__int64 oldAppID = config._client_id;
		// is locked in case Notepad++ opens a file while the configuration is being copied
		mutex.Lock();
		config = copy;
		mutex.Unlock();
		if (!copy._enable)
			rpc.Close();
		else
		{
			// If the new ID is different from the previous one, then the presence is
			// closed and a new one is created with the new ID
			if (oldAppID != client_id)
				rpc.Close();

			rpc.Update();
			rpc.Init();
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
		if (notify->idFrom == IDC_CREATEAPP || notify->idFrom == IDC_DOC_HELP)
		{
			switch (notify->code)
			{
			case NM_CLICK:
			case NM_RETURN:
				{
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
				if (MessageBox(hDlg, 
					_T("Are you sure to reset the configuration to its original state?"), 
					_T(""), MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
				{
					break;
				}
;
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
		ShowWin32LastError();
	}
}
