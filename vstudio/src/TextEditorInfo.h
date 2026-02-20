// Copyright (C) 2022 - 2025 Zukaritasu
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

#include "LanguageInfo.h"

#include <Windows.h>
#include <string>
#include <filesystem>

#include "FileFilter.hpp"

const LPCSTR TOKENS[] =
{
	"%(file)", "%(extension)", "%(line)", "%(column)",
	"%(size)", "%(line_count)", "%(lang)", "%(Lang)",
	"%(LANG)", "%(position)", "%(workspace)"
};

class TextEditorInfo
{
public:
	struct FileInfo
	{
		std::string name;
		std::string extension;
	};

	TextEditorInfo();

	void LoadEditorStatus() noexcept;
	void WriteFormat(std::string& buffer, const char* format) noexcept;
	bool IsFileInfoEmpty() const noexcept;
	const LanguageInfo& GetLanguageInfo() const noexcept;
	const std::string& GetCurrentRepositoryUrl() const noexcept { return currentRepositoryUrl; }
	bool IsCurrentFilePrivate() noexcept;
	bool IsTextEditorIdling() const noexcept { return _textEditorIdling; }

	static std::wstring GetEditorTextPropertyW(int prop);

private:
	struct Property
	{
		std::string key;
		std::string value;

		Property& operator =(int value);
		Property& operator =(const std::string& value);

		operator int() const noexcept { return std::atoi(value.c_str()); }
		operator const std::string& () const noexcept { return value; }
		operator __int64() const noexcept { return std::atoll(value.c_str()); }
	};

	std::string currentRepositoryUrl{};

	Property props[ARRAYSIZE(TOKENS)];
	FileInfo _info{};
	LanguageInfo _lang_info;
	FileFilter _fileFilter{};
	bool _textEditorIdling = false;
	__int64 _lastFileLength = 0;

	bool ContainsTag(const char* format, const char* tag, size_t pos) noexcept;
	bool SearchWorkspace(std::filesystem::path currentDir, std::string& workspace, std::string& absolutePathWorkspace, std::string& repoUrl) noexcept;
	void GetRepositoryUrl(const std::string& fileConfig, std::string& url) noexcept;
	// true = upper, false = lower
	std::string& GetStringCase(std::string& s, bool case_) noexcept;


	static std::string GetEditorTextProperty(int prop);
	static std::string GetFormattedCurrentFileSize(HWND hWndScin, int64_t* fileSize = nullptr);
};
