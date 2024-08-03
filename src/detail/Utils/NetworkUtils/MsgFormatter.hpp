#ifndef MSG_FORMATTER_HPP
#define MSG_FORMATTER_HPP

#include <string>
#include <map>

#include "../../ThirdPartyLibs/nlohmann/json.hpp"

inline std::string doMsgAssembly(const std::string& event, const std::map<std::string, std::string>& data) {
    nlohmann::basic_json<nlohmann::ordered_map> jsonObject;

    // 按顺序添加键值对
    jsonObject["event"] = event;

    nlohmann::json dataObject;
    for (const auto& kv : data) {
        dataObject[kv.first] = kv.second;
    }
    jsonObject["data"] = dataObject;

    return jsonObject.dump();
}

inline void doMsgParse(const std::string& jsonStr, std::string& event, std::map<std::string, std::string>& data) {
    nlohmann::json jsonObject = nlohmann::json::parse(jsonStr);

    if (jsonObject.contains("event")) {
        event = jsonObject["event"].get<std::string>();
    }

    if (jsonObject.contains("data")) {
        nlohmann::json dataObject = jsonObject["data"];
        for (auto it = dataObject.begin(); it != dataObject.end(); ++it) {
            try {
                // 如果当前值是字符串类型
                if (it.value().is_string()) {
                    data[it.key()] = it.value().get<std::string>();
                }
                // 如果当前值是数字类型
                else if (it.value().is_number()) {
                    data[it.key()] = std::to_string(it.value().get<double>());
                }
                // 处理其他类型，例如布尔值或其他JSON对象
                else if (it.value().is_boolean()) {
                    data[it.key()] = it.value().get<bool>() ? "true" : "false";
                }
                else if (it.value().is_null()) {
                    data[it.key()] = "null";
                }
                // 处理其他无法预料的类型
                else {
                    data[it.key()] = "unsupported_type";
                }
            } catch (const std::exception& e) {
                // 如果转换失败，捕获异常并处理
                std::cerr << "Error parsing key: " << it.key() << ", value: " << it.value() << ", error: " << e.what() << std::endl;
                data[it.key()] = "error";
            }
        }
    }
}

#endif //MSG_FORMATTER_HPP
