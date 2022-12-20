#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>

#include "nfd/nfd.h"

class FileDialogs
{
private:
    static std::unordered_map<std::string, std::string> S_PATHS;

public:
    FileDialogs() = delete;
    FileDialogs(const FileDialogs&) = delete;
    FileDialogs& operator= (const FileDialogs&) = delete;

    static bool openDialog(const std::string& id, std::string& outFilename);
    static bool saveDialog(const std::string& id, const std::string& extension, std::string& outFilename);
};
