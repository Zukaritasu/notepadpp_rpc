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
