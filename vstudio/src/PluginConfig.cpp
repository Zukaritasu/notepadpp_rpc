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
#include "PluginResources.h"
#include "PluginDefinition.h"

#include <windows.h>
#include <shlwapi.h>
#include <tchar.h>
#include <fstream>
#include <string.h>

#include "PluginError.h"

extern NppData nppData;

#pragma warning(disable: 4996)

#define APPNAME _T("DiscordRPC")

static TCHAR* GetFileNameConfig(TCHAR* buffer, size_t size)
{
	SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, size, (LPARAM)buffer);
	_tcsncat(buffer, _T("\\DiscordRPC.ini"), size);
	return buffer;
}

static void ReadProperty(char buffer[128], LPCTSTR key, LPCTSTR def, LPCTSTR file)
{
#ifdef UNICODE
	wchar_t value[128] = { 0 };
	GetPrivateProfileString(APPNAME, key, def, value, 128, file);
	WideCharToMultiByte(CP_UTF8, 0, value, -1, buffer, 128, nullptr, false);
#else
	GetPrivateProfileString(APPNAME, key, def, buffer, 128, file);
#endif // UNICODE
}

static void WriteProperty(LPCTSTR key, const char* value, LPCTSTR file)
{
#ifdef UNICODE
	wchar_t buf[128] = { 0 };
	MultiByteToWideChar(CP_UTF8, 0, value, -1, buf, 128);
	WritePrivateProfileString(APPNAME, key, buf, file);
#else
	WritePrivateProfileString(APPNAME, key, value, file);
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
	strcpy(config._large_text_format, DEF_LARGE_TEXT_FORMAT);
}

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

	TCHAR id[20] = { 0 };
	int count = GetPrivateProfileString(APPNAME, _T("clientId"), _T(DEF_APPLICATION_ID_STR), id, ARRAYSIZE(id), file);
	errno = 0;
	if (count < 18 || count > 19 || (config._client_id = _ttoi64(id)) < 1E17 || errno == ERANGE)
	{
		config._client_id = DEF_APPLICATION_ID;
	}

	ReadProperty(config._details_format, _T("detailsFormat"), _T(DEF_DETAILS_FORMAT), file);
	ReadProperty(config._state_format, _T("stateFormat"), _T(DEF_STATE_FORMAT), file);
	ReadProperty(config._large_text_format, _T("largeTextFormat"), _T(DEF_LARGE_TEXT_FORMAT), file);
}

void SaveConfig(const PluginConfig& config)
{
	TCHAR file[MAX_PATH] = { 0 };
	GetFileNameConfig(file, MAX_PATH);

	auto WriteBool = [&file](LPCTSTR key, bool value)
	{
		WritePrivateProfileString(APPNAME, key, value ? _T("1") : _T("0"), file);
	};

	WriteBool(_T("hideDetails"), config._hide_details);
	WriteBool(_T("elapsedTime"), config._elapsed_time);
	WriteBool(_T("enable"),      config._enable);
	WriteBool(_T("langImage"),   config._lang_image);
	WriteBool(_T("hideState"),   config._hide_state);

	TCHAR id[20] = { '\0' };
	_sntprintf(id, 20, _T("%I64d"), config._client_id);
	WritePrivateProfileString(APPNAME, _T("clientId"), id, file);
	
	WriteProperty(_T("detailsFormat"), config._details_format, file);
	WriteProperty(_T("stateFormat"), config._state_format, file);
	WriteProperty(_T("largeTextFormat"), config._large_text_format, file);
}
