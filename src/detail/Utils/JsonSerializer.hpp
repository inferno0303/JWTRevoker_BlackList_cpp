#ifndef JSON_STRING_HPP
#define JSON_STRING_HPP

#include <string>
#include <map>
#include <boost/json/src.hpp>

inline std::string msgAssembly(const std::string &event, const std::map<std::string, std::string> &data) {
    boost::json::object jsonObject;
    jsonObject["event"] = event;
    boost::json::object dataObject;
    for (const auto &[fst, snd]: data) {
        dataObject[fst] = snd;
    }
    jsonObject["data"] = dataObject;
    // 将对象序列化为 json 格式的字符串
    return boost::json::serialize(jsonObject);
}

inline void msgParse(const std::string &jsonStr, std::string &event, std::map<std::string, std::string> &data) {
    boost::json::value jsonValue = boost::json::parse(jsonStr);
    if (!jsonValue.is_object()) {
        throw std::runtime_error("Invalid JSON format");
    }
    boost::json::object jsonObject = jsonValue.as_object();
    // 解析 event 字段 -> std::string
    if (jsonObject.contains("event")) {
        event = boost::json::value_to<std::string>(jsonObject["event"]);
    }
    // 解析 data 字段 -> std::map<std::string, std::string>
    if (jsonObject.contains("data")) {
        if (boost::json::value &dataValue = jsonObject["data"]; dataValue.is_object() || dataValue.is_array()) {
            for (boost::json::object dataObject = dataValue.as_object(); auto &it: dataObject) {
                try {
                    if (it.value().is_string()) {
                        data[it.key()] = boost::json::value_to<std::string>(it.value());
                    } else if (it.value().is_int64()) {
                        data[it.key()] = std::to_string(it.value().as_int64());
                    } else if (it.value().is_uint64()) {
                        data[it.key()] = std::to_string(it.value().as_uint64());
                    } else if (it.value().is_double()) {
                        data[it.key()] = std::to_string(it.value().as_double());
                    } else if (it.value().is_bool()) {
                        data[it.key()] = it.value().as_bool() ? "true" : "false";
                    } else if (it.value().is_null()) {
                        data[it.key()] = "null";
                    } else {
                        data[it.key()] = "unsupported_type";
                    }
                } catch (const std::exception &e) {
                    std::cerr << "Error parsing key: " << it.key() << ", value: " << it.value() << ", error: " << e.
                            what() << std::endl;
                    data[it.key()] = "error";
                }
            }
        } else {
            data["message"] = boost::json::serialize(dataValue);
        }
    }
}

#endif //JSON_STRING_HPP
