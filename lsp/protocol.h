#ifndef NEUTRON_LSP_PROTOCOL_H
#define NEUTRON_LSP_PROTOCOL_H

#include <json/json.h>
#include <optional>
#include <string>
#include <vector>

namespace neutron {
namespace lsp {

struct Position {
  int line;
  int character;

  Json::Value toJson() const;
  static Position fromJson(const Json::Value &json);
};

struct Range {
  Position start;
  Position end;

  Json::Value toJson() const;
  static Range fromJson(const Json::Value &json);
};

enum class DiagnosticSeverity {
  Error = 1,
  Warning = 2,
  Information = 3,
  Hint = 4
};

struct Diagnostic {
  Range range;
  DiagnosticSeverity severity;
  std::string message;
  std::string source;

  Json::Value toJson() const;
};

struct TextDocumentItem {
  std::string uri;
  std::string languageId;
  int version;
  std::string text;
};

struct TextDocumentIdentifier {
  std::string uri;
};

struct VersionedTextDocumentIdentifier : public TextDocumentIdentifier {
  int version;
};

struct TextDocumentContentChangeEvent {
  std::optional<Range> range;
  std::string text;
};

struct DidChangeTextDocumentParams {
  VersionedTextDocumentIdentifier textDocument;
  std::vector<TextDocumentContentChangeEvent> contentChanges;
};

} // namespace lsp
} // namespace neutron

#endif // NEUTRON_LSP_PROTOCOL_H
