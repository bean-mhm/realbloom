#include "FileDialogs.h"

std::unordered_map<std::string, std::string> FileDialogs::S_PATHS;

bool FileDialogs::openDialog(const std::string& id, std::string& outFilename)
{
    std::string defaultPath = "";
    if (S_PATHS.count(id) > 0)
        defaultPath = S_PATHS[id];

    nfdchar_t* outPath = nullptr;
    nfdresult_t result = NFD_OpenDialog("", defaultPath.empty() ? nullptr : defaultPath.c_str(), &outPath);

    if (result == NFD_OKAY)
    {
        outFilename = outPath;
        free(outPath);

        std::string parentDir = std::filesystem::canonical(std::filesystem::path(outFilename).parent_path()).string();
        S_PATHS[id] = parentDir;

        return true;
    }

    return false;
}

bool FileDialogs::saveDialog(const std::string& id, const std::string& extension, std::string& outFilename)
{
    std::string defaultPath = "";
    if (S_PATHS.count(id) > 0)
        defaultPath = S_PATHS[id];

    nfdchar_t* outPath = NULL;
    nfdresult_t result = NFD_SaveDialog(extension.c_str(), defaultPath.empty() ? nullptr : defaultPath.c_str(), &outPath);

    if (result == NFD_OKAY)
    {
        outFilename = outPath;
        free(outPath);

        std::string parentDir = std::filesystem::canonical(std::filesystem::path(outFilename).parent_path()).string();
        S_PATHS[id] = parentDir;

        bool hasExtension = false;
        size_t lastDotIndex = outFilename.find_last_of('.');
        if (lastDotIndex != std::string::npos)
            if (outFilename.substr(lastDotIndex + 1) == extension)
                hasExtension = true;

        if (!hasExtension)
            outFilename += "." + extension;

        return true;
    }

    return false;
}
