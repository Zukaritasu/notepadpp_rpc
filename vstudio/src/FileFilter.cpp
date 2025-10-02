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

#include "FileFilter.hpp"

#include <filesystem>
#include <regex>

bool FileFilter::IsPrivate(const std::string& filePath) const
{
    std::string normalizedPath = filePath;
    std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

    for (const auto& pattern : ignorePatterns)
        if (MatchesPattern(pattern, normalizedPath))
            return true;
    return false;
}

void FileFilter::LoadGitignore(const std::string& gitignorePath)
{
    if (gitignorePath.empty() || !std::filesystem::exists(gitignorePath))
    {
		Clean();
        return;
    }

    ignorePatterns.clear();
    std::ifstream file(gitignorePath);
    if (!file.is_open())
    {
		Clean();
        return;
    }
    std::string line;
    while (std::getline(file, line))
    {
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        if (line.empty() || line[0] == '#')
            continue;
        ignorePatterns.push_back(line);
	}

	file.close();
	currentParent = std::filesystem::path(gitignorePath).parent_path().string();
}

static std::regex ConvertPatternToRegex(std::string& strRex, const std::string& pattern) {
    std::string rx = pattern;
    std::replace(rx.begin(), rx.end(), '\\', '/');

    if (!rx.empty() && rx[0] == '/')
        rx.erase(0, 1);

    rx = std::regex_replace(rx, std::regex(R"(\*\*)"), R"(.*)");
    rx = std::regex_replace(rx, std::regex(R"(\*)"), R"([^/]*)");
    rx = std::regex_replace(rx, std::regex(R"(\?)"), R"([^/])");

    strRex = rx;
    return std::regex(rx);
}

bool FileFilter::MatchesPattern(const std::string& pattern, const std::string& filePath) const {
    std::filesystem::path file(filePath);
    std::filesystem::path base(currentParent);

    std::error_code ec;
    std::filesystem::path relative = std::filesystem::relative(file, base, ec);
    if (ec) relative = file.filename();

    std::string normalized = relative.string();
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

	std::string patternRegex;
    std::regex rx = ConvertPatternToRegex(patternRegex, pattern);
    bool result = std::regex_search(normalized, rx);

    return result;
}

void FileFilter::Clean()
{
    ignorePatterns.clear();
	currentParent.clear();
}
