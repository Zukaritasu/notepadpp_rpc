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

#include "PluginUtil.h"

// FieInfo struct contains information on the path and name of the file,
// including the extension. In case the file has no extension, it will
// be classified as a text file
struct FileInfo
{
	char  name[MAX_PATH];
	char  extension[MAX_PATH];
};

void ProcessFormat(char* buf, const char* format, const FileInfo* info, const LanguageInfo* lang = nullptr);
