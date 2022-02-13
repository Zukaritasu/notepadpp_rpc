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
#include "PluginResources.h"

#include <discord_game_sdk.h>
#include <CommCtrl.h>
#include <tchar.h>
#include <ctime>
#include <cstdio>
#include <sys/stat.h>

#include <Shlwapi.h>

#pragma warning(disable: 4100)

/*==============================================================*/

FuncItem funcItem[nbFunc];

NppData nppData;

/*==============================================================*/

#define TITLE_MBOX_DRPC   TEXT("Discord RPC")

#define DEFAULT_CLIENT_ID 938157386068279366

#define DEF_CLIENT_ID_STR "938157386068279366"

#define MIN_CLIENT_ID     100000000000000000

/*==============================================================*/

static IDiscordCore* core       = NULL;
static DiscordCreateParams params{};
static DiscordActivity rpc;

static HANDLE plugin            = NULL;
static HANDLE thread            = NULL;

static volatile bool loop_while;

/*==============================================================*/

struct Config
{
	__int64 client_id;
	bool    enable;
	bool    show_sz;
};

struct FileData
{
	char fl_name[MAX_PATH];
	char ab_path[MAX_PATH];
	char fl_extn[MAX_PATH];
};

static Config config;
static FileData gl_fdata{};

/*==============================================================*/

static void ClosePresence   ();
static void ShowErrorMessage(LPCTSTR msg, HWND hWnd = NULL);
static void ShowError       (DWORD code = 0);
static bool DiscordError    (EDiscordResult result);
static void SetDefaultConfig(Config& config);
static void SaveConfig      (const Config& config);
static void LoadConfig      (Config& config);

/*==============================================================*/

static void SetDefaultRPC(DiscordActivity& _rpc)
{
	memset(&_rpc, 0, sizeof(DiscordActivity));
	_rpc.type             = DiscordActivityType_Playing;
	_rpc.timestamps.start = _time64(NULL);

	strcpy(_rpc.assets.large_image, "favicon");
}

static const char* GetBytesSuffix(__int64 size)
{
	if ((size /= 1000) <= 0)
		return "bytes";
	if ((size /= 1000) <= 0)
		return "KB";
	if ((size /= 1000) <= 0)
		return "MB";
	if ((size /= 1000) <= 0)
		return "GB";
	return "TB";
}

static void GetSizeFormat(char* buf, __int64 size)
{
	char number[20]{};
	sprintf(number, "%lld", size);

	if (size >= 1000)
	{
		if ((size >= 1E3  && size < 1E4 ) || /* KB */
			(size >= 1E6  && size < 1E7 ) || /* MB */
			(size >= 1E9  && size < 1E10) || /* GB */
			(size >= 1E12 && size < 1E13)) { /* TB */
			number[4] = '\0';
			number[3] = number[2];
			number[2] = number[1];
			number[1] = ',';  /* 0,00 */
		} else if (
			(size >= 1E4  && size < 1E5 ) || /* KB */
			(size >= 1E7  && size < 1E8 ) || /* MB */
			(size >= 1E10 && size < 1E11) || /* GB */
			(size >= 1E13 && size < 1E14)) { /* TB */
			number[4] = '\0';
			number[3] = number[2];
			number[2] = ',';  /* 00,0 */
		} else {
			number[3] = '\0'; /* 569000KB to 569KB */
		}
	}

	sprintf(buf, "Size: %s %s", number, GetBytesSuffix(size));
}

static void UpdatePresence(FileData fl_data = FileData())
{
	rpc.details[0] = rpc.state[0] = '\0';

	if (fl_data.fl_name[0] != '\0') {
		sprintf(rpc.details, "Editing: %s", fl_data.fl_name);
		if (config.show_sz && PathFileExistsA(fl_data.ab_path)) {
			struct _stat64i32 fl_stat;
			_stat(fl_data.ab_path, &fl_stat);
			GetSizeFormat(rpc.state, fl_stat.st_size);
		}
	}
	if (fl_data.fl_extn[0] != '\0')
	{
		strupr(fl_data.fl_extn);
		sprintf_s(rpc.assets.large_text, "Editing a %s file", fl_data.fl_extn);
	}
	else
	{
		strcpy(rpc.assets.large_text, "Notepad++");
	}
	if (core)
	{
		IDiscordActivityManager* manager = core->get_activity_manager(core);
		manager->update_activity(manager, &rpc, NULL,
			[](void*, EDiscordResult result) {
			DiscordError(result);
		});
	}
}

