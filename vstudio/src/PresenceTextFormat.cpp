// Copyright (C) 2022 -2025 Zukaritasu
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
#include <filesystem>

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

	// Save old values to check if something changed for idle detection
	int oldCurrentLine    = props[2],
		oldCurrentColumn  = props[3];
	__int64 oldFileLength = _lastFileLength;

	props[0] = _info.name;
	props[1] = _info.extension;

	props[2] = (int)::SendMessage(nppData._nppHandle, NPPM_GETCURRENTLINE, 0, 0) + 1;
	props[3] = (int)::SendMessage(nppData._nppHandle, NPPM_GETCURRENTCOLUMN, 0, 0) + 1;

	char file_size_buf[48] = { '\0' };
	HWND hWnd = ::GetCurrentScintilla();
	::StrFormatByteSize64A(_lastFileLength = ::SendMessage(hWnd, SCI_GETLENGTH, 0, 0), file_size_buf, 48);

	props[4] = file_size_buf;
	props[5] = (int)::SendMessage(hWnd, SCI_GETLINECOUNT, 0, 0);

	std::string lang_name = _lang_info._name;

	props[6] = GetStringCase(lang_name, false); // lower case

	lang_name[0] = (char)std::toupper(lang_name[0]);
	props[7] = lang_name; // first letter in upper case
	props[8] = GetStringCase(lang_name, true); // upper case
	props[9] = static_cast<int>(::SendMessage(hWnd, SCI_GETCURRENTPOS, 0, 0) + 1L);
	
	char currentDir[MAX_PATH] = { '\0' };
	GetEditorProperty(currentDir, NPPM_GETCURRENTDIRECTORY);

	std::string workspace, absolutePathWorkspace, repoUrl;
	if (SearchWorkspace(currentDir, workspace, absolutePathWorkspace, repoUrl))
	{
		props[10] = workspace;
		_fileFilter.LoadGitignore((std::filesystem::path(absolutePathWorkspace) / ".gitignore").string());
	}
	else
	{
		workspace = currentDir;
		props[10] = workspace.find_last_of("\\") != std::string::npos ?
				workspace.substr(workspace.find_last_of("\\/") + 1) :
			workspace;
	}
	currentRepositoryUrl = repoUrl;

	_textEditorIdle = (oldCurrentLine == (int)props[2] && oldCurrentColumn == (int)props[3] && oldFileLength == _lastFileLength);
}

void PresenceTextFormat::WriteFormat(std::string& buffer, const char* format) noexcept
{
	char buf[128] = { '\0' };
	StringBuilder builder(buf, sizeof buf);
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

	buffer = buf;
}

bool PresenceTextFormat::IsFileInfoEmpty() const noexcept
{
	return _info.name[0] == '\0';
}

const LanguageInfo& PresenceTextFormat::GetLanguageInfo() const noexcept
{
	return _lang_info;
}

bool PresenceTextFormat::IsCurrentFilePrivate() noexcept
{
	char currentDir[MAX_PATH] = { '\0' };
	GetEditorProperty(currentDir, NPPM_GETCURRENTDIRECTORY);
	std::filesystem::path parent = currentDir;
	return _fileFilter.IsPrivate((parent / _info.name).string());
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

bool PresenceTextFormat::SearchWorkspace(std::filesystem::path currentDir, std::string& workspace, std::string& absolutePathWorkspace, std::string& repoUrl) noexcept
{
	while (!currentDir.empty())
	{
		bool existsGitignoreFile = std::filesystem::exists(currentDir / ".gitignore");
		bool existsGitFolder = std::filesystem::exists(currentDir / ".git");
		if (existsGitFolder || existsGitignoreFile)
		{
			absolutePathWorkspace = currentDir.string();
			workspace = currentDir.string();
			workspace.find_last_of("\\") != std::string::npos ?
				workspace = workspace.substr(workspace.find_last_of("\\/") + 1) :
				workspace = workspace;
			if (existsGitFolder)
				GetRepositoryUrl((currentDir / ".git" / "config").string(), repoUrl);
			return true;
		}

		auto tempCurrentDir = currentDir;
		currentDir = currentDir.parent_path();
		if (currentDir == tempCurrentDir) // root reached
			break;
	}
	return false;
}

void PresenceTextFormat::GetRepositoryUrl(const std::string& fileConfig, std::string& url) noexcept
{
	if (std::filesystem::exists(fileConfig))
	{
		FILE* file = nullptr;
		fopen_s(&file, fileConfig.c_str(), "r");
		if (file != nullptr)
		{
			char line[512] = { '\0' };
			bool inRemoteOrigin = false;
			while (fgets(line, sizeof line, file) != nullptr)
			{
				if (line[0] == '[')
					inRemoteOrigin = (strstr(line, "[remote \"origin\"]") != nullptr);
				else if (inRemoteOrigin)
				{
					char* pos = strstr(line, "url = ");
					if (pos != nullptr)
					{
						pos += 6; // move past "url = "
						char* end = strchr(pos, '\n');
						if (end != nullptr) *end = '\0';
						url = pos;
						// Convert SSH URL to HTTPS if necessary
						if (url.find("git@") == 0)
						{
							size_t colonPos = url.find(':');
							if (colonPos != std::string::npos)
							{
								std::string domain = url.substr(4, colonPos - 4); // Skip "git@"
								std::string path = url.substr(colonPos + 1);
								url = "https://" + domain + "/" + path;
							}
						}
						else if (url.find("http://") == 0)
							url.replace(0, 7, "https://");
						break;
					}
				}
			}
			fclose(file);
		}
	}
	else
	{
		url.clear();
	}
}

std::string& PresenceTextFormat::GetStringCase(std::string& s, bool case_) noexcept
{
	for (char& c : s)
		c = case_ ? (char)std::toupper(c) : (char)std::tolower(c);
	return s;
}
