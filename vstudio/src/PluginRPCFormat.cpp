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

#include "PluginRPCFormat.h"
#include "PluginInterface.h"

#include <strsafe.h>
#include <discord_game_sdk.h>
#include <string>
#include <stdio.h>
#include <Shlwapi.h>
#include <assert.h>
#include <cctype>

#define BUFFERSIZE sizeof(DiscordActivity::details)

struct ScintillaStatus
{
	std::string key;
	std::string value;
};

static ScintillaStatus status[] =
{
	{ "%(file)" }, { "%(extension)" } , { "%(line)" }, { "%(column)" }, 
	{ "%(size)" }, { "%(line_count)" }, { "%(lang)" }, { "%(Lang)" },
	{ "%(LANG)" }
};

class StringBuilder
{
private:
	int _length;
	int _count;
	char* _buf;

public:
	StringBuilder(char* buf, int length)
	{
		_buf = buf;
		_length = length;
		_count = 0;
	}
	
	void Append(const std::string& str)
	{
		for (char c : str)
		{
			Append(c);
			if (IsFull())
			{
				break;
			}
		}
	}
	
	void Append(char c)
	{
		if ((_count + 2) < _length)
		{
			_buf[_count++] = c;
			_buf[_count  ] = '\0';
		}
	}
	
	bool IsFull()
	{
		return _count == _length;
	}
};

extern NppData nppData;

static void LoadScintillaStatus(const FileInfo* info, const LanguageInfo* lang)
{
	assert(info != nullptr && lang != nullptr);
	
	status[0].value = info->name;
	status[1].value = info->extension;

	status[2].value = std::to_string((int)SNDMSG(nppData._nppHandle, NPPM_GETCURRENTLINE, 0, 0) + 1);
	status[3].value = std::to_string((int)SNDMSG(nppData._nppHandle, NPPM_GETCURRENTCOLUMN, 0, 0) + 1);

	char file_size_buf[48] = { 0 };
	HWND hWnd = GetCurrentScintilla();
	StrFormatByteSize64A(SNDMSG(hWnd, SCI_GETLENGTH, 0, 0), file_size_buf, 48);
	status[4].value = file_size_buf;
	status[5].value = std::to_string((int)SNDMSG(hWnd, SCI_GETLINECOUNT, 0, 0) + 1);
	
	// true = upper false = lower
	static auto GetStringCase = [](std::string& s, bool case_) -> std::string&
	{
		for (char& c : s) c = case_ ? (char)std::toupper(c) : (char)std::tolower(c);
		return s;
	};
	
	std::string lang_name = lang->name;
	
	status[6].value = GetStringCase(lang_name, false); // lower case
	
	lang_name[0]    = (char)std::toupper(lang_name[0]);
	status[7].value = lang_name; // first letter in upper case
	status[8].value = GetStringCase(lang_name, true); // upper case
}

static bool ConstainsTag(const char* format, const char* tag, size_t pos)
{
	for (size_t i = 0; tag[i] != '\0'; i++)
		if (format[pos] == '\0' || format[pos++] != tag[i])
			return false;
	return true;
}

void ProcessFormat(char* buf, const char* format, const FileInfo* info, const LanguageInfo* lang)
{
	LoadScintillaStatus(info, lang);
	
	StringBuilder builder(buf, BUFFERSIZE);
	bool _continue;

	for (size_t i = 0; format[i] != '\0' && !builder.IsFull(); i++)
	{
		_continue = false;
		if (format[i] == '%' && format[i + 1] == '(' && format[i + 2] != '\0')
		{
			for (size_t k = 0; k < ARRAYSIZE(status); k++)
			{
				if (format[i + 2] == status[k].key[2] && ConstainsTag(format, status[k].key.c_str(), i))
				{
					builder.Append(status[k].value);
					i += status[k].key.length() - 1;
					_continue = true;
					break;
				}
			}
		}

		if (_continue) continue;
		
		builder.Append(format[i]);
	}
}
