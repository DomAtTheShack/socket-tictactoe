#ifndef JSON_SOCKET_H
#define JSON_SOCKET_H

#include <string>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class JsonConverter {
public:
    static std::string toString(const json& data) {
        return data.dump();
    }

    static json fromString(const std::string& rawData) {
        try {
            return json::parse(rawData);
        }
        catch (const json::parse_error& e) {
            std::cerr << "[JSON Error] Parse failed: " << e.what() << '\n';
            // Return empty object (check with .empty())
            return json::object();
        }
    }
};

#endif