#include "protocol.h"

namespace neutron {
namespace lsp {

Json::Value Position::toJson() const {
    Json::Value json;
    json[std::string("line")] = line;
    json[std::string("character")] = character;
    return json;
}

Position Position::fromJson(const Json::Value& json) {
    return {
        json.get("line", 0).asInt(),
        json.get("character", 0).asInt()
    };
}

Json::Value Range::toJson() const {
    Json::Value json;
    json[std::string("start")] = start.toJson();
    json[std::string("end")] = end.toJson();
    return json;
}

Range Range::fromJson(const Json::Value& json) {
    return {
        Position::fromJson(json[std::string("start")]),
        Position::fromJson(json[std::string("end")])
    };
}

Json::Value Diagnostic::toJson() const {
    Json::Value json;
    json[std::string("range")] = range.toJson();
    json[std::string("severity")] = static_cast<int>(severity);
    json[std::string("message")] = message;
    json[std::string("source")] = source;
    return json;
}

} // namespace lsp
} // namespace neutron
