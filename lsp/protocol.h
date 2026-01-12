#ifndef NEUTRON_LSP_PROTOCOL_H
#define NEUTRON_LSP_PROTOCOL_H

// Windows macro undefs - must be before any standard library includes
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    // Undefine Windows macros that conflict with C++ code
    // NOTE: Do NOT undefine FAR and NEAR as they are needed by some Windows headers
    #undef TRUE
    #undef FALSE
    #undef DELETE
    #undef ERROR
    #undef IN
    #undef OUT
    #undef OPTIONAL
    #undef interface
    #undef small
    #undef max
    #undef min
#endif

// Force disable string_view support in JsonCpp on macOS to prevent linker
// errors This must be defined before including json/json.h
#if defined(__APPLE__)
#undef JSON_HAS_STRING_VIEW
#define JSON_HAS_STRING_VIEW 0
#undef JSON_USE_STRING_VIEW
#define JSON_USE_STRING_VIEW 0
#endif

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
