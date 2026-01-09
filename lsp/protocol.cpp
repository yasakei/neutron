#include "protocol.h"

namespace neutron {
namespace lsp {

// Helper to safely set JSON values (force const char* overload)
static inline Json::Value& pset(Json::Value& v, const char* key) {
    return v[key];
}

static inline const Json::Value& pget(const Json::Value& v, const char* key) {
    return v[key];
}

Json::Value Position::toJson() const {
    Json::Value json;
    pset(json, "line") = line;
    pset(json, "character") = character;
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
