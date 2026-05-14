#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <limits>
#include <optional>
#include <cstdlib>

// Read key=value config file into a string map.
std::map<std::string, std::string> readConfig(const std::string& filename) {

    std::map<std::string, std::string> config;
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        // Skip empty lines or comments
        if (line.empty() || line[0] == '#') continue;

        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            std::string key = line.substr(0, delimiterPos);
            std::string value = line.substr(delimiterPos + 1);
            config[key] = value;
        }
    }
    return config;
}

// Parse an integer value from the config map. Returns std::nullopt if missing/invalid.
int getConfigInt(const std::map<std::string, std::string>& config, const std::string& key, int defaultValue) {
    auto it = config.find(key);
    if (it == config.end()) {
        return defaultValue;
    }

    const std::string& raw = it->second;
    if (raw.empty()) {
        return defaultValue;
    }

    char* end = nullptr;
    long value = std::strtol(raw.c_str(), &end, 10);
    if (end == raw.c_str() || *end != '\0') {
        return defaultValue;
    }

    return static_cast<int>(value);
}