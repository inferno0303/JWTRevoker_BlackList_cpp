#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

using namespace std;

string escapeString(const string& str) {
    stringstream ss;
    ss << '"';
    for (char c : str) {
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    ss << "\\u" << hex << setw(4) << setfill('0') << (int)c;
                } else {
                    ss << c;
                }
        }
    }
    ss << '"';
    return ss.str();
}

string intToJson(int number) {
    return to_string(number);
}

string arrayToJson(const vector<int>& array) {
    stringstream ss;
    ss << '[';
    for (size_t i = 0; i < array.size(); ++i) {
        if (i > 0) ss << ',';
        ss << array[i];
    }
    ss << ']';
    return ss.str();
}

string stringToJson(const string& str) {
    return escapeString(str);
}

string createJsonObject(int number, const vector<int>& array, const string& str) {
    stringstream ss;
    ss << '{';
    ss << "\"number\":" << intToJson(number) << ',';
    ss << "\"array\":" << arrayToJson(array) << ',';
    ss << "\"string\":" << stringToJson(str);
    ss << '}';
    return ss.str();
}