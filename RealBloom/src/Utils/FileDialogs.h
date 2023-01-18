#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

#include <nfde/nfd.h>

#include "Misc.h"

class FileDialogs
{
private:
    static std::unordered_map<std::string, std::string> S_PATHS;

    static bool genericDialog(
        const std::string& id,
        std::vector<std::string> filterList,
        std::string defaultName,
        std::string& outFilename,
        bool save);

public:
    FileDialogs() = delete;
    FileDialogs(const FileDialogs&) = delete;
    FileDialogs& operator= (const FileDialogs&) = delete;

    static bool openDialog(
        const std::string& id,
        const std::vector<std::string>& filterList,
        std::string& outFilename);

    static bool saveDialog(
        const std::string& id,
        const std::vector<std::string>& filterList,
        const std::string& defaultName,
        std::string& outFilename);
};
