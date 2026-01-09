#include "protocol.h"

namespace neutron {
namespace lsp {

// Helper to safely set JSON values (avoids string_view issues on older jsoncpp)
static inline Json::Value& pset(Json::Value& v, const char* key) {
    std::string k(key);
    return v[k];
}

static inline const Json::Value& pget(const Json::Value& v, const char* key) {
    std::string k(key);
    return v[k];
}

Json::Value Position::toJson() const {
    Json::Value json;
    pset(json, "line") = line;
    pset(json, "character") = character;
    return json;
}

Position Position::fromJson(const Json::Value& json) {
    std::string lineKey("line");
    std::string charKey("character");
    return {
        json.get(lineKey, 0).asInt(),
        json.get(charKey, 0).asInt()
    };
}

Json::Value Range::toJson() const {
    Json::Value json;
    pset(json, "start") = start.toJson();
    pset(json, "end") = end.toJson();
    return json;
}

Range Range::fromJson(const Json::Value& json) {
    return {
        Position::fromJson(pget(json, "start")),
        Position::fromJson(pget(json, "end"))
    };
}

Json::Value Diagnostic::toJson() const {
    Json::Value json;
    pset(json, "range") = range.toJson();
    pset(json, "severity") = static_cast<int>(severity);
    pset(json, "message") = message;
    pset(json, "source") = source;
    return json;
}

} // namespace lsp
} // namespace neutron
