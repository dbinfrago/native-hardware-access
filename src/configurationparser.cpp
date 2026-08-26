// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#include "configurationparser.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>

bool ConfigurationParser::load(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        return false;
    }

    std::string line;
    while (std::getline(file, line))
    {
        this->parseLine(line);
    }

    file.close();
    return true;
}

std::string ConfigurationParser::get(const std::string& key) const
{
    auto it = configData.find(key);
    if (it == configData.end())
    {
        throw std::runtime_error("Property not found in configuration: " + key);
    }
    return it->second;
}

bool ConfigurationParser::hasKey(const std::string& key) const
{
    return configData.find(key) != configData.end();
}

void ConfigurationParser::parseLine(const std::string& line)
{
    if (line.empty())
    {
        return;
    }

    std::stringstream ss(line);
    std::string key, value;

    ss >> key;

    // Check for comments (lines starting with #) or empty keys
    if (key.empty() || key[0] == '#')
    {
        return;
    }

    ss >> value;
    
    // trim whitespaces
    size_t first = value.find_first_not_of(" \t");
    if (first != std::string::npos)
    {
        value = value.substr(first);
    }
    size_t last = value.find_last_not_of(" \t\r\n");
    if (last != std::string::npos)
    {
        value = value.substr(0, last + 1);
    }

    if (!value.empty())
    {
        std::cerr << "INFO: parsed line from configuration file: '"<< key << "': '" << value << "'" << std::endl;
        configData[key] = value;
    }
    else
    {
        std::cerr << "WARNING: discarding line from configuration file: '"<< line << "'" << std::endl;
    }
}
