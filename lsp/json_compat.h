#pragma once

// Fix for JsonCpp linker errors on macOS due to missing string_view symbols
// We disable string_view support in the header so the compiler doesn't try to
// use those overloads which might be missing from the linked library
#if defined(__APPLE__)
#define JSON_HAS_STRING_VIEW 0
#define JSON_USE_STRING_VIEW 0
#endif

#include <json/json.h>
#include <string>

namespace neutron {
namespace lsp {

// A wrapper type to ensure we pass keys as const char* to JsonCpp
// This helps avoid linker ambiguity with std::string vs string_view
struct JKey {
  std::string val;

  JKey(const char *s) : val(s) {}
  JKey(const std::string &s) : val(s) {}

  // Allow implicit conversion to const char* for JsonCpp APIs
  operator const char *() const { return val.c_str(); }

  // Helper to get std::string if needed
  std::string str() const { return val; }
};

} // namespace lsp
} // namespace neutron