static DWORD WINAPI RunCallBacks(LPVOID)
{
	loop_while = true;
	SetDefaultRPC(rpc);
	UpdatePresence(gl_fdata);
	while (loop_while) {
		if (DiscordError(core->run_callbacks(core))) {
			ClosePresence();
			break;
		}
		Sleep(1000 / 60);
	}
	loop_while = true;
	return 0;
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

static void InitPresence(const Config& _config)
{
	if (core == NULL && _config.enable) {
		DiscordCreateParamsSetDefault(&params);

		params.client_id = _config.client_id;
		params.flags     = DiscordCreateFlags_Default;

		if (!DiscordError(DiscordCreate(DISCORD_VERSION, &params, &core))) {
			thread = CreateThread(NULL, 0, RunCallBacks, NULL, 0, NULL);
			ShowError();
			if (thread == NULL) {
				ClosePresence();
			}
		}
	}
}

#define NPP_SUFFIX_LEN (sizeof(" - Notepad++") - 1)

static void GetFileData(FileData& fl_data, const char* title)
{
	fl_data.ab_path[0] =
	fl_data.fl_extn[0] =
	fl_data.fl_name[0] = '\0'; 

	const int start = title[0] == '*' ? 1 : 0;
	const int end   = lstrlenA(title) - NPP_SUFFIX_LEN;
	int j = 0;

	for (int i = start; i < end; i++)
		fl_data.ab_path[j++] = title[i];
	fl_data.ab_path[j] = '\0';

	lstrcpyA(fl_data.fl_name, PathFindFileNameA(fl_data.ab_path));

	const char* ext = PathFindExtensionA(fl_data.fl_name);
	if (ext != NULL && ext[0] != '\0') {
		memcpy(fl_data.fl_extn, ext + 1, lstrlenA(ext));
	}
}

static void UpdatePresenceByTitleWindowA(const char* title_win)
{
	int length = lstrlenA(title_win);
	if (length > NPP_SUFFIX_LEN) {
		GetFileData(gl_fdata, title_win);
		UpdatePresence(gl_fdata);
	} else {
		UpdatePresence();
	}
}

static void UpdatePresenceByTitleWindowW(const wchar_t* title_win)
{
	int length = lstrlenW(title_win);
	if (length > NPP_SUFFIX_LEN) {
		char title[MAX_PATH * 2]{};
		WideCharToMultiByte(CP_UTF8, 0, title_win, length,
			title, sizeof(title), NULL, FALSE);
		GetFileData(gl_fdata, title);
		UpdatePresence(gl_fdata);
	} else {
		UpdatePresence();
	}
}

static void UpdatePresenceByTitleWindow(HWND hWnd, LPARAM lParam = 0)
{
	static const BOOL isUnicode = IsWindowUnicode(hWnd);
	void* title = reinterpret_cast<void*>(lParam);
	if (title == NULL)
	{
		if (isUnicode) {
			wchar_t title_w[1024]{};
			GetWindowTextW(hWnd, title_w, ARRAYSIZE(title_w));
			title = reinterpret_cast<void*>(title_w);
		} else {
			char title_c[1024]{};
			GetWindowTextA(hWnd, title_c, ARRAYSIZE(title_c));
			title = reinterpret_cast<void*>(title_c);
		}
	}

	if (isUnicode)
		UpdatePresenceByTitleWindowW(
			reinterpret_cast<wchar_t*>(title));
	else
		UpdatePresenceByTitleWindowA(
			reinterpret_cast<char*>(title));
}

LRESULT CALLBACK SubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
							  LPARAM lParam, UINT_PTR, DWORD_PTR)
{
	switch (uMsg) {
	case WM_SETTEXT:
		UpdatePresenceByTitleWindow(hWnd, lParam);
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
	SetWindowSubclass(nppData._nppHandle, SubClassProc,
		ID_SUB_CLSPROC, 0);

	setCommand(0, TEXT("Options"), OptionsPlugin);
	setCommand(1, NULL, NULL);
	setCommand(2, TEXT("About"),   About);
}

void commandMenuCleanUp()
{
	RemoveWindowSubclass(nppData._nppHandle, SubClassProc,
		ID_SUB_CLSPROC);
}

void pluginInit(HANDLE handle)
{
	plugin = handle;
	LoadConfig(config);
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

#define TEXT_ABOUT                \
	_T("Author: Zukaritasu\n\n" ) \
	_T("Copyright: (c) 2022\n\n") \
	_T("License: GPL v3\n\n"    ) \
	_T("Version: 1.2.98.1"      )


void About()
{
	MessageBox(nppData._nppHandle, TEXT_ABOUT,
		TITLE_MBOX_DRPC, MB_ICONINFORMATION);
}

static void ShowErrorMessage(LPCTSTR msg, HWND hWnd)
{
	MessageBox(hWnd ? hWnd : nppData._nppHandle, msg,
			   TITLE_MBOX_DRPC, MB_ICONERROR);
}

#define GetErrorMessage(pmsg, code) \
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | \
				  FORMAT_MESSAGE_ALLOCATE_BUFFER, \
				  NULL, code, 0, reinterpret_cast<TCHAR*>(pmsg), 0, NULL)

static void ShowError(DWORD code)
{
	DWORD error = code == 0 ? GetLastError() : code;
	if (error != 0)
	{
		TCHAR* msg  = NULL;
		DWORD count = GetErrorMessage(&msg, error);
		if (msg != NULL && count > 0) {
			ShowErrorMessage(msg);
			LocalFree(msg);
		}
	}
}

#define DISCORD_ERROR_FORMAT \
	_T("An error has occurred in Discord. Error code: 0x000000%X")

#ifdef UNICODE
#	define BUF_E_SIZE (sizeof(DISCORD_ERROR_FORMAT) / 2)
#else
#	define BUF_E_SIZE (sizeof(DISCORD_ERROR_FORMAT))
#endif /* UNICODE */

static bool DiscordError(EDiscordResult result)
{
	if (result != DiscordResult_Ok)
	{
		TCHAR error[BUF_E_SIZE]{};
		_stprintf(error, DISCORD_ERROR_FORMAT, (int)result);
		ShowErrorMessage(error);
		return true;
	}
	return false;
}

static void SetDefaultConfig(Config& _config)
{
	_config.client_id = DEFAULT_CLIENT_ID;
	_config.enable    = true;
	_config.show_sz   = true;
}

static void GetDirectoryDiscordRPC(TCHAR* fl_name)
{
	GetEnvironmentVariable(_T("LOCALAPPDATA"), fl_name, MAX_PATH);
	lstrcat(fl_name, _T("\\DiscordRPC"));
}

#define FILENAME_CONFIG "config.ini"

static void GetConfigFileName(TCHAR* fl_name)
{
	GetDirectoryDiscordRPC(fl_name);
	lstrcat(fl_name, _T("\\" FILENAME_CONFIG));
}

#define WriteProp(key, val) \
	WritePrivateProfileString(_T("DiscordRPC"), \
	_T(key), val, filename)

void SaveConfig(const Config& _config)
{
	TCHAR filename[MAX_PATH]{};
	TCHAR client_id[20]{};

	GetDirectoryDiscordRPC(filename);

	if (!PathIsDirectory(filename) &&
		!CreateDirectory(filename, NULL))
	{
		ShowError();
		return;
	}

	lstrcat(filename, _T("\\" FILENAME_CONFIG));
	_stprintf(client_id, _T("%lld"), _config.client_id);

	if (!WriteProp("id",      client_id) ||
		!WriteProp("enable",  _config.enable  ? _T("1") : _T("0")) ||
		!WriteProp("show_sz", _config.show_sz ? _T("1") : _T("0")))
	{
		ShowError();
	}
}

#define ReadProp(key, def) \
	GetPrivateProfileString(_T("DiscordRPC"), \
	_T(key), _T(def), value, 20, filename)

void LoadConfig(Config& _config)
{
	TCHAR filename[MAX_PATH]{};
	GetConfigFileName(filename);

	TCHAR value[20]{};

	ReadProp("id", DEF_CLIENT_ID_STR);

	if ((_config.client_id = _ttoi64(value)) < MIN_CLIENT_ID) {
		 _config.client_id = DEFAULT_CLIENT_ID;
	}

	ReadProp("enable",  "1");
	_config.enable  = value[0] == '1';
	ReadProp("show_sz", "1");
	_config.show_sz = value[0] == '1';
}

static bool GetClientID(HWND hDlg, __int64& id)
{
	int length = GetWindowTextLength(GetDlgItem(hDlg, IDC_EDIT_CID));
	if (length == 18) {
		TCHAR num[20]{};
		GetWindowText(GetDlgItem(hDlg, IDC_EDIT_CID), num, 20);
		if ((id = _ttoi64(num)) >= MIN_CLIENT_ID) {
			return true;
		}
	}

	ShowErrorMessage(_T("This ID is invalid"), hDlg);
	return false;
}

static void InitializeControls(HWND hWnd, const Config& _config)
{
	TCHAR number[20]{};
	_stprintf(number, _T("%lld"), _config.client_id);
	SendDlgItemMessage(hWnd, IDC_EDIT_CID, WM_SETTEXT, 0, (LPARAM)number);

	static auto SetButtonCheck = [](HWND hDlg, unsigned id, bool check)
	{
		SendDlgItemMessage(hDlg, id, BM_SETCHECK,
			check ? BST_CHECKED : BST_UNCHECKED, 0);
	};

	SetButtonCheck(hWnd, IDC_ENABLE,      _config.enable);
	SetButtonCheck(hWnd, IDC_SHOW_FILESZ, _config.show_sz);
}

static bool ProcessCommand(HWND hWnd)
{
	__int64 client_id = 0;
	if (!GetClientID(hWnd, client_id))
		return false;
	static auto IsButtonChecked = [](HWND hDlg, unsigned id)
	{
		return SendDlgItemMessage(hDlg, id, BM_GETCHECK, 0, 0)
			== BST_CHECKED;
	};

	Config copy = config;

	config.client_id = client_id;
	config.enable    = IsButtonChecked(hWnd, IDC_ENABLE);
	config.show_sz   = IsButtonChecked(hWnd, IDC_SHOW_FILESZ);

	if (memcmp(&copy, &config, sizeof(Config)) != 0)
	{
		if (!config.enable) {
			ClosePresence();
		} else {
			if (core != NULL) {
				if (params.client_id != client_id) {
					ClosePresence();
				}
			}
			UpdatePresenceByTitleWindow(nppData._nppHandle);
			InitPresence(config);
		}

		SaveConfig(config);
	}
	return true;
}

static INT_PTR CALLBACK OptionsProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		InitializeControls(hWnd, config);
		return (INT_PTR)TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
		case IDCANCEL:
			if (LOWORD(wParam) == IDOK && !ProcessCommand(hWnd))
				break;
			EndDialog(hWnd, 0);
			break;
		case IDC_RESET:
		{
			Config cfg;
			SetDefaultConfig(cfg);
			InitializeControls(hWnd, cfg);
			break;
		}
		default:
			break;
		}
		
		return (INT_PTR)TRUE;
	}
	return (INT_PTR)FALSE;
}

void OptionsPlugin()
{
	DialogBox((HINSTANCE)plugin, MAKEINTRESOURCE(IDD_PLUGIN_OPTIONS),
			  nppData._nppHandle, OptionsProc);
	ShowError();
}


