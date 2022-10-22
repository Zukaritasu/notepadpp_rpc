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

#define BUFFERSIZE sizeof(DiscordActivity::details)

struct ScintillaStatus
{
	std::string key;
	std::string value;
};

static ScintillaStatus status[] =
{
	{ "%(file)" }, { "%(extension)" } , { "%(line)" }, { "%(column)" }, 
	{ "%(size)" }, { "%(line_count)" }, { "%(lang)" }
};

extern NppData nppData;

static void LoadScintillaStatus(const FileInfo* info, const LanguageInfo* lang)
{
	assert(info != nullptr);
	
	status[0].value = info->name;
	status[1].value = info->extension;

	status[2].value = std::to_string((int)SNDMSG(nppData._nppHandle, NPPM_GETCURRENTLINE, 0, 0) + 1);
	status[3].value = std::to_string((int)SNDMSG(nppData._nppHandle, NPPM_GETCURRENTCOLUMN, 0, 0) + 1);

	char file_size_buf[48] = { 0 };
	HWND hWnd = GetCurrentScintilla();
	StrFormatByteSize64A(SNDMSG(hWnd, SCI_GETLENGTH, 0, 0), file_size_buf, 48);
	status[4].value = file_size_buf;
	status[5].value = std::to_string((int)SNDMSG(hWnd, SCI_GETLINECOUNT, 0, 0) + 1);
	status[6].value = lang != nullptr ? lang->name : "";
}

static bool ConstainsTag(const char* format, const char* tag, size_t pos)
{
	for (size_t i = 0; tag[i] != '\0'; i++)
		if (format[pos] == '\0' || format[pos++] != tag[i])
			return false;
	return true;
}

static void AppendString(char* buf, const char* tag, size_t& bpos, size_t& fpos, const std::string& value)
{
	if ((BUFFERSIZE - bpos) > 0)
	{
		StringCchPrintfA(buf + bpos, BUFFERSIZE - bpos, "%s", value.c_str());
		bpos += strlen(buf + bpos);
		fpos += strlen(tag) - 1;
	}
}

void ProcessFormat(char* buf, const char* format, const FileInfo* info, const LanguageInfo* lang)
{
	LoadScintillaStatus(info, lang);
	
	bool _continue;
	for (size_t i = 0, j = 0; format[i] != '\0' && j < (BUFFERSIZE - 1); i++)
	{
		_continue = false;
		if (format[i] == '%' && format[i + 1] == '(' && format[i + 2] != '\0')
		{
			for (size_t k = 0; k < ARRAYSIZE(status); k++)
			{
				if (format[i + 2] == status[k].key[2] && ConstainsTag(format, status[k].key.c_str(), i))
				{
					AppendString(buf, status[k].key.c_str(), j, i, status[k].value);
					_continue = true;
					break;
				}
			}
		}

		if (_continue) continue;

		buf[j++] = format[i];
		buf[j] = 0;
	}
}
