// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#ifndef CONFIGURATIONPARSER_H
#define CONFIGURATIONPARSER_H

#include <unordered_map>
#include <string>

/**
 * @class ConfigurationParser
 * @brief A class to parse key-value configuration files.
 */
class ConfigurationParser {
private:
    std::unordered_map<std::string, std::string> configData;

public:
    /**
     * @brief Loads and parses the configuration file.
     * @param filename Path to the configuration file.
     * @return true if loaded successfully, false if file could not be opened.
     */
    bool load(const std::string& filename);

    /**
     * @brief Get value for the key specified.
     * @param key The key
     * @return the value which belongs to `key`
     * @throw runtime_error if the key is not found
     */
    std::string get(const std::string& key) const;

    /**
     * @brief Check if a key exists.
     * @param key The key
     * @return true if key exists
     */
    bool hasKey(const std::string& key) const;

private:
    /**
     * @brief Internal helper to parse a single line. Handles comments (#) and ignores empty lines.
     * @param line the line to parse, ideally '<string> <string>'
     */
    void parseLine(const std::string& line);
};

#endif // CONFIGURATIONPARSER_H
