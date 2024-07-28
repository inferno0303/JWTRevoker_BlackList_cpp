#ifndef MSG_FORMAT_HPP
#define MSG_FORMAT_HPP

#include <string>
#include <map>

#include "../../ThirdPartyLibs/nlohmann/json.hpp"

inline std::string doMsgAssembly(const std::string& event, const std::map<std::string, std::string>& data) {
    nlohmann::json jsonObject;

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
            data[it.key()] = it.value().get<std::string>();
        }
    }
}

#endif //MSG_FORMAT_HPP
