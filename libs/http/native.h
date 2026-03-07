#ifndef NEUTRON_LIBS_HTTP_NATIVE_H
#define NEUTRON_LIBS_HTTP_NATIVE_H

/*
 * Code Documentation: HTTP Module (libs/http/native.h)
 * ====================================================
 * 
 * This header defines the HTTP module - Neutron's HTTP client and
 * server capabilities. It provides functions for making HTTP requests
 * and creating HTTP servers.
 * 
 * What This File Includes:
 * ------------------------
 * - register_http_functions(): Register all HTTP module functions
 * - neutron_init_http_module(): C API for module initialization
 * 
 * Available Functions (implemented in native.cpp):
 * -----------------------------------------------
 * Client Functions:
 * - http.get(url, options?) → response: HTTP GET request
 * - http.post(url, data, options?) → response: HTTP POST request
 * - http.put(url, data, options?) → response: HTTP PUT request
 * - http.delete(url, options?) → response: HTTP DELETE request
 * - http.request(method, url, options?) → response: Generic HTTP request
 * 
 * Server Functions:
 * - http.createServer(handler) → server: Create HTTP server
 * - server.listen(port, host?) → void: Start listening
 * - server.close() → void: Stop server
 * 
 * Response Object:
 * - status: number (HTTP status code)
 * - headers: object (response headers)
 * - body: string (response body)
 * - json(): object (parse body as JSON)
 * 
 * Adding Features:
 * ----------------
 * - New methods: Add PATCH, HEAD, OPTIONS support
 * - HTTPS: Add TLS/SSL support for secure connections
 * - WebSockets: Add WebSocket upgrade support
 * - Middleware: Add request/response middleware pipeline
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT make requests to untrusted URLs without validation
 * - Do NOT expose server to untrusted input without sanitization
 * - Do NOT ignore TLS certificate validation in production
 * - Do NOT block the main thread for long-running requests (use async)
 * - Do NOT store sensitive data in URLs (use headers/body)
 * 
 * Example Usage:
 * --------------
 * @code
 * use http
 * 
 * // GET request
 * var response = http.get("https://api.example.com/data")
 * print(response.status)  // 200
 * print(response.json())  // Parsed JSON
 * 
 * // POST request
 * var response = http.post("https://api.example.com/users", 
 *                          json.stringify({name: "Alice"}))
 * 
 * // Create server
 * var server = http.createServer(fn(req, res) {
 *     res.status = 200
 *     res.body = "Hello, World!"
 * })
 * server.listen(8080)
 * @endcode
 */

#include "vm.h"

namespace neutron {

/**
 * @brief Register all HTTP module functions with the VM.
 * @param vm The VM instance.
 * @param env The environment to register functions in.
 * 
 * Creates an "http" module and registers all HTTP client/server functions.
 * Called automatically when "use http" is encountered.
 */
void register_http_functions(VM& vm, std::shared_ptr<Environment> env);

extern "C" {
    /**
     * @brief C API entry point for HTTP module initialization.
     * @param vm The VM instance (as opaque pointer).
     */
    void neutron_init_http_module(VM* vm);
}

} // namespace neutron

#endif // NEUTRON_LIBS_HTTP_NATIVE_H