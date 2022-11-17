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
    if (m_tokens.size() < 1)
        return false;
    return std::find(this->m_tokens.begin(), this->m_tokens.end(), argument)
        != this->m_tokens.end();
}

bool CliParser::exists(const std::string& argument, const std::string& argumentShort) const
{
    if (m_tokens.size() < 1)
        return false;
    return (exists(argument) || exists(argumentShort));
}

bool CliParser::anyExists(const std::vector<std::string>& arguments) const
{
    if (m_tokens.size() < 1)
        return false;

    for (const auto& argument : arguments)
    {
        if (exists(argument))
            return true;
    }
    return false;
}

bool CliParser::hasValue(const std::string& argument, const std::string& argumentShort) const
{
    if (m_tokens.size() < 1)
        return false;

    std::vector<std::string>::const_iterator itr;
    itr = std::find(this->m_tokens.begin(), this->m_tokens.end(), argument);

    if ((itr != this->m_tokens.end()) && (++itr != this->m_tokens.end()))
        return true;
    return false;
}

/// @author iain
const std::string& CliParser::get(const std::string& argument) const
{
    std::vector<std::string>::const_iterator itr;
    itr = std::find(this->m_tokens.begin(), this->m_tokens.end(), argument);
    if (itr != this->m_tokens.end() && ++itr != this->m_tokens.end())
    {
        return *itr;
    }
    throw std::exception(std::string("Argument \"" + argument + "\" doesn't have a value.").c_str());
}

const std::string& CliParser::get(size_t index) const
{
    if (index < m_tokens.size())
    {
        return m_tokens[index];
    }
    throw std::exception(std::string("Invalid argument index (" + std::to_string(index) + ").").c_str());
}

const std::string& CliParser::get(const std::string& argument, const std::string& argumentShort) const
{
    if (exists(argumentShort))
        return get(argumentShort);
    return get(argument);
}

bool CliParser::first(const std::string& command) const
{
    if (m_tokens.size() < 1)
        return false;
    return (get(0) == command);
}
