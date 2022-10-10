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

#include "PluginConfig.h"
#include "PluginError.h"
#include "PluginResources.h"
#include "PluginDefinition.h"

#include <Windows.h>
#include <Shlwapi.h>
#include <strsafe.h>

extern NppData nppData;

#pragma warning(disable: 4996) // wcscpy

static LPTSTR GetFileNameConfig(LPTSTR buffer, int size)
{
	SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, size, (LPARAM)buffer);
	StringCbCat(buffer, sizeof(TCHAR) * size, _T("\\DiscordRPC.ini"));
	return buffer;
}

#define APPNAME _T("DiscordRPC")

void LoadConfig(PluginConfig& config)
{
	TCHAR file[MAX_PATH] = { 0 };
	GetFileNameConfig(file, MAX_PATH);

	if (!PathFileExists(file))
	{
		GetDefaultConfig(config);
		return;
	}

	auto GetBool = [&file](LPCTSTR key) -> bool
	{
		return GetPrivateProfileInt(APPNAME, key, 1, file) == 1;
	};

	config._hide_details = GetBool(_T("hideDetails"));
	config._elapsed_time = GetBool(_T("elapsedTime"));
	config._enable       = GetBool(_T("enable"));
	config._lang_image   = GetBool(_T("langImage"));
	config._hide_state   = GetBool(_T("hideState"));

	TCHAR id[19] = { 0 };

	int count = GetPrivateProfileString(APPNAME, _T("clientId"), _T(DEF_APPLICATION_ID_STR), id, 19, file);
	if (count != 18 || (config._client_id = _tstoi64(id)) < 1E17)
		config._client_id = DEF_APPLICATION_ID;

#ifndef UNICODE
	GetPrivateProfileString(APPNAME, "detailsFormat", DEF_DETAILS_FORMAT,
		config._details_format, 128, file);
	GetPrivateProfileString(APPNAME, "stateFormat", DEF_STATE_FORMAT,
		config._state_format, 128, file);
#else
	auto GetProperty = [&file](/*128*/char* buffer, LPCWSTR key, LPCWSTR defValue)
	{
		wchar_t value[128] = { 0 };
		GetPrivateProfileString(APPNAME, key, defValue, value, 128, file);
		if (value[0] == L'\0') // key= ?
			wcscpy(value, defValue);
		WideCharToMultiByte(CP_UTF8, 0, value, -1, buffer, 128, nullptr, false);
	};

	GetProperty(config._details_format, L"detailsFormat", _T(DEF_DETAILS_FORMAT));
	GetProperty(config._state_format, L"stateFormat", _T(DEF_STATE_FORMAT));
#endif // UNICODE
}

void GetDefaultConfig(PluginConfig& config)
{
	config._hide_details = false;
	config._elapsed_time = true;
	config._enable       = true;
	config._lang_image   = true;
	config._hide_state   = false;
	config._client_id    = DEF_APPLICATION_ID;

	strcpy(config._details_format, DEF_DETAILS_FORMAT);
	strcpy(config._state_format, DEF_STATE_FORMAT);
}

void SaveConfig(const PluginConfig& config)
{
	TCHAR file[MAX_PATH] = { 0 };
	GetFileNameConfig(file, MAX_PATH);

	auto WriteBool = [&file](LPCTSTR key, bool value) -> void
	{
		WritePrivateProfileString(APPNAME, key, value ? _T("1") : _T("0"), file);
	};

	WriteBool(_T("hideDetails"), config._hide_details);
	WriteBool(_T("elapsedTime"), config._elapsed_time);
	WriteBool(_T("enable"),      config._enable);
	WriteBool(_T("langImage"),   config._lang_image);
	WriteBool(_T("hideState"),   config._hide_state);

	TCHAR id[19];
	StringCbPrintf(id, sizeof(id), _T("%I64d"), config._client_id);
	WritePrivateProfileString(APPNAME, _T("clientId"), id, file);
	
#ifdef UNICODE
	wchar_t buf[128] = { 0 };
	MultiByteToWideChar(CP_UTF8, 0, config._details_format, -1, buf, 128);
	WritePrivateProfileString(APPNAME, L"detailsFormat", buf, file);

	MultiByteToWideChar(CP_UTF8, 0, config._state_format, -1, buf, 128);
	WritePrivateProfileString(APPNAME, L"stateFormat", buf, file);
#else
	WritePrivateProfileString(APPNAME, "detailsFormat", config._details_format, file);
	WritePrivateProfileString(APPNAME, "stateFormat", config._state_format, file);
#endif // UNICODE

	
}
