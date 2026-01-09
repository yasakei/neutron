#ifndef NEUTRON_LSP_SERVER_H
#define NEUTRON_LSP_SERVER_H

#include "protocol.h"
#include "compiler/scanner.h"
#include "compiler/parser.h"
#include "stmt.h"
#include "runtime/error_handler.h"
#include <map>
#include <functional>
#include <memory>
#include <iostream>

namespace neutron {
namespace lsp {

class LSPServer {
public:
    LSPServer();
    void run();

private:
    void handleMessage(const Json::Value& message);
    void handleRequest(const Json::Value& message);
    void handleNotification(const Json::Value& message);

    void onInitialize(const Json::Value& params, const Json::Value& id);
    void onShutdown(const Json::Value& id);
    void onTextDocumentDidOpen(const Json::Value& params);
    void onTextDocumentDidChange(const Json::Value& params);
    void onDocumentSymbol(const Json::Value& params, const Json::Value& id);
    void onCompletion(const Json::Value& params, const Json::Value& id);
    void onHover(const Json::Value& params, const Json::Value& id);
    void onCodeLens(const Json::Value& params, const Json::Value& id);
    void onCodeLensResolve(const Json::Value& params, const Json::Value& id);
    void onFormatting(const Json::Value& params, const Json::Value& id);
    
    void validateDocument(const std::string& uri, const std::string& text);
    void publishDiagnostics(const std::string& uri, const std::vector<Diagnostic>& diagnostics);
    void collectSymbols(const std::vector<std::unique_ptr<Stmt>>& statements, Json::Value& symbols);

    void sendJson(const Json::Value& json);

    std::map<std::string, std::string> openDocuments;
    bool running;
};

} // namespace lsp
} // namespace neutron

#endif // NEUTRON_LSP_SERVER_H
