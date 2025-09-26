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
