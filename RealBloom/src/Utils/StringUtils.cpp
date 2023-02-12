#include "StringUtils.h"

// https://stackoverflow.com/a/3418285/18049911
void strReplace(std::string& s, const std::string& from, const std::string& to)
{
    if (from.empty())
        return;

    size_t start_pos = 0;
    while ((start_pos = s.find(from, start_pos)) != std::string::npos)
    {
        s.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

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
    size_t numWords = 0;

    std::string word;
    while (i >> word)
    {
        if (((lineLnegth + word.size()) > length) && (numWords > 0))
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

        numWords++;
    }

    std::string result = o.str();
    result = result.substr(0, result.size() - 1);
    return result;
}

void strSplit(const std::string& s, char delimiter, std::vector<std::string>& outElements)
{
    outElements.clear();

    std::size_t from = 0;
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == delimiter)
        {
            outElements.push_back(strTrim(s.substr(from, i - from)));
            from = i + 1;
        }
    }

    if (from < s.size())
        outElements.push_back(strTrim(s.substr(from, s.size() - from)));
}

std::string strFromDataSize(uint64_t bytes)
{
    static const char* suffixes[]{ "bytes", "KiB", "MiB", "GiB", "TiB" };
    static double powers[]
    {
        pow(1024, 0), // 1 byte
        pow(1024, 1), // 1 KB
        pow(1024, 2), // 1 MB
        pow(1024, 3), // 1 GB
        pow(1024, 4)  // 1 TB
    };

    uint64_t mag = (uint64_t)fmin(4, fmax(0, floor(log((double)bytes) / log(1024.0))));
    if (mag == 0)
        return strFormat("%lu %s", bytes, suffixes[mag]);
    else
        return strFormat("%.1f %s", (double)bytes / powers[mag], suffixes[mag]);
}

std::string strFromBigInteger(uint64_t bigInteger)
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

    uint64_t mag = (uint64_t)fmin(4, fmax(0, floor(log((double)bigInteger) / log(1000.0))));
    if (mag == 0)
        return strFormat("%lu", bigInteger);
    else
        return strFormat("%.1f%s", (double)bigInteger / powers[mag], suffixes[mag]);
}

std::string strFromBool(bool v)
{
    return v ? "True" : "False";
}

std::string strFromColorChannelID(uint32_t ch)
{
    if (ch == 3) return "A";
    if (ch == 2) return "B";
    if (ch == 1) return "G";
    return "R";
}

std::string strFromDuration(float seconds)
{
    if (seconds < 1.0f)
    {
        return strFormat("%.3fs", seconds);
    }
    else if (seconds < 60.0f)
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
            return strFormat("%uh %um %us", intHr, intMin, intSec);
        }
        else
        {
            return strFormat("%um %us", intMin, intSec);
        }
    }
}

std::string strFromElapsed(float seconds)
{
    uint32_t intSec = (uint32_t)floorf(seconds);

    uint32_t intHr = intSec / 3600;
    uint32_t intMin = (intSec / 60) % 60;
    intSec %= 60;

    return strFormat("%02u:%02u:%02u", intHr, intMin, intSec);
}


std::string strFromTime()
{
    time_t rawtime;
    struct tm timeinfo;
    char buffer[128];

    time(&rawtime);
    localtime_s(&timeinfo, &rawtime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);

    return std::string(buffer);
}

std::string strFromInt(int64_t v)
{
    return std::to_string(v);
}

std::string strFromFloat(float v)
{
    return strFormat("%.3f", v);
}

int64_t strToInt(const std::string& s)
{
    try
    {
        return std::stoll(s);
    }
    catch (const std::exception& e)
    {
        throw std::exception(strFormat("Couldn't parse an integer from \"%s\".", s.c_str()).c_str());
    }
}

float strToFloat(const std::string& s)
{
    try
    {
        return std::stof(s);
    }
    catch (const std::exception& e)
    {
        throw std::exception(strFormat("Couldn't parse a float from \"%s\".", s.c_str()).c_str());
    }
}

std::array<float, 4> strToRGBA(const std::string& s)
{
    try
    {
        if (s.empty())
            throw std::exception("empty string");

        std::vector<std::string> elements;
        strSplit(s, ',', elements);

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
        throw std::exception(strFormat("Failed to parse color from string \"%s\": %s", s.c_str(), e.what()).c_str());
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

        std::vector<std::string> elements;
        strSplit(s, ',', elements);

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
        throw std::exception(strFormat("Failed to parse XY from string \"%s\": %s", s.c_str(), e.what()).c_str());
    }
}
