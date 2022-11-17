#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

// Source:
// https://stackoverflow.com/a/868894/18049911

class CliParser
{
private:
    std::vector<std::string> m_tokens;

public:
    CliParser(int& argc, char** argv);

    size_t num() const;

    bool exists(const std::string& argument) const;
    bool exists(const std::string& argument, const std::string& argumentShort) const;
    bool anyExists(const std::vector<std::string>& arguments) const;

    bool hasValue(const std::string& argument, const std::string& argumentShort) const;
    const std::string& get(const std::string& argument) const;
    const std::string& get(const std::string& argument, const std::string& argumentShort) const;
    const std::string& get(size_t index) const;

    bool first(const std::string& command) const;
};
