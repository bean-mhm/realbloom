#include "FileDialogs.h"

std::unordered_map<std::string, std::string> FileDialogs::S_PATHS;

const std::vector<std::string> DEFAULT_FILTER_LIST{ "All Files", "*" };

// Convert filter list to a vector of nfdfilteritem_t
// Example input: { "Headers", "h,hpp" }
std::vector<nfdfilteritem_t> convertFilterList(const std::vector<std::string>& filterList)
{
    std::vector<nfdfilteritem_t> filterListVec;

    for (size_t i = 0; i < filterList.size(); i += 2)
    {
        nfdfilteritem_t item;
        item.name = filterList[i].c_str();
        item.spec = filterList[i + 1].c_str();

        filterListVec.push_back(item);
    }

    return filterListVec;
}

bool FileDialogs::genericDialog(
    const std::string& id,
    std::vector<std::string> filterList,
    std::string defaultName,
    std::string& outFilename,
    bool save)
{
    // Get the default path based on id
    std::string defaultPath = "";
    if (S_PATHS.count(id) > 0)
        defaultPath = S_PATHS[id];

    // Default filter list
    if (filterList.empty())
        filterList = DEFAULT_FILTER_LIST;

    // Convert the filter list
    std::vector<nfdfilteritem_t> filterListVec = convertFilterList(filterList);

    // Open the dialog
    nfdchar_t* outPath;
    nfdresult_t result;
    if (save)
    {
        result = NFD_SaveDialog(&outPath, filterListVec.data(), filterListVec.size(), defaultPath.c_str(), defaultName.c_str());
    }
    else
    {
        result = NFD_OpenDialog(&outPath, filterListVec.data(), filterListVec.size(), defaultPath.c_str());
    }

    // Get the result
    if (result == NFD_OKAY)
    {
        outFilename = outPath;
        NFD_FreePath(outPath);

        // Update the path for id
        std::string parentDir = std::filesystem::canonical(std::filesystem::path(outFilename).parent_path()).string();
        S_PATHS[id] = parentDir;

        return true;
    }
    else if (result == NFD_ERROR)
    {
        printError(__FUNCTION__, "", NFD_GetError());
    }

    return false;
}

bool FileDialogs::openDialog(const std::string& id, const std::vector<std::string>& filterList, std::string& outFilename)
{
    return genericDialog(id, filterList, "", outFilename, false);
}

bool FileDialogs::saveDialog(const std::string& id, const std::vector<std::string>& filterList, const std::string& defaultName, std::string& outFilename)
{
    return genericDialog(id, filterList, defaultName, outFilename, true);
}
