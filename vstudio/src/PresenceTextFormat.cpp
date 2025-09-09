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

#include "PresenceTextFormat.h"
#include "PluginInterface.h"
#include "StringBuilder.h"
#include "PluginUtil.h"

#include <cctype>
#include <Shlwapi.h>
#include <discord_game_sdk.h>

#ifdef _DEBUG
	#include <cstdio>
#endif // _DEBUG


#pragma warning(disable: 4458 4996)

extern NppData nppData;

PresenceTextFormat::Property& 
PresenceTextFormat::Property::operator =(int value)
{
	this->value = std::to_string(value);
	return *this;
}

PresenceTextFormat::Property& 
PresenceTextFormat::Property::operator =(const std::string& value)
{
	this->value = value;
	return *this;
}

PresenceTextFormat::PresenceTextFormat()
{
	for (size_t i = 0; i < ARRAYSIZE(TOKENS); i++)
	{
		props[i].key = TOKENS[i];
	}
}

void PresenceTextFormat::LoadEditorStatus() noexcept
{
	GetEditorProperty(_info.name, NPPM_GETFILENAME);
	GetEditorProperty(_info.extension, NPPM_GETEXTPART);
	_lang_info = ::GetLanguageInfo(strlwr(_info.extension));

	static_assert(ARRAYSIZE(TOKENS) != 9, "TOKENS and PresenceTextFormat::props");

	props[0] = _info.name;
	props[1] = _info.extension;

	props[2] = (int)::SendMessage(nppData._nppHandle, NPPM_GETCURRENTLINE, 0, 0) + 1;
	props[3] = (int)::SendMessage(nppData._nppHandle, NPPM_GETCURRENTCOLUMN, 0, 0) + 1;

	char file_size_buf[48] = { '\0' };
	HWND hWnd = ::GetCurrentScintilla();
	::StrFormatByteSize64A(::SendMessage(hWnd, SCI_GETLENGTH, 0, 0), file_size_buf, 48);

	props[4] = file_size_buf;
	props[5] = (int)::SendMessage(hWnd, SCI_GETLINECOUNT, 0, 0) + 1;

	std::string lang_name = _lang_info._name;

	props[6] = GetStringCase(lang_name, false); // lower case

	lang_name[0] = (char)std::toupper(lang_name[0]);
	props[7] = lang_name; // first letter in upper case
	props[8] = GetStringCase(lang_name, true); // upper case
	props[9] = static_cast<int>(::SendMessage(hWnd, SCI_GETCURRENTPOS, 0, 0) + 1L);
	
	char currentDir[MAX_PATH] = { '\0' };
	GetEditorProperty(currentDir, NPPM_GETCURRENTDIRECTORY);

	std::string workspace;
	if (SearchWorkspace(currentDir, workspace))
		props[10] = workspace;
	else
		props[10] = "<unknown>";
}

void PresenceTextFormat::WriteFormat(char* buffer, const char* format) noexcept
{
	StringBuilder builder(buffer, sizeof(DiscordActivity::details));
	bool _continue;

	for (size_t i = 0; format[i] != '\0' && !builder.IsFull(); i++)
	{
		_continue = false;
		if (format[i] == '%' && format[i + 1] == '(' && format[i + 2] != '\0')
		{
			for (size_t k = 0; k < ARRAYSIZE(props); k++)
			{
				if (format[i + 2] == props[k].key[2] && ContainsTag(format, props[k].key.c_str(), i))
				{
					builder.Append(props[k].value);
					i += props[k].key.length() - 1;
					_continue = true;
					break;
				}
			}
		}

		if (_continue) continue;

		builder.Append(format[i]);
	}
}

bool PresenceTextFormat::IsFileInfoEmpty() const noexcept
{
	return _info.name[0] == '\0';
}

const LanguageInfo& PresenceTextFormat::GetLanguageInfo() const noexcept
{
	return _lang_info;
}

void PresenceTextFormat::GetEditorProperty(char* buffer, int prop) noexcept
{
	buffer[0] = '\0';
#ifndef UNICODE
	::SendMessage(nppData._nppHandle, prop, MAX_PATH, reinterpret_cast<LPARAM>(buffer));
#else
	wchar_t wbuffer[MAX_PATH] = { L'\0' };
	::SendMessage(nppData._nppHandle, prop, MAX_PATH, reinterpret_cast<LPARAM>(wbuffer));
	::WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, buffer, MAX_PATH, nullptr, FALSE);
#endif // UNICODE
}

bool PresenceTextFormat::ContainsTag(const char* format, const char* tag, size_t pos) noexcept
{
	for (size_t i = 0; tag[i] != '\0'; i++)
		if (format[pos] == '\0' || format[pos++] != tag[i])
			return false;
	return true;
}

bool PresenceTextFormat::SearchWorkspace(std::filesystem::path currentDir, std::string& workspace) noexcept
{
	while (!currentDir.empty())
	{
		if (std::filesystem::exists(currentDir / ".git"))
		{
			workspace = currentDir.string();
			workspace.find_last_of("\\") != std::string::npos ?
				workspace = workspace.substr(workspace.find_last_of("\\/") + 1) :
				workspace = workspace;
			return true;
		}

		auto tempCurrentDir = currentDir;
		currentDir = currentDir.parent_path();
		if (currentDir == tempCurrentDir) // root reached
			break;
	}
	return false;
}

std::string& PresenceTextFormat::GetStringCase(std::string& s, bool case_) noexcept
{
	for (char& c : s)
		c = case_ ? (char)std::toupper(c) : (char)std::tolower(c);
	return s;
}
