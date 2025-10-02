// Copyright (C) 2025 Zukaritasu
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
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

class FileFilter
{
public:
    FileFilter() = default;
    virtual ~FileFilter() = default;

    bool IsPrivate(const std::string& filePath) const;
    void LoadGitignore(const std::string& gitignorePath);

private:
    std::vector<std::string> ignorePatterns;
	std::string currentParent;

    bool MatchesPattern(const std::string& pattern, const std::string& path) const;
    void Clean();
};
