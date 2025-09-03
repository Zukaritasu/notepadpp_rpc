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

#include "LanguageInfo.h"

#include <Windows.h>
#include <string>

const LPCSTR TOKENS[] =
{
	"%(file)", "%(extension)", "%(line)", "%(column)",
	"%(size)", "%(line_count)", "%(lang)", "%(Lang)",
	"%(LANG)", "%(position)"
};

class PresenceTextFormat
{
public:
	struct FileInfo
	{
		char name[MAX_PATH];
		char extension[MAX_PATH];
	};

	PresenceTextFormat();

	void LoadEditorStatus() noexcept;
	void WriteFormat(char* buffer, const char* format) noexcept;
	bool IsFileInfoEmpty() const noexcept;
	const LanguageInfo& GetLanguageInfo() const noexcept;

private:
	struct Property
	{
		std::string key;
		std::string value;

		Property& operator =(int value);
		Property& operator =(const std::string& value);
	};

	Property props[ARRAYSIZE(TOKENS)];
	FileInfo _info{};
	LanguageInfo _lang_info{};

	void GetEditorProperty(char* buffer, int prop) noexcept;
	bool ContainsTag(const char* format, const char* tag, size_t pos) noexcept;
	// true = upper, false = lower
	std::string& GetStringCase(std::string& s, bool case_) noexcept;
};
