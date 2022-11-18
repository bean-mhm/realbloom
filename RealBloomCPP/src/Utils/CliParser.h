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
    bool exists(const std::vector<std::string>& aliases) const;

    bool hasValue(const std::string& argument) const;
    bool hasValue(const std::vector<std::string>& aliases) const;

    const std::string& get(const std::string& argument) const;
    const std::string& get(const std::vector<std::string>& aliases) const;
    const std::string& get(size_t index) const;

    bool first(const std::string& command) const;
    bool first(const std::vector<std::string>& commands) const;
};
