#include "StringUtils.h"

bool strContains(const std::string& source, const std::string& substring)
{
    return (source.find(substring) != std::string::npos);
}

std::string strList(const std::vector<std::string>& list, const std::string& separator)
{
    std::string result = "";
    for (size_t i = 0; i < list.size(); i++)
    {
        if (i > 0) result += separator;
        result += list[i];
    }
    return result;
}

std::string strRightPadding(const std::string& s, size_t length)
{
    if (length > s.size())
    {
        size_t numSpaces = (length - s.size());
        std::string padding;
        for (size_t i = 0; i < numSpaces; i++)
            padding += " ";
        return s + padding;
    }
    return s + " ";
}


std::string strWordWrap(const std::string& s, size_t length, size_t leftPadding)
{
    std::istringstream i(s);
    std::ostringstream o("");
    size_t lineLnegth = 0;
    std::string word;
    while (i >> word)
    {
        if ((lineLnegth + word.size()) > length)
        {
            o << '\n';
            for (size_t i = 0; i < leftPadding; i++)
                o << ' ';
            lineLnegth = 0;
        }
        if (word.size() > length)
        {
            o << word.substr(0, length - 1) << "-\n";
            for (size_t i = 0; i < leftPadding; i++)
                o << ' ';
            o << word.substr(length - 1) << ' ';
            lineLnegth = (word.size() - length + 1) + 1;
        } else
        {
            o << word << ' ';
            lineLnegth += word.size() + 1;
        }
    }
    std::string result = o.str();
    result = result.substr(0, result.size() - 1);
    return result;
}

std::string strFromDuration(float seconds)
{
    if (seconds < 60.0f)
    {
        return strFormat("%.1fs", seconds);
    } else
    {
        uint32_t intSec = (int)floorf(seconds);

        uint32_t intHr = intSec / 3600;
        uint32_t intMin = (intSec / 60) % 60;
        intSec %= 60;

        if (intHr > 0)
        {
            return strFormat("%dh %dm %ds", intHr, intMin, intSec);
        } else
        {
            return strFormat("%dm %ds", intMin, intSec);
        }
    }
}

std::string strFromElapsed(float seconds)
{
    uint32_t intSec = (int)floorf(seconds);

    uint32_t intHr = intSec / 3600;
    uint32_t intMin = (intSec / 60) % 60;
    intSec %= 60;

    return strFormat("%02d:%02d:%02d", intHr, intMin, intSec);
}

std::string strFromSize(uint64_t sizeBytes)
{
    static const char* suffixes[]{ "bytes", "KB", "MB", "GB", "TB" };
    static double powers[]
    {
        pow(1024, 0), // 1 byte
        pow(1024, 1), // 1 KB
        pow(1024, 2), // 1 MB
        pow(1024, 3), // 1 GB
        pow(1024, 4)  // 1 TB
    };

    uint64_t mag = (uint64_t)fmin(4, fmax(0, floor(log((double)sizeBytes) / log(1024.0))));
    if (mag == 0)
        return strFormat("%lu %s", sizeBytes, suffixes[mag]);
    else
        return strFormat("%.1f %s", (double)sizeBytes / powers[mag], suffixes[mag]);
}

std::string strFromBigNumber(uint64_t bigNumber)
{
    static const char* suffixes[]{ "", "K", "M", "B", "T" };
    static double powers[]
    {
        pow(1000, 0), // 1
        pow(1000, 1), // 1K
        pow(1000, 2), // 1M
        pow(1000, 3), // 1B
        pow(1000, 4)  // 1T
    };

    uint64_t mag = (uint64_t)fmin(4, fmax(0, floor(log((double)bigNumber) / log(1000.0))));
    if (mag == 0)
        return strFormat("%lu", bigNumber);
    else
        return strFormat("%.1f%s", (double)bigNumber / powers[mag], suffixes[mag]);
}
