/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, for both open source and commercial purposes.
 * 
 * Conditions:
 * 
 * 1. The above copyright notice and this permission notice shall be included
 *    in all copies or substantial portions of the Software.
 * 
 * 2. Attribution is appreciated but NOT required.
 *    Suggested (optional) credit:
 *    "Built using Neutron Programming Language (c) yasakei"
 * 
 * 3. The name "Neutron" and the name of the copyright holder may not be used
 *    to endorse or promote products derived from this Software without prior
 *    written permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
