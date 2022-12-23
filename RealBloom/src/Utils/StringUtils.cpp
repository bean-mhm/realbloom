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

std::string strLeftPadding(const std::string& s, size_t length, bool addSpace)
{
    if (length > s.size())
    {
        size_t numSpaces = (length - s.size());
        std::string padding;
        for (size_t i = 0; i < numSpaces; i++)
            padding += " ";
        return padding + s;
    }
    return addSpace ? (" " + s) : s;
}

std::string strRightPadding(const std::string& s, size_t length, bool addSpace)
{
    if (length > s.size())
    {
        size_t numSpaces = (length - s.size());
        std::string padding;
        for (size_t i = 0; i < numSpaces; i++)
            padding += " ";
        return s + padding;
    }
    return addSpace ? (s + " ") : s;
}


std::string strWordWrap(const std::string& s, size_t lineLength, size_t leftPadding)
{
    size_t length = lineLength - leftPadding;

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
        }
        else
        {
            o << word << ' ';
            lineLnegth += word.size() + 1;
        }
    }
    std::string result = o.str();
    result = result.substr(0, result.size() - 1);
    return result;
}

std::vector<std::string> strSplit(const std::string& s, char delimiter)
{
    std::vector<std::string> elements;

    std::size_t from = 0;
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == delimiter)
        {
            elements.push_back(strTrim(s.substr(from, i - from)));
            from = i + 1;
        }
    }
    if (from <= s.size())
        elements.push_back(strTrim(s.substr(from, s.size() - from)));

    return elements;
}

std::string strFromDuration(float seconds)
{
    if (seconds < 60.0f)
    {
        return strFormat("%.1fs", seconds);
    }
    else
    {
        uint32_t intSec = (int)floorf(seconds);

        uint32_t intHr = intSec / 3600;
        uint32_t intMin = (intSec / 60) % 60;
        intSec %= 60;

        if (intHr > 0)
        {
            return strFormat("%dh %dm %ds", intHr, intMin, intSec);
        }
        else
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

std::string strFromBool(bool v)
{
    return v ? "True" : "False";
}

std::string strFromTime()
{
    time_t rawtime;
    struct tm timeinfo;
    char buffer[128];

    time(&rawtime);
    localtime_s(&timeinfo, &rawtime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);

    std::string result = buffer;
    return result;
}

std::array<float, 4> strToRGBA(const std::string& s)
{
    try
    {
        if (s.empty())
            throw std::exception("empty string");

        std::vector<std::string> elements = strSplit(s, ',');
        if (elements.size() == 1)
        {
            float v = std::stof(elements[0]);
            return { v, v, v, 1.0f };
        }
        else if (elements.size() == 3)
        {
            return { std::stof(elements[0]), std::stof(elements[1]), std::stof(elements[2]), 1.0f };
        }
        else if (elements.size() == 4)
        {
            return { std::stof(elements[0]), std::stof(elements[1]), std::stof(elements[2]), std::stof(elements[3]) };
        }
        throw std::exception("invalid input");
    }
    catch (const std::exception& e)
    {
        throw std::exception(strFormat("Failed to parse color from string: %s", e.what()).c_str());
    }
}

std::array<float, 3> strToRGB(const std::string& s)
{
    std::array<float, 4> rgba = strToRGBA(s);
    return { rgba[0], rgba[1], rgba[2] };
}

std::array<float, 2> strToXY(const std::string& s)
{
    try
    {
        if (s.empty())
            throw std::exception("empty string");

        std::vector<std::string> elements = strSplit(s, ',');
        if (elements.size() == 1)
        {
            float v = std::stof(elements[0]);
            return { v, v };
        }
        else if (elements.size() == 2)
        {
            return { std::stof(elements[0]), std::stof(elements[1]) };
        }
        throw std::exception("invalid input");
    }
    catch (const std::exception& e)
    {
        throw std::exception(strFormat("Failed to parse XY from string: %s", e.what()).c_str());
    }
}
