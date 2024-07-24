#ifndef CONFIGREADER_HPP
#define CONFIGREADER_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>

// 函数用于去除字符串两端的空白字符
inline std::string trim(const std::string& str) {
    std::string result = str;

    // 去除开头的空白字符
    result.erase(0, result.find_first_not_of(" \t"));

    // 去除结尾的空白字符
    result.erase(result.find_last_not_of(" \t") + 1);

    return result;
}

// 定义一个函数来读取配置文件并将其存储在一个unordered_map中
inline std::unordered_map<std::string, std::string> readConfig(const std::string& filename) {
    std::unordered_map<std::string, std::string> config;

    // 打开配置文件
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open the file: " << filename << std::endl;
        return config;
    }

    std::string line;
    while (std::getline(file, line)) {
        // 去除前后空白字符
        line = trim(line);

        // 忽略空行和注释行（以#开头）
        if (line.empty() || line[0] == '#')
            continue;

        // 查找等号位置
        const auto delimiterPos = line.find('=');
        if (delimiterPos == std::string::npos) {
            std::cerr << "Invalid line in config file: " << line << std::endl;
            continue;
        }

        // 获取 key 和 value
        std::string key = line.substr(0, delimiterPos);
        std::string value = line.substr(delimiterPos + 1);

        // 去除 key 和 value 两端的空白字符
        key = trim(key);
        value = trim(value);

        // 存储到unordered_map中
        config[key] = value;
    }

    return config;
}

#endif // CONFIGREADER_HPP
