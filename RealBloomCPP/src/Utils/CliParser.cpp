#include "CliParser.h"

// Source:
// https://stackoverflow.com/a/868894/18049911

CliParser::CliParser(int& argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
        this->m_tokens.push_back(std::string(argv[i]));
}

size_t CliParser::num() const
{
    return m_tokens.size();
}

/// @author iain
bool CliParser::exists(const std::string& argument) const
{
    return std::find(this->m_tokens.begin(), this->m_tokens.end(), argument)
        != this->m_tokens.end();
}

bool CliParser::exists(const std::vector<std::string>& aliases) const
{
    for (const auto& alias : aliases)
        if (exists(alias)) return true;
    return false;
}

bool CliParser::hasValue(const std::string& argument) const
{
    std::vector<std::string>::const_iterator itr;
    itr = std::find(this->m_tokens.begin(), this->m_tokens.end(), argument);

    if ((itr != this->m_tokens.end()) && (++itr != this->m_tokens.end()))
    {
        if (!(itr->empty()))
        {
            if (itr->starts_with("-"))
            {
                if (itr->size() > 1)
                    if (isdigit(itr->c_str()[1]) || (itr[1] == "."))
                        return true;
            }
            else
            {
                return true;
            }
        }
    }
    return false;
}

bool CliParser::hasValue(const std::vector<std::string>& aliases) const
{
    for (const auto& alias : aliases)
        if (hasValue(alias)) return true;
    return false;
}

/// @author iain
const std::string& CliParser::get(const std::string& argument) const
{
    if (!hasValue(argument))
        throw std::exception(std::string("Argument \"" + argument + "\" doesn't have a value.").c_str());

    std::vector<std::string>::const_iterator itr;
    itr = std::find(this->m_tokens.begin(), this->m_tokens.end(), argument);
    if ((itr != this->m_tokens.end()) && (++itr != this->m_tokens.end()))
        return *itr;
    return "";
}

const std::string& CliParser::get(const std::vector<std::string>& aliases) const
{
    if (!hasValue(aliases))
        throw std::exception(std::string("Argument \"" + aliases[0] + "\" doesn't have a value.").c_str());

    for (const auto& alias : aliases)
        if (hasValue(alias)) return get(alias);
    return "";
}

const std::string& CliParser::get(size_t index) const
{
    if (index < m_tokens.size())
        return m_tokens[index];
    throw std::exception(std::string("Invalid argument index (" + std::to_string(index) + ").").c_str());
}

bool CliParser::first(const std::string& command) const
{
    if (m_tokens.size() < 1)
        return false;
    return (get(0) == command);
}

bool CliParser::first(const std::vector<std::string>& commands) const
{
    for (const auto& command : commands)
        if (first(command)) return true;
    return false;
}
