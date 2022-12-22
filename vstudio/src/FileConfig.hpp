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

#pragma once

#include <string>
#include <windows.h>
#include <tchar.h>
#include <errno.h>

typedef std::basic_string<TCHAR> TString;

class FileConfig {
public:
	FileConfig(const TString& file_name, const TString& appname) :
		file_name(file_name), appname(appname) {}

	bool GetBool(const TString& key, bool def = true);
	__int64 GetInt64(const TString& key, __int64 def = 0);
	std::string GetString(const TString& key, const TString& def);
private:
	TString file_name, appname;
};

inline bool FileConfig::GetBool(const TString& key, bool def)
{
	return GetPrivateProfileInt(appname.c_str(), key.c_str(), def ? 1 : 0, file_name.c_str()) == 1;
}

inline __int64 FileConfig::GetInt64(const TString& key, __int64 def)
{
	TCHAR number[48] = { '\0' };
	int count = GetPrivateProfileString(appname.c_str(), key.c_str(), nullptr,
			number, ARRAYSIZE(number), file_name.c_str());
	if (count == 0)
		return def;
	auto result = _tstoi64(number);
	return errno == ERANGE ? def : result;
}

inline std::string FileConfig::GetString(const TString& key, const TString& def)
{
	TCHAR buffer[256] = { '\0' };
	int count = GetPrivateProfileString(appname.c_str(), key.c_str(), def.c_str(),
			buffer, 256, file_name.c_str());
#ifdef UNICODE
	if (count > 0)
	{
		char ansi[256];
		WideCharToMultiByte(CP_UTF8, 0, buffer, -1, ansi, 256, nullptr, false);
		return ansi;
	}
#endif // UNICODE
	return count == 0 ? def : buffer;
}
