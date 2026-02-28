#include "protocol.h"

namespace neutron {
namespace lsp {

Json::Value Position::toJson() const {
  Json::Value json;
  json["line"] = line;
  json["character"] = character;
  return json;
}

Position Position::fromJson(const Json::Value &json) {
  return {json.get("line", 0).asInt(), json.get("character", 0).asInt()};
}

Json::Value Range::toJson() const {
  Json::Value json;
  json["start"] = start.toJson();
  json["end"] = end.toJson();
  return json;
}

Range Range::fromJson(const Json::Value &json) {
  return {Position::fromJson(json["start"]), Position::fromJson(json["end"])};
}

Json::Value Diagnostic::toJson() const {
  Json::Value json;
  json["range"] = range.toJson();
  json["severity"] = static_cast<int>(severity);
  json["message"] = message;
  json["source"] = source;
  return json;
}

} // namespace lsp
} // namespace neutron
