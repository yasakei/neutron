#include "server.h"
#include <algorithm>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <iomanip>

namespace neutron {
namespace lsp {

// Helper to decode percent-encoded URI characters
static std::string decodeURIComponent(const std::string& str) {
  std::string result;
  for (size_t i = 0; i < str.length(); ++i) {
    if (str[i] == '%' && i + 2 < str.length()) {
      std::string hex = str.substr(i + 1, 2);
      try {
        int value = std::stoi(hex, nullptr, 16);
        result += static_cast<char>(value);
        i += 2;
      } catch (...) {
        result += str[i];
      }
    } else {
      result += str[i];
    }
  }
  return result;
}

// Helper to safely access JSON values using string literals
static inline bool has(const Json::Value &v, const char *key) {
  return v.isMember(key);
}

static inline const Json::Value &get(const Json::Value &v, const char *key) {
  return v[key];
}

static inline Json::Value &set(Json::Value &v, const char *key) {
  return v[key];
}

LSPServer::LSPServer() : running(true) {}

std::string LSPServer::uriToPath(const std::string& uri) {
  // Convert file:// URI to filesystem path
  // Examples:
  //   file:///C:/path/file.nt -> C:/path/file.nt (Windows)
  //   file:///c%3A/path/file.nt -> c:\path\file.nt (Windows with encoded colon)
  //   file:///home/user/file.nt -> /home/user/file.nt (Unix)
  
  std::string path = uri;
  
  // Check if it starts with file://
  if (path.rfind("file://", 0) == 0) {
    path = path.substr(7); // Remove "file://"
    
    // Decode percent-encoded characters FIRST
    path = decodeURIComponent(path);
    
    // On Windows, remove leading slash if we have a drive letter
    // After decoding: /C:/path or /c:/path -> C:/path or c:/path
    #ifdef _WIN32
    if (path.length() >= 3 && path[0] == '/' && 
        ((path[1] >= 'a' && path[1] <= 'z') || (path[1] >= 'A' && path[1] <= 'Z')) &&
        path[2] == ':') {
      path = path.substr(1);
    }
    // Convert forward slashes to backslashes on Windows
    std::replace(path.begin(), path.end(), '/', '\\');
    #endif
  }
  
  return path;
}

void LSPServer::run() {
  std::cerr << "[LSP] Server started. Waiting for input..." << std::endl;
  // Basic LSP loop: read header, read content, process

  while (running && std::cin.good()) {
    std::string line;
    int contentLength = 0;

    // Read headers
    while (std::getline(std::cin, line)) {
      // Handle CR LF or just LF
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }

      if (line.empty()) {
        break; // End of headers
      }

      if (line.rfind("Content-Length: ", 0) == 0) {
        try {
          contentLength = std::stoi(line.substr(16));
        } catch (...) {
          std::cerr << "[LSP] Error parsing Content-Length from: " << line
                    << std::endl;
        }
      }
    }

    if (contentLength > 0) {
      std::vector<char> buffer(contentLength);
      std::cin.read(buffer.data(), contentLength);

      if (std::cin.gcount() != contentLength) {
        std::cerr << "[LSP] Failed to read " << contentLength << " bytes. Got "
                  << std::cin.gcount() << std::endl;
        break; // Read error
      }

      std::string content(buffer.begin(), buffer.end());
      std::cerr << "[LSP] Received Message: " << content << std::endl;

      Json::Value root;
      Json::Reader reader;
      if (reader.parse(content, root)) {
        handleMessage(root);
      } else {
        std::cerr << "[LSP] JSON Parse Error." << std::endl;
      }
    } else {
      // No content length? might be EOF or noise
      if (std::cin.eof()) {
        std::cerr << "[LSP] End of input stream." << std::endl;
        break;
      }
    }
  }
}

void LSPServer::handleMessage(const Json::Value &message) {
  if (has(message, "id")) {
    // Request or Response (we only handle incoming requests)
    if (has(message, "method")) {
      handleRequest(message);
    }
  } else {
    // Notification
    handleNotification(message);
  }
}

void LSPServer::handleRequest(const Json::Value &message) {
  std::string method = get(message, "method").asString();
  Json::Value id = get(message, "id"); // Preserve original type (int or string)

  std::cerr << "[LSP] Handling Request: " << method << " ["
            << id.toStyledString() << "]" << std::endl;

  if (method == "initialize") {
    onInitialize(get(message, "params"), id);
  } else if (method == "shutdown") {
    onShutdown(id);
  } else if (method == "textDocument/documentSymbol") {
    onDocumentSymbol(get(message, "params"), id);
  } else if (method == "textDocument/completion") {
    onCompletion(get(message, "params"), id);
  } else if (method == "textDocument/hover") {
    onHover(get(message, "params"), id);
  } else if (method == "textDocument/codeLens") {
    onCodeLens(get(message, "params"), id);
  } else if (method == "codeLens/resolve") {
    onCodeLensResolve(get(message, "params"), id);
  } else if (method == "textDocument/formatting") {
    onFormatting(get(message, "params"), id);
  } else {
    std::cerr << "[LSP] Unknown request: " << method << std::endl;
    // Return null result for unknown methods
    Json::Value response;
    set(response, "jsonrpc") = "2.0";
    set(response, "id") = id;
    set(response, "result") = Json::nullValue;
    sendJson(response);
  }
}

void LSPServer::handleNotification(const Json::Value &message) {
  std::string method = get(message, "method").asString();
  std::cerr << "[LSP] Handling Notification: " << method << std::endl;

  if (method == "initialized") {
    // Client is ready, we can send additional info if needed
    std::cerr << "[LSP] Client initialized successfully." << std::endl;
  } else if (method == "textDocument/didOpen") {
    onTextDocumentDidOpen(get(message, "params"));
  } else if (method == "textDocument/didChange") {
    onTextDocumentDidChange(get(message, "params"));
  } else if (method == "textDocument/didSave") {
    // Re-validate on save
    std::string uri =
        get(get(get(message, "params"), "textDocument"), "uri").asString();
    if (openDocuments.find(uri) != openDocuments.end()) {
      validateDocument(uri, openDocuments[uri]);
    }
  } else if (method == "textDocument/didClose") {
    std::string uri =
        get(get(get(message, "params"), "textDocument"), "uri").asString();
    openDocuments.erase(uri);
  } else if (method == "exit") {
    running = false;
  } else {
    std::cerr << "[LSP] Unknown notification: " << method << std::endl;
  }
}

void LSPServer::onInitialize(const Json::Value &params, const Json::Value &id) {
  (void)params; // Unused for now
  Json::Value result;
  Json::Value capabilities;
  Json::Value textDocumentSync;

  set(textDocumentSync, "openClose") = true;
  set(textDocumentSync, "change") =
      1; // Full sync (TextDocumentSyncKind.Full = 1)

  set(capabilities, "textDocumentSync") = textDocumentSync;
  set(capabilities, "documentSymbolProvider") = true;

  // Completion provider
  Json::Value completionProvider;
  set(completionProvider, "triggerCharacters") = Json::arrayValue;
  set(completionProvider, "triggerCharacters").append(".");
  set(capabilities, "completionProvider") = completionProvider;

  // Hover provider
  set(capabilities, "hoverProvider") = true;

  // CodeLens provider (for Run button)
  Json::Value codeLensProvider;
  set(codeLensProvider, "resolveProvider") = true;
  set(capabilities, "codeLensProvider") = codeLensProvider;

  // Document formatting provider
  set(capabilities, "documentFormattingProvider") = true;

  set(result, "capabilities") = capabilities;

  Json::Value response;
  set(response, "jsonrpc") = "2.0";
  set(response, "id") = id;
  set(response, "result") = result;

  sendJson(response);
}

void LSPServer::onShutdown(const Json::Value &id) {
  Json::Value response;
  set(response, "jsonrpc") = "2.0";
  set(response, "id") = id;
  set(response, "result") = Json::Value(Json::nullValue);

  sendJson(response);
}

void LSPServer::onTextDocumentDidOpen(const Json::Value &params) {
  std::string uri = get(get(params, "textDocument"), "uri").asString();
  std::string text = get(get(params, "textDocument"), "text").asString();

  openDocuments[uri] = text;
  validateDocument(uri, text);
}

void LSPServer::onTextDocumentDidChange(const Json::Value &params) {
  std::string uri = get(get(params, "textDocument"), "uri").asString();
  // Assuming Full sync for now
  if (get(params, "contentChanges").size() > 0) {
    std::string text = get(get(params, "contentChanges")[0], "text").asString();
    openDocuments[uri] = text;
    validateDocument(uri, text);
  }
}

void LSPServer::validateDocument(const std::string &uri,
                                 const std::string &text) {
  std::vector<Diagnostic> diagnostics;

  // Reset previous state first
  ErrorHandler::reset();

  // Configure ErrorHandler with source for column detection
  std::vector<std::string> lines;
  std::stringstream ss(text);
  std::string line;
  while (std::getline(ss, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    lines.push_back(line);
  }
  ErrorHandler::setSourceLines(lines);
  
  // Convert URI to filesystem path for error reporting
  std::string filePath = uriToPath(uri);
  ErrorHandler::setCurrentFile(filePath);

  // Set the error callback to capture diagnostics
  ErrorHandler::setErrorCallback([&](const ErrorInfo &error) {
    Diagnostic d;
    d.message = error.message;

    // Map Neutron ErrorType to Severity
    if (error.type == ErrorType::SYNTAX_ERROR ||
        error.type == ErrorType::LEXICAL_ERROR) {
      d.severity = DiagnosticSeverity::Error;
    } else {
      d.severity = DiagnosticSeverity::Warning;
    }

    int line = error.line > 0 ? error.line - 1 : 0;
    int col = error.column > 0 ? error.column : 0;

    d.range.start.line = line;
    d.range.start.character = col;
    d.range.end.line = line;
    // Heuristic: highlight 1 char or rest of line if generic
    d.range.end.character = col + 1;

    d.source = "neutron";
    diagnostics.push_back(d);
  });

  try {
    Scanner scanner(text);
    std::vector<Token> tokens = scanner.scanTokens();

    Parser parser(tokens);
    parser.parse();
  } catch (...) {
    // Catch any stray exceptions
  }

  // Clear callback
  ErrorHandler::setErrorCallback(nullptr);

  // Additional validation for .ntsc files and safe {} blocks
  bool isNtscFile = (uri.find(".ntsc") != std::string::npos);

  // Track which lines are inside safe {} blocks
  std::vector<bool> lineInSafeBlock(lines.size(), false);

  // First pass: identify which lines are inside safe {} blocks
  if (!isNtscFile) {
    int braceDepth = 0;
    bool inSafe = false;
    int safeStartBrace = 0;

    for (size_t i = 0; i < lines.size(); i++) {
      const std::string &lineText = lines[i];

      // Check for "safe {" or "safe\n{"
      if (lineText.find("safe") != std::string::npos) {
        size_t safePos = lineText.find("safe");
        // Check it's not part of another word
        bool validSafe = true;
        if (safePos > 0 &&
            (isalnum(lineText[safePos - 1]) || lineText[safePos - 1] == '_')) {
          validSafe = false;
        }
        if (safePos + 4 < lineText.length() &&
            (isalnum(lineText[safePos + 4]) || lineText[safePos + 4] == '_')) {
          validSafe = false;
        }
        if (validSafe) {
          inSafe = true;
          safeStartBrace = braceDepth;
        }
      }

      // Count braces
      bool inString = false;
      for (size_t j = 0; j < lineText.length(); j++) {
        char c = lineText[j];
        if (c == '"' || c == '\'')
          inString = !inString;
        if (!inString) {
          if (c == '{') {
            braceDepth++;
            if (inSafe && braceDepth == safeStartBrace + 1) {
              // This is the opening brace of safe block
            }
          } else if (c == '}') {
            braceDepth--;
            if (inSafe && braceDepth == safeStartBrace) {
              inSafe = false;
            }
          }
        }
      }

      lineInSafeBlock[i] = inSafe;
    }
  }

  // Check for missing type annotations
  std::regex varNoType(R"(^\s*var\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=)");
  std::regex varWithType(
      R"(^\s*var\s+(int|float|string|bool|array|object|any)\s+)");
  std::regex funNoReturnType(
      R"(^\s*fun\s+[a-zA-Z_][a-zA-Z0-9_]*\s*\([^)]*\)\s*\{)");
  std::regex funWithReturnType(
      R"(^\s*fun\s+[a-zA-Z_][a-zA-Z0-9_]*\s*\([^)]*\)\s*->\s*(int|float|string|bool|array|object|any)\s*\{)");

  for (size_t i = 0; i < lines.size(); i++) {
    const std::string &lineText = lines[i];
    bool requireTypes = isNtscFile || lineInSafeBlock[i];

    if (!requireTypes)
      continue;

    std::smatch match;
    std::string context = isNtscFile ? ".ntsc files" : "safe blocks";

    // Check for var without type: "var x = " but not "var int x = "
    if (std::regex_search(lineText, match, varNoType)) {
      // Make sure it's not a typed var
      if (!std::regex_search(lineText, varWithType)) {
        Diagnostic d;
        d.message = "Variable '" + match[1].str() +
                    "' must have a type annotation in " + context +
                    " (e.g., var int " + match[1].str() + " = ...)";
        d.severity = DiagnosticSeverity::Error;
        d.range.start.line = static_cast<int>(i);
        d.range.start.character = static_cast<int>(lineText.find("var"));
        d.range.end.line = static_cast<int>(i);
        d.range.end.character = static_cast<int>(lineText.find("var") + 3);
        d.source = "neutron";
        diagnostics.push_back(d);
      }
    }

    // Check for function without return type
    if (std::regex_search(lineText, funNoReturnType) &&
        !std::regex_search(lineText, funWithReturnType)) {
      // Extract function name
      std::regex funName(R"(fun\s+([a-zA-Z_][a-zA-Z0-9_]*))");
      if (std::regex_search(lineText, match, funName)) {
        Diagnostic d;
        d.message = "Function '" + match[1].str() +
                    "' must have a return type annotation in " + context +
                    " (e.g., fun " + match[1].str() + "(...) -> int { ... })";
        d.severity = DiagnosticSeverity::Error;
        d.range.start.line = static_cast<int>(i);
        d.range.start.character = static_cast<int>(lineText.find("fun"));
        d.range.end.line = static_cast<int>(i);
        d.range.end.character = static_cast<int>(lineText.find("{"));
        d.source = "neutron";
        diagnostics.push_back(d);
      }
    }
  }

  publishDiagnostics(uri, diagnostics);
}

void LSPServer::publishDiagnostics(const std::string &uri,
                                   const std::vector<Diagnostic> &diagnostics) {
  Json::Value params;
  set(params, "uri") = uri;

  Json::Value diagArray = Json::arrayValue;
  for (const auto &d : diagnostics) {
    diagArray.append(d.toJson());
  }
  set(params, "diagnostics") = diagArray;

  Json::Value notification;
  set(notification, "jsonrpc") = "2.0";
  set(notification, "method") = "textDocument/publishDiagnostics";
  set(notification, "params") = params;

  sendJson(notification);
}

void LSPServer::onDocumentSymbol(const Json::Value &params,
                                 const Json::Value &id) {
  std::string uri = get(get(params, "textDocument"), "uri").asString();
  Json::Value result = Json::arrayValue;

  if (openDocuments.find(uri) != openDocuments.end()) {
    std::string text = openDocuments[uri];

    ErrorHandler::setErrorCallback([](const ErrorInfo &) {});

    try {
      Scanner scanner(text);
      std::vector<Token> tokens = scanner.scanTokens();
      Parser parser(tokens);
      auto statements = parser.parse();
      collectSymbols(statements, result);
    } catch (...) {
    }

    ErrorHandler::setErrorCallback(nullptr);
  }

  Json::Value response;
  set(response, "jsonrpc") = "2.0";
  set(response, "id") = id;
  set(response, "result") = result;

  sendJson(response);
}

void LSPServer::collectSymbols(
    const std::vector<std::unique_ptr<Stmt>> &statements,
    Json::Value &symbols) {
  for (const auto &stmt : statements) {
    if (!stmt)
      continue;

    Json::Value symbol;
    bool isSymbol = false;

    if (stmt->type == StmtType::FUNCTION) {
      const auto *func = static_cast<const FunctionStmt *>(stmt.get());
      set(symbol, "name") = func->name.lexeme;
      set(symbol, "kind") = 12; // Function
      int line = func->name.line > 0 ? func->name.line - 1 : 0;

      Json::Value range;
      set(set(range, "start"), "line") = line;
      set(set(range, "start"), "character") = 0;
      set(set(range, "end"), "line") = line;
      set(set(range, "end"), "character") = 10; // arbitrary

      set(set(symbol, "location"), "uri") = "";

      set(symbol, "range") = range;
      set(symbol, "selectionRange") = range;

      isSymbol = true;
    } else if (stmt->type == StmtType::CLASS) {
      const auto *cls = static_cast<const ClassStmt *>(stmt.get());
      set(symbol, "name") = cls->name.lexeme;
      set(symbol, "kind") = 5; // Class
      int line = cls->name.line > 0 ? cls->name.line - 1 : 0;

      Json::Value range;
      set(set(range, "start"), "line") = line;
      set(set(range, "start"), "character") = 0;
      set(set(range, "end"), "line") = line;
      set(set(range, "end"), "character") = 10;

      set(symbol, "range") = range;
      set(symbol, "selectionRange") = range;

      isSymbol = true;
    }

    if (isSymbol) {
      symbols.append(symbol);
    }
  }
}

void LSPServer::onCompletion(const Json::Value &params, const Json::Value &id) {
  Json::Value result = Json::arrayValue;

  std::string uri = get(get(params, "textDocument"), "uri").asString();
  int line = get(get(params, "position"), "line").asInt();
  int character = get(get(params, "position"), "character").asInt();

  std::string documentText;
  std::string currentLine;

  if (openDocuments.find(uri) != openDocuments.end()) {
    documentText = openDocuments[uri];

    // Get the current line
    std::vector<std::string> lines;
    std::stringstream ss(documentText);
    std::string lineStr;
    while (std::getline(ss, lineStr)) {
      if (!lineStr.empty() && lineStr.back() == '\r')
        lineStr.pop_back();
      lines.push_back(lineStr);
    }

    if (line >= 0 && line < (int)lines.size()) {
      currentLine = lines[line];
      if (character > 0 && character <= (int)currentLine.length()) {
        currentLine = currentLine.substr(0, character);
      }
    }
  }

  // Check which modules are imported
  std::set<std::string> importedModules;
  std::regex useRegex(R"(use\s+(\w+))");
  std::smatch match;
  std::string::const_iterator searchStart(documentText.cbegin());
  while (std::regex_search(searchStart, documentText.cend(), match, useRegex)) {
    importedModules.insert(match[1].str());
    searchStart = match.suffix().first;
  }

  // Check if we're completing after a dot (e.g., "sys.", "hello"., myVar.)
  std::string dotContext;
  std::regex dotRegex(R"((\w+|"[^"]*")\.\s*$)");
  if (std::regex_search(currentLine, match, dotRegex)) {
    dotContext = match[1].str();
  }

  // Check if we're completing after a string literal
  std::regex stringLiteralRegex(R"("[^"]*"\.\s*$)");
  bool isStringLiteral = std::regex_search(currentLine, stringLiteralRegex);

  // Module function definitions
  struct ModuleFuncs {
    const char *name;
    std::vector<std::pair<const char *, const char *>> funcs; // {name, detail}
  };

  std::vector<ModuleFuncs> allModules = {
      {"sys",
       {{"read", "Read file contents"},
        {"write", "Write to file"},
        {"append", "Append to file"},
        {"exists", "Check if path exists"},
        {"mkdir", "Create directory"},
        {"rmdir", "Remove directory"},
        {"listdir", "List directory contents"},
        {"cwd", "Get current working directory"},
        {"chdir", "Change directory"},
        {"env", "Get environment variable"},
        {"args", "Get command line args"},
        {"input", "Read user input"},
        {"exit", "Exit program"},
        {"exec", "Execute command"},
        {"alloc", "Allocate memory"},
        {"cp", "Copy file"},
        {"mv", "Move file"},
        {"rm", "Remove file"}}},
      {"json",
       {{"stringify", "Convert to JSON string"},
        {"parse", "Parse JSON string"},
        {"get", "Get JSON value"}}},
      {"math",
       {{"add", "Add numbers"},
        {"subtract", "Subtract numbers"},
        {"multiply", "Multiply numbers"},
        {"divide", "Divide numbers"},
        {"pow", "Power"},
        {"sqrt", "Square root"},
        {"abs", "Absolute value"},
        {"sin", "Sine"},
        {"cos", "Cosine"},
        {"tan", "Tangent"},
        {"floor", "Floor"},
        {"ceil", "Ceiling"},
        {"round", "Round"},
        {"min", "Minimum"},
        {"max", "Maximum"},
        {"random", "Random number"}}},
      {"fmt",
       {{"to_int", "Convert to integer"},
        {"to_str", "Convert to string"},
        {"to_float", "Convert to float"},
        {"to_bin", "Convert to binary"},
        {"type", "Get type name"}}},
      {"arrays",
       {{"new", "Create new array"},
        {"length", "Get array length"},
        {"push", "Add element"},
        {"pop", "Remove last element"},
        {"at", "Get element at index"},
        {"set", "Set element at index"},
        {"slice", "Get slice of array"},
        {"join", "Join elements"},
        {"reverse", "Reverse array"},
        {"sort", "Sort array"},
        {"index_of", "Find index of element"},
        {"contains", "Check if contains"},
        {"remove", "Remove element"},
        {"clear", "Clear array"},
        {"clone", "Clone array"},
        {"flat", "Flatten array"},
        {"fill", "Fill array"},
        {"range", "Create range"},
        {"shuffle", "Shuffle array"}}},
      {"time",
       {{"now", "Get current timestamp"},
        {"format", "Format time"},
        {"sleep", "Sleep for milliseconds"}}},
      {"http",
       {{"get", "HTTP GET request"},
        {"post", "HTTP POST request"},
        {"put", "HTTP PUT request"},
        {"delete", "HTTP DELETE request"},
        {"head", "HTTP HEAD request"},
        {"patch", "HTTP PATCH request"}}},
      {"regex",
       {{"test", "Test if pattern matches"},
        {"search", "Search for pattern"},
        {"find", "Find first match"},
        {"findAll", "Find all matches"},
        {"replace", "Replace matches"},
        {"split", "Split by pattern"},
        {"isValid", "Check if pattern is valid"},
        {"escape", "Escape special characters"}}},
      {"async",
       {{"run", "Run async function"},
        {"await", "Await async result"},
        {"sleep", "Async sleep"},
        {"timer", "Create timer"}}},
      {"process",
       {{"spawn", "Spawn new process"},
        {"send", "Send message to process"},
        {"receive", "Receive message"},
        {"self", "Get current process"},
        {"is_alive", "Check if process is alive"},
        {"kill", "Kill process"}}},
      {"crypto",
       {{"base64_encode", "Encode string to Base64"},
        {"base64_decode", "Decode Base64 string"},
        {"sha256", "Calculate SHA-256 hash"},
        {"hex_encode", "Encode string to hexadecimal"},
        {"hex_decode", "Decode hexadecimal string"},
        {"random_bytes", "Generate random bytes"},
        {"random_string", "Generate random string"},
        {"xor_cipher", "XOR cipher encryption/decryption"}}},
      {"path",
       {{"join", "Join path segments"},
        {"split", "Split path into directory and filename"},
        {"dirname", "Get directory portion of path"},
        {"basename", "Get filename portion of path"},
        {"extname", "Get file extension"},
        {"isabs", "Check if path is absolute"},
        {"normalize", "Normalize path"},
        {"resolve", "Convert to absolute path"},
        {"relative", "Get relative path between two paths"},
        {"toUnix", "Convert to Unix-style path"},
        {"toWindows", "Convert to Windows-style path"},
        {"sep", "Platform path separator"},
        {"delimiter", "Platform path delimiter"}}},
      {"random",
       {{"random", "Generate random float between 0.0 and 1.0"},
        {"uniform", "Generate random float between a and b"},
        {"randint", "Generate random integer between a and b (inclusive)"},
        {"choice", "Choose random element from array"},
        {"shuffle", "Shuffle array in-place"},
        {"sample", "Get k random elements without replacement"},
        {"seed", "Set random seed (optional argument)"},
        {"gauss", "Generate random number from Gaussian distribution"},
        {"expovariate", "Generate random number from exponential distribution"},
        {"triangular", "Generate random number from triangular distribution"},
        {"getrandbits", "Generate random integer with k random bits"}}}};

  // Check if the dot context is a known imported module
  bool isModuleCompletion = false;
  if (!dotContext.empty() && !isStringLiteral && importedModules.count(dotContext)) {
    isModuleCompletion = true;
  }

  // If it's a module completion, provide module functions
  if (isModuleCompletion) {
    for (const auto &mod : allModules) {
      if (dotContext == mod.name) {
        for (const auto &func : mod.funcs) {
          Json::Value item;
          set(item, "label") = func.first;
          set(item, "kind") = 3; // Function
          set(item, "detail") = func.second;
          set(item, "insertText") = std::string(func.first) + "($1)";
          set(item, "insertTextFormat") = 2; // Snippet
          result.append(item);
        }
        break;
      }
    }

    Json::Value response;
    set(response, "jsonrpc") = "2.0";
    set(response, "id") = id;
    set(response, "result") = result;
    sendJson(response);
    return;
  }

  // If we have a dot context but it's not a known module, assume it could be a string variable
  // This includes string literals and any variable that might be a string
  bool isStringCompletion = false;
  std::string variableName;
  if (!dotContext.empty()) {
    if (isStringLiteral) {
      isStringCompletion = true;
    } else {
      // For variables, try to determine if they might be strings by analyzing the document
      variableName = dotContext;
      
      // Always provide string completions for variables - this is optimistic but helpful
      // since we don't have full type inference in the LSP
      isStringCompletion = true;
      
      // Optional: Try to be smarter by looking for string-like patterns in the document
      // Check if the variable is assigned a string literal or string method result
      std::string varPattern = variableName + R"(\s*=\s*("[^"]*"|.*\.(upper|lower|strip|replace|substring)\(\)))";
      std::regex stringAssignmentRegex(varPattern);
      if (std::regex_search(documentText, stringAssignmentRegex)) {
        // High confidence this is a string variable
        isStringCompletion = true;
      }
      
      // Check if variable has string type annotation
      std::string typePattern = R"(var\s+string\s+)" + variableName;
      std::regex stringTypeRegex(typePattern);
      if (std::regex_search(documentText, stringTypeRegex)) {
        // Definitely a string variable
        isStringCompletion = true;
      }
    }
  }

  // If we're completing after a string (e.g., "hello"., str.)
  if (isStringCompletion) {
    // String properties (no parentheses)
    struct StringProperty {
      const char* name;
      const char* detail;
      const char* documentation;
    };
    
    StringProperty stringProperties[] = {
      {"length", "Number of characters", "Returns the number of characters in the string"},
      {"chars", "Array of characters", "Returns an array of individual characters"}
    };
    
    for (const auto& prop : stringProperties) {
      Json::Value item;
      set(item, "label") = prop.name;
      set(item, "kind") = 10; // Property
      set(item, "detail") = prop.detail;
      set(item, "documentation") = prop.documentation;
      set(item, "insertText") = prop.name;
      result.append(item);
    }

    // String methods (with parentheses)
    struct StringMethod {
      const char* name;
      const char* signature;
      const char* detail;
      const char* documentation;
    };
    
    StringMethod stringMethods[] = {
      // Basic methods
      {"contains", "contains(substring)", "Check if string contains substring", "Returns true if the string contains the specified substring"},
      {"split", "split(delimiter)", "Split string into array", "Splits the string into an array using the specified delimiter"},
      {"substring", "substring(start, [end])", "Extract substring", "Returns a portion of the string from start to end"},
      {"strip", "strip([chars])", "Remove whitespace", "Removes whitespace or specified characters from both ends"},
      {"lstrip", "lstrip([chars])", "Remove left whitespace", "Removes whitespace or specified characters from the left end"},
      {"rstrip", "rstrip([chars])", "Remove right whitespace", "Removes whitespace or specified characters from the right end"},
      {"replace", "replace(old, new, [count])", "Replace substring", "Replaces occurrences of old substring with new substring"},
      
      // Search methods
      {"find", "find(substring, [start], [end])", "Find substring index", "Returns the index of the first occurrence of substring, -1 if not found"},
      {"rfind", "rfind(substring, [start], [end])", "Find last substring index", "Returns the index of the last occurrence of substring, -1 if not found"},
      {"index", "index(substring, [start], [end])", "Find substring (throws error)", "Like find() but throws an error if substring is not found"},
      {"rindex", "rindex(substring, [start], [end])", "Find last substring (throws error)", "Like rfind() but throws an error if substring is not found"},
      
      // Case conversion (Unicode-aware)
      {"upper", "upper()", "Convert to uppercase", "Converts the string to uppercase (Unicode-aware, supports é → É)"},
      {"lower", "lower()", "Convert to lowercase", "Converts the string to lowercase (Unicode-aware, supports É → é)"},
      {"capitalize", "capitalize()", "Capitalize first character", "Capitalizes the first character and lowercases the rest"},
      {"title", "title()", "Convert to title case", "Converts the string to title case (first letter of each word capitalized)"},
      {"swapcase", "swapcase()", "Swap character case", "Swaps the case of all characters (upper becomes lower, lower becomes upper)"},
      {"casefold", "casefold()", "Aggressive lowercase", "Converts to lowercase for case-insensitive comparisons"},
      
      // Character classification
      {"isalnum", "isalnum()", "Check if alphanumeric", "Returns true if all characters are alphanumeric (letters or digits)"},
      {"isalpha", "isalpha()", "Check if alphabetic", "Returns true if all characters are alphabetic (letters)"},
      {"isdigit", "isdigit()", "Check if digits", "Returns true if all characters are digits"},
      {"islower", "islower()", "Check if lowercase", "Returns true if all cased characters are lowercase"},
      {"isupper", "isupper()", "Check if uppercase", "Returns true if all cased characters are uppercase"},
      {"isspace", "isspace()", "Check if whitespace", "Returns true if all characters are whitespace"},
      {"istitle", "istitle()", "Check if title case", "Returns true if the string is in title case"},
      
      // Sequence operations
      {"slice", "slice(start, [end], [step])", "Advanced slicing", "Returns a slice of the string with optional step parameter"},
      
      // Unicode methods
      {"normalize", "normalize(form)", "Unicode normalization", "Normalizes Unicode text (forms: NFC, NFD, NFKC, NFKD)"},
      {"encode", "encode(encoding)", "Encode to bytes", "Encodes the string using the specified encoding (utf-8, utf-16, ascii)"},
      {"isunicode", "isunicode()", "Check if contains Unicode", "Returns true if the string contains Unicode characters"},
      
      // Formatting
      {"format", "format(args...)", "Format string", "Formats the string with positional arguments using {0}, {1}, etc."}
    };
    
    for (const auto& method : stringMethods) {
      Json::Value item;
      set(item, "label") = method.name;
      set(item, "kind") = 2; // Method
      set(item, "detail") = method.signature;
      set(item, "documentation") = method.documentation;
      
      // Create snippet with appropriate parameters
      std::string snippet = std::string(method.name) + "(";
      if (strstr(method.name, "contains") || strstr(method.name, "find") || 
          strstr(method.name, "index") || strstr(method.name, "replace")) {
        snippet += "$1";
      }
      snippet += ")";
      
      set(item, "insertText") = snippet;
      set(item, "insertTextFormat") = 2; // Snippet
      result.append(item);
    }

    Json::Value response;
    set(response, "jsonrpc") = "2.0";
    set(response, "id") = id;
    set(response, "result") = result;
    sendJson(response);
    return;
  }

  // Check if we might be completing array methods
  bool isArrayCompletion = false;
  if (!dotContext.empty() && !isStringCompletion) {
    // Check for array patterns in the document
    std::string arrayPattern = dotContext + R"(\s*=\s*(\[.*\]|.*\.split\(|.*\.chars))";
    std::regex arrayAssignmentRegex(arrayPattern);
    if (std::regex_search(documentText, arrayAssignmentRegex)) {
      isArrayCompletion = true;
    }
    
    // Check if variable has array type annotation
    std::string arrayTypePattern = R"(var\s+array\s+)" + dotContext;
    std::regex arrayTypeRegex(arrayTypePattern);
    if (std::regex_search(documentText, arrayTypeRegex)) {
      isArrayCompletion = true;
    }
  }

  // If we're completing after an array variable
  if (isArrayCompletion) {
    // Array property
    Json::Value lengthProp;
    set(lengthProp, "label") = "length";
    set(lengthProp, "kind") = 10; // Property
    set(lengthProp, "detail") = "Number of elements";
    set(lengthProp, "documentation") = "Returns the number of elements in the array";
    set(lengthProp, "insertText") = "length";
    result.append(lengthProp);

    // Array methods
    struct ArrayMethod {
      const char* name;
      const char* signature;
      const char* detail;
    };
    
    ArrayMethod arrayMethods[] = {
      {"push", "push(element)", "Add element to end"},
      {"pop", "pop()", "Remove and return last element"},
      {"slice", "slice(start, [end])", "Get portion of array"},
      {"map", "map(function)", "Transform each element"},
      {"filter", "filter(function)", "Filter elements"},
      {"find", "find(function)", "Find first matching element"},
      {"indexOf", "indexOf(element)", "Get index of element"},
      {"join", "join(separator)", "Join elements into string"},
      {"reverse", "reverse()", "Reverse array in place"},
      {"sort", "sort()", "Sort array in place"}
    };
    
    for (const auto& method : arrayMethods) {
      Json::Value item;
      set(item, "label") = method.name;
      set(item, "kind") = 2; // Method
      set(item, "detail") = method.signature;
      set(item, "documentation") = method.detail;
      
      std::string snippet = std::string(method.name) + "(";
      if (strcmp(method.name, "push") == 0 || strcmp(method.name, "indexOf") == 0 || 
          strcmp(method.name, "join") == 0) {
        snippet += "$1";
      }
      snippet += ")";
      
      set(item, "insertText") = snippet;
      set(item, "insertTextFormat") = 2; // Snippet
      result.append(item);
    }

    Json::Value response;
    set(response, "jsonrpc") = "2.0";
    set(response, "id") = id;
    set(response, "result") = result;
    sendJson(response);
    return;
  }

  // General completions (not after a dot)

  // Keywords
  const char *keywords[] = {"var",      "fun",     "class",  "if",      "else",
                            "while",    "for",     "do",     "return",  "break",
                            "continue", "match",   "case",   "default", "try",
                            "catch",    "finally", "throw",  "retry",   "use",
                            "using",    "from",    "static", "safe",    "true",
                            "false",    "nil",     "this",   "super",   "init"};
  for (const char *kw : keywords) {
    Json::Value item;
    set(item, "label") = kw;
    set(item, "kind") = 14; // Keyword
    set(item, "detail") = "keyword";
    result.append(item);
  }

  // Types
  const char *types[] = {"int",   "float",  "string", "bool",
                         "array", "object", "any"};
  for (const char *t : types) {
    Json::Value item;
    set(item, "label") = t;
    set(item, "kind") = 25; // Type parameter
    set(item, "detail") = "type";
    result.append(item);
  }

  // Built-in modules (always suggest for use statements)
  const char *modules[] = {"sys",  "json", "math",  "fmt",   "arrays",
                           "time", "http", "regex", "async", "process",
                           "crypto", "path", "random"};
  for (const char *m : modules) {
    Json::Value item;
    set(item, "label") = m;
    set(item, "kind") = 9; // Module
    set(item, "detail") = "module";
    result.append(item);
  }

  // Built-in function
  Json::Value sayItem;
  set(sayItem, "label") = "say";
  set(sayItem, "kind") = 3; // Function
  set(sayItem, "detail") = "Print to console";
  set(sayItem, "insertText") = "say($1)";
  set(sayItem, "insertTextFormat") = 2; // Snippet
  result.append(sayItem);

  // Check if we're in a safe context (inside safe{} block or .ntsc file)
  bool inSafeContext = (uri.find(".ntsc") != std::string::npos);
  if (!inSafeContext) {
    // Check for safe { block
    std::regex safeBlockRegex(R"(safe\s*\{)");
    if (std::regex_search(documentText, safeBlockRegex)) {
      // Simple check - if there's a safe block in the file, suggest typed
      // completions
      inSafeContext = true;
    }
  }

  // Add type-safe snippets (especially useful in safe contexts)
  if (inSafeContext) {
    // Typed variable declarations
    const char *typeNames[] = {"int",   "float",  "string", "bool",
                               "array", "object", "any"};
    for (const char *t : typeNames) {
      Json::Value item;
      set(item, "label") = std::string("var ") + t;
      set(item, "kind") = 15; // Snippet
      set(item, "detail") = "Typed variable declaration";
      set(item, "insertText") =
          std::string("var ") + t + " ${1:name} = ${2:value};";
      set(item, "insertTextFormat") = 2; // Snippet
      result.append(item);
    }

    // Typed function with return type
    Json::Value typedFun;
    set(typedFun, "label") = "fun (typed)";
    set(typedFun, "kind") = 15; // Snippet
    set(typedFun, "detail") = "Function with type annotations";
    set(typedFun, "insertText") =
        "fun ${1:name}(${2:type} ${3:param}) -> ${4:returnType} {\n\t$0\n}";
    set(typedFun, "insertTextFormat") = 2;
    result.append(typedFun);

    // Typed class
    Json::Value typedClass;
    set(typedClass, "label") = "class (typed)";
    set(typedClass, "kind") = 15; // Snippet
    set(typedClass, "detail") = "Class with type annotations";
    set(typedClass, "insertText") =
        "class ${1:Name} {\n\tvar ${2:type} ${3:field};\n\t\n\tfun "
        "init(${4:type} ${5:param}) -> int {\n\t\tthis.${3:field} = "
        "${5:param};\n\t\treturn 0;\n\t}\n}";
    set(typedClass, "insertTextFormat") = 2;
    result.append(typedClass);
  }

  // Safe block snippet
  Json::Value safeBlock;
  set(safeBlock, "label") = "safe";
  set(safeBlock, "kind") = 15; // Snippet
  set(safeBlock, "detail") = "Type-safe code block";
  set(safeBlock, "documentation") =
      "Enforces type annotations on all variables, functions, and classes "
      "within the block";
  set(safeBlock, "insertText") = "safe {\n\t$0\n}";
  set(safeBlock, "insertTextFormat") = 2;
  result.append(safeBlock);

  // Only suggest module functions for imported modules
  for (const auto &mod : allModules) {
    if (importedModules.count(mod.name)) {
      for (const auto &func : mod.funcs) {
        Json::Value item;
        set(item, "label") = std::string(mod.name) + "." + func.first;
        set(item, "kind") = 3; // Function
        set(item, "detail") = func.second;
        set(item, "insertText") =
            std::string(mod.name) + "." + func.first + "($1)";
        set(item, "insertTextFormat") = 2; // Snippet
        result.append(item);
      }
    }
  }

  Json::Value response;
  set(response, "jsonrpc") = "2.0";
  set(response, "id") = id;
  set(response, "result") = result;

  sendJson(response);
}

void LSPServer::onHover(const Json::Value &params, const Json::Value &id) {
  std::string uri = get(get(params, "textDocument"), "uri").asString();
  int line = get(get(params, "position"), "line").asInt();
  int character = get(get(params, "position"), "character").asInt();

  Json::Value result = Json::nullValue;

  if (openDocuments.find(uri) != openDocuments.end()) {
    std::string text = openDocuments[uri];

    // Split into lines
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string lineStr;
    while (std::getline(ss, lineStr)) {
      if (!lineStr.empty() && lineStr.back() == '\r')
        lineStr.pop_back();
      lines.push_back(lineStr);
    }

    if (line >= 0 && line < static_cast<int>(lines.size())) {
      std::string &currentLine = lines[line];

      // Find word at position
      int start = character;
      int end = character;
      while (start > 0 &&
             (isalnum(currentLine[start - 1]) ||
              currentLine[start - 1] == '_' || currentLine[start - 1] == '.'))
        start--;
      while (end < static_cast<int>(currentLine.length()) &&
             (isalnum(currentLine[end]) || currentLine[end] == '_' ||
              currentLine[end] == '.'))
        end++;

      std::string word = currentLine.substr(start, end - start);

      // Check if in .ntsc file
      bool isNtscFile = (uri.find(".ntsc") != std::string::npos);

      // Hover documentation
      std::string doc;
      if (word == "say")
        doc = "**say(value)**\n\nPrints value to console with newline.";
      else if (word == "var") {
        if (isNtscFile) {
          doc = "**var**\n\nDeclare a typed variable (required in .ntsc "
                "files).\n```neutron\nvar int x = 10;\nvar string name = "
                "\"Alice\";\n```";
        } else {
          doc = "**var**\n\nDeclare a variable.\n```neutron\nvar x = 10;\nvar "
                "int y = 20;  // with type\n```";
        }
      } else if (word == "fun") {
        if (isNtscFile) {
          doc = "**fun**\n\nDeclare a typed function (parameter and return "
                "types required in .ntsc files).\n```neutron\nfun add(int a, "
                "int b) -> int {\n    return a + b;\n}\n```";
        } else {
          doc = "**fun**\n\nDeclare a function.\n```neutron\nfun add(a, b) {\n "
                "   return a + b;\n}\n```";
        }
      } else if (word == "class") {
        if (isNtscFile) {
          doc = "**class**\n\nDeclare a typed class (all properties and "
                "methods must have types in .ntsc files).\n```neutron\nclass "
                "Person {\n    var string name;\n    fun init(string n) -> int "
                "{\n        this.name = n;\n        return 0;\n    }\n}\n```";
        } else {
          doc = "**class**\n\nDeclare a class.\n```neutron\nclass Person {\n   "
                " var name;\n    init(n) { this.name = n; }\n}\n```";
        }
      } else if (word == "safe")
        doc =
            "**safe**\n\nType-safe code block. All variables, functions, and "
            "classes inside must have type annotations.\n```neutron\nsafe {\n  "
            "  var int x = 10;\n    fun add(int a, int b) -> int {\n        "
            "return a + b;\n    }\n}\n```\n\n**Requirements inside safe "
            "block:**\n- Variables must have type: `var int x = 10;`\n- "
            "Functions must have parameter types and return type: `fun add(int "
            "a, int b) -> int`\n- Class properties and methods must be typed";
      else if (word == "use")
        doc = "**use**\n\nImport a built-in module.\n```neutron\nuse sys;\nuse "
              "json;\n```";
      else if (word == "using")
        doc = "**using**\n\nImport a Neutron source file.\n```neutron\nusing "
              "'utils.nt';\n```";
      else if (word == "sys")
        doc = "**sys module**\n\nSystem operations: file I/O, environment, "
              "process control.\n\nFunctions: `read`, `write`, `exists`, "
              "`mkdir`, `cwd`, `env`, `args`, `exec`, `exit`";
      else if (word == "json")
        doc = "**json module**\n\nJSON parsing and "
              "serialization.\n\nFunctions: `stringify`, `parse`, `get`";
      else if (word == "math")
        doc = "**math module**\n\nMathematical operations.\n\nFunctions: "
              "`add`, `subtract`, `multiply`, `divide`, `pow`, `sqrt`, `abs`";
      else if (word == "arrays")
        doc = "**arrays module**\n\nArray manipulation.\n\nFunctions: `push`, "
              "`pop`, `slice`, `join`, `sort`, `reverse`, `contains`, `range`";
      else if (word == "time")
        doc = "**time module**\n\nTime operations.\n\nFunctions: `now`, "
              "`format`, `sleep`";
      else if (word == "http")
        doc = "**http module**\n\nHTTP client.\n\nFunctions: `get`, `post`, "
              "`put`, `delete`, `head`, `patch`";
      else if (word == "regex")
        doc = "**regex module**\n\nRegular expressions.\n\nFunctions: `test`, "
              "`search`, `find`, `findAll`, `replace`, `split`";
      else if (word == "try")
        doc = "**try/catch/finally**\n\nException handling.\n```neutron\ntry "
              "{\n    riskyOp();\n} catch (e) {\n    say(e);\n} finally {\n    "
              "cleanup();\n}\n```";
      else if (word == "match")
        doc = "**match**\n\nPattern matching (switch-like).\n```neutron\nmatch "
              "(x) {\n    case 1 => say(\"one\");\n    case 2 => "
              "say(\"two\");\n    default => say(\"other\");\n}\n```";
      else if (word == "retry")
        doc =
            "**retry**\n\nRetry a block multiple times.\n```neutron\nretry (3) "
            "{\n    connect();\n} catch (e) {\n    say(\"Failed\");\n}\n```";
      // Type keywords
      else if (word == "int")
        doc = "**int**\n\nInteger type.\n```neutron\nvar int x = 42;\nfun "
              "getValue() -> int { return 10; }\n```";
      else if (word == "float")
        doc = "**float**\n\nFloating-point number type.\n```neutron\nvar float "
              "pi = 3.14159;\n```";
      else if (word == "string")
        doc = "**string**\n\nString type.\n```neutron\nvar string name = "
              "\"Alice\";\n```";
      else if (word == "bool")
        doc = "**bool**\n\nBoolean type.\n```neutron\nvar bool active = "
              "true;\n```";
      else if (word == "array")
        doc = "**array**\n\nArray type.\n```neutron\nvar array items = [1, 2, "
              "3];\n```";
      else if (word == "object")
        doc = "**object**\n\nObject/dictionary type.\n```neutron\nvar object "
              "data = {\"key\": \"value\"};\n```";
      else if (word == "any")
        doc = "**any**\n\nAny type (accepts any value).\n```neutron\nvar any "
              "value = 42;\nvalue = \"now a string\";\n```";
      
      // String properties and methods
      else if (word == "length")
        doc = "**length** (property)\n\nReturns the number of characters in the string.\n```neutron\nvar text = \"hello\";\nsay(text.length);  // 5\n```";
      else if (word == "chars")
        doc = "**chars** (property)\n\nReturns an array of individual characters.\n```neutron\nvar text = \"abc\";\nsay(text.chars);  // [\"a\", \"b\", \"c\"]\n```";
      else if (word == "upper")
        doc = "**upper()** (method)\n\nConverts string to uppercase (Unicode-aware).\n```neutron\nsay(\"hello\".upper());  // \"HELLO\"\nsay(\"café\".upper());   // \"CAFÉ\"\n```";
      else if (word == "lower")
        doc = "**lower()** (method)\n\nConverts string to lowercase (Unicode-aware).\n```neutron\nsay(\"HELLO\".lower());  // \"hello\"\nsay(\"CAFÉ\".lower());   // \"café\"\n```";
      else if (word == "contains")
        doc = "**contains(substring)** (method)\n\nChecks if string contains substring.\n```neutron\nsay(\"hello world\".contains(\"world\"));  // true\n```";
      else if (word == "find")
        doc = "**find(substring, [start], [end])** (method)\n\nReturns index of first occurrence, -1 if not found.\n```neutron\nsay(\"hello\".find(\"ll\"));  // 2\nsay(\"hello\".find(\"xyz\"));  // -1\n```";
      else if (word == "replace")
        doc = "**replace(old, new, [count])** (method)\n\nReplaces occurrences of substring.\n```neutron\nsay(\"hello\".replace(\"l\", \"x\"));  // \"hexxo\"\n```";
      else if (word == "strip")
        doc = "**strip([chars])** (method)\n\nRemoves whitespace from both ends.\n```neutron\nsay(\"  hello  \".strip());  // \"hello\"\n```";
      else if (word == "split")
        doc = "**split(delimiter)** (method)\n\nSplits string into array.\n```neutron\nsay(\"a,b,c\".split(\",\"));  // [\"a\", \"b\", \"c\"]\n```";
      else if (word == "isalpha")
        doc = "**isalpha()** (method)\n\nChecks if all characters are alphabetic.\n```neutron\nsay(\"hello\".isalpha());  // true\nsay(\"hello123\".isalpha());  // false\n```";
      else if (word == "isdigit")
        doc = "**isdigit()** (method)\n\nChecks if all characters are digits.\n```neutron\nsay(\"123\".isdigit());  // true\nsay(\"12a\".isdigit());  // false\n```";
      else if (word == "slice")
        doc = "**slice(start, [end], [step])** (method)\n\nAdvanced string slicing with step support.\n```neutron\nsay(\"hello\".slice(1, 4));  // \"ell\"\nsay(\"hello\".slice(0, -1, 2));  // \"hlo\"\n```";
      else if (word == "format")
        doc = "**format(args...)** (method)\n\nFormats string with positional arguments.\n```neutron\nsay(\"Hello {0}!\".format(\"World\"));  // \"Hello World!\"\n```";
      else if (word == "normalize")
        doc = "**normalize(form)** (method)\n\nUnicode normalization.\n```neutron\nvar text = \"café\";\nsay(text.normalize(\"NFC\"));\n```";
      else if (word == "encode")
        doc = "**encode(encoding)** (method)\n\nEncodes string to bytes.\n```neutron\nvar bytes = \"hello\".encode(\"utf-8\");\n```";
      else if (word == "isunicode")
        doc = "**isunicode()** (method)\n\nChecks if string contains Unicode characters.\n```neutron\nsay(\"café\".isunicode());  // true\nsay(\"hello\".isunicode());  // false\n```";

      if (!doc.empty()) {
        Json::Value contents;
        set(contents, "kind") = "markdown";
        set(contents, "value") = doc;
        set(result, "contents") = contents;
      }
    }
  }

  Json::Value response;
  set(response, "jsonrpc") = "2.0";
  set(response, "id") = id;
  set(response, "result") = result;

  sendJson(response);
}

void LSPServer::onCodeLens(const Json::Value &params, const Json::Value &id) {
  std::string uri = get(get(params, "textDocument"), "uri").asString();
  Json::Value result = Json::arrayValue;

  // Add "Run" button at the top of the file
  Json::Value runLens;
  Json::Value runRange;
  Json::Value startPos, endPos;

  set(startPos, "line") = 0;
  set(startPos, "character") = 0;
  set(endPos, "line") = 0;
  set(endPos, "character") = 0;
  set(runRange, "start") = startPos;
  set(runRange, "end") = endPos;

  set(runLens, "range") = runRange;

  // Command to run the file
  Json::Value runCommand;
  set(runCommand, "title") = "▶ Run";
  set(runCommand, "command") = "neutron.runFile";
  set(runCommand, "arguments") = Json::arrayValue;
  set(runCommand, "arguments").append(uri);
  set(runLens, "command") = runCommand;

  result.append(runLens);

  // Add "Format" button
  Json::Value formatLens;
  set(formatLens, "range") = runRange;

  Json::Value formatCommand;
  set(formatCommand, "title") = "⚙ Format";
  set(formatCommand, "command") = "neutron.formatFile";
  set(formatCommand, "arguments") = Json::arrayValue;
  set(formatCommand, "arguments").append(uri);
  set(formatLens, "command") = formatCommand;

  result.append(formatLens);

  // Find functions and add "Run" buttons for main-like functions
  if (openDocuments.find(uri) != openDocuments.end()) {
    std::string text = openDocuments[uri];
    std::regex funcRegex(R"(fun\s+(\w+)\s*\()");
    std::smatch match;
    std::string::const_iterator searchStart(text.cbegin());
    int lineNum = 0;
    size_t lastPos = 0;

    while (std::regex_search(searchStart, text.cend(), match, funcRegex)) {
      // Calculate line number
      size_t matchPos = match.position() + (searchStart - text.cbegin());
      for (size_t i = lastPos; i < matchPos; i++) {
        if (text[i] == '\n')
          lineNum++;
      }
      lastPos = matchPos;

      std::string funcName = match[1].str();
      if (funcName == "main") {
        Json::Value mainLens;
        Json::Value mainRange;
        Json::Value mainStart, mainEnd;

        set(mainStart, "line") = lineNum;
        set(mainStart, "character") = 0;
        set(mainEnd, "line") = lineNum;
        set(mainEnd, "character") = 0;
        set(mainRange, "start") = mainStart;
        set(mainRange, "end") = mainEnd;

        set(mainLens, "range") = mainRange;

        Json::Value mainCommand;
        set(mainCommand, "title") = "▶ Run main()";
        set(mainCommand, "command") = "neutron.runFile";
        set(mainCommand, "arguments") = Json::arrayValue;
        set(mainCommand, "arguments").append(uri);
        set(mainLens, "command") = mainCommand;

        result.append(mainLens);
      }

      searchStart = match.suffix().first;
    }
  }

  Json::Value response;
  set(response, "jsonrpc") = "2.0";
  set(response, "id") = id;
  set(response, "result") = result;

  sendJson(response);
}

void LSPServer::onCodeLensResolve(const Json::Value &params,
                                  const Json::Value &id) {
  // CodeLens is already resolved, just return it
  Json::Value response;
  set(response, "jsonrpc") = "2.0";
  set(response, "id") = id;
  set(response, "result") = params;

  sendJson(response);
}

void LSPServer::onFormatting(const Json::Value &params, const Json::Value &id) {
  std::string uri = get(get(params, "textDocument"), "uri").asString();
  Json::Value result = Json::arrayValue;

  if (openDocuments.find(uri) != openDocuments.end()) {
    std::string text = openDocuments[uri];

    // Simple formatting - fix indentation
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      lines.push_back(line);
    }

    std::ostringstream formatted;
    int indentLevel = 0;

    for (size_t i = 0; i < lines.size(); i++) {
      std::string &currentLine = lines[i];

      // Get content without leading whitespace
      size_t contentStart = currentLine.find_first_not_of(" \t");
      std::string content = (contentStart == std::string::npos)
                                ? ""
                                : currentLine.substr(contentStart);

      // Trim trailing whitespace
      size_t contentEnd = content.find_last_not_of(" \t\r\n");
      if (contentEnd != std::string::npos) {
        content = content.substr(0, contentEnd + 1);
      }

      // Check for closing brace - decrease indent first
      int thisIndent = indentLevel;
      if (!content.empty() && (content[0] == '}' || content[0] == ']')) {
        thisIndent = std::max(0, thisIndent - 1);
      }

      // Handle else/catch/finally at same level
      if (content.find("} else") == 0 || content.find("} catch") == 0 ||
          content.find("} finally") == 0) {
        thisIndent = std::max(0, indentLevel - 1);
      }

      // Write formatted line
      if (!content.empty()) {
        formatted << std::string(thisIndent * 4, ' ') << content << "\n";
      } else {
        formatted << "\n";
      }

      // Count braces for next line's indent
      int braceCount = 0;
      bool inString = false;
      for (char c : content) {
        if (c == '"' || c == '\'')
          inString = !inString;
        if (!inString) {
          if (c == '{' || c == '[')
            braceCount++;
          if (c == '}' || c == ']')
            braceCount--;
        }
      }
      indentLevel = std::max(0, indentLevel + braceCount);
    }

    std::string newText = formatted.str();

    // Create a single TextEdit that replaces the entire document
    Json::Value edit;
    Json::Value range;
    Json::Value startPos, endPos;

    set(startPos, "line") = 0;
    set(startPos, "character") = 0;
    set(endPos, "line") = (int)lines.size();
    set(endPos, "character") = 0;

    set(range, "start") = startPos;
    set(range, "end") = endPos;

    set(edit, "range") = range;
    set(edit, "newText") = newText;

    result.append(edit);
  }

  Json::Value response;
  set(response, "jsonrpc") = "2.0";
  set(response, "id") = id;
  set(response, "result") = result;

  sendJson(response);
}

void LSPServer::sendJson(const Json::Value &json) {
  Json::FastWriter writer;
  std::string output = writer.write(json);

  // Log what we are sending
  std::cerr << "[LSP] Sending: " << output; // writer output has newline

  std::cout << "Content-Length: " << output.length() << "\r\n\r\n"
            << output << std::flush;
}

} // namespace lsp
} // namespace neutron
