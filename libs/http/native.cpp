// Windows macro undefs - must be before any includes that might include Windows headers
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    // Undefine Windows macros that conflict with C++ code
    #undef TRUE
    #undef FALSE
    #undef NEAR
    #undef FAR
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

#ifdef HAVE_CURL

#include "native.h"
#include "vm.h"
#include "types/obj_string.h"
#include <string>
#include <unordered_map>
#include <curl/curl.h>
#include <sstream>
#include <cstring>
#ifdef _WIN32
#include <process.h>  // Required for std::thread on Windows
#endif
#include <thread>
#include <atomic>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    // #pragma comment(lib, "Ws2_32.lib") - Handled in CMakeLists.txt
    
    typedef int socklen_t;
    #define close closesocket
    #define SHUT_RDWR SD_BOTH
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    typedef int SOCKET;
#endif

namespace neutron {

// Global server state
static std::atomic<bool> serverRunning{false};
static std::thread* serverThread = nullptr;
static SOCKET serverSocket = INVALID_SOCKET;

// Helper to initialize Winsock on Windows
static void initSockets() {
#ifdef _WIN32
    static bool initialized = false;
    if (!initialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
        initialized = true;
    }
#endif
}

// CURL write callback
static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// CURL header callback
static size_t curlHeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    auto* headers = (std::unordered_map<std::string, std::string>*)userdata;
    size_t totalSize = size * nitems;
    std::string header(buffer, totalSize);
    
    size_t colonPos = header.find(':');
    if (colonPos != std::string::npos) {
        std::string key = header.substr(0, colonPos);
        std::string value = header.substr(colonPos + 1);
        
        // Trim whitespace
        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);
        
        (*headers)[key] = value;
    }
    
    return totalSize;
}

// Helper function to create a response object
Value createResponse(VM& vm, double status, const std::string& body, const std::unordered_map<std::string, std::string>& headers = {}) {
    auto responseObject = vm.allocate<JsonObject>();
    responseObject->properties[vm.internString("status")] = Value(status);
    responseObject->properties[vm.internString("body")] = Value(vm.internString(body));
    
    // Create headers object
    auto headersObject = vm.allocate<JsonObject>();
    for (const auto& header : headers) {
        headersObject->properties[vm.internString(header.first)] = Value(vm.internString(header.second));
    }
    responseObject->properties[vm.internString("headers")] = Value(headersObject);
    
    return Value(responseObject);
}

// HTTP GET request
Value http_get(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::runtime_error("Expected 1-2 arguments for http.get().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("First argument for http.get() must be a string URL.");
    }
    
    std::string url = arguments[0].as.obj_string->chars;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    
    std::string responseBody;
    std::unordered_map<std::string, std::string> responseHeaders;
    long httpCode = 0;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlHeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Neutron/1.0");
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        std::string error = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        throw std::runtime_error("HTTP GET failed: " + error);
    }
    
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    
    return createResponse(vm, static_cast<double>(httpCode), responseBody, responseHeaders);
}

// HTTP POST request
Value http_post(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 3) {
        throw std::runtime_error("Expected 1-3 arguments for http.post().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("First argument for http.post() must be a string URL.");
    }
    
    std::string url = arguments[0].as.obj_string->chars;
    std::string data = "";
    
    if (arguments.size() >= 2 && arguments[1].type == ValueType::OBJ_STRING) {
        data = arguments[1].as.obj_string->chars;
    }
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    
    std::string responseBody;
    std::unordered_map<std::string, std::string> responseHeaders;
    long httpCode = 0;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlHeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Neutron/1.0");
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        std::string error = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        throw std::runtime_error("HTTP POST failed: " + error);
    }
    
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    
    return createResponse(vm, static_cast<double>(httpCode), responseBody, responseHeaders);
}

// HTTP PUT request
Value http_put(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 3) {
        throw std::runtime_error("Expected 1-3 arguments for http.put().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("First argument for http.put() must be a string URL.");
    }
    
    std::string url = arguments[0].as.obj_string->chars;
    std::string data = "";
    
    // Handle data if provided
    if (arguments.size() >= 2) {
        if (arguments[1].type != ValueType::OBJ_STRING) {
            throw std::runtime_error("Second argument for http.put() must be a string data.");
        }
        data = arguments[1].as.obj_string->chars;
    }
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    
    std::string responseBody;
    std::unordered_map<std::string, std::string> responseHeaders;
    long httpCode = 0;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlHeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Neutron/1.0");
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        std::string error = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        throw std::runtime_error("HTTP PUT failed: " + error);
    }
    
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    
    return createResponse(vm, static_cast<double>(httpCode), responseBody, responseHeaders);
}

// HTTP DELETE request
Value http_delete(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::runtime_error("Expected 1-2 arguments for http.delete().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("First argument for http.delete() must be a string URL.");
    }
    
    std::string url = arguments[0].as.obj_string->chars;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    
    std::string responseBody;
    std::unordered_map<std::string, std::string> responseHeaders;
    long httpCode = 0;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlHeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Neutron/1.0");
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        std::string error = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        throw std::runtime_error("HTTP DELETE failed: " + error);
    }
    
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    
    return createResponse(vm, static_cast<double>(httpCode), responseBody, responseHeaders);
}

// HTTP HEAD request
Value http_head(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::runtime_error("Expected 1-2 arguments for http.head().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("First argument for http.head() must be a string URL.");
    }
    
    std::string url = arguments[0].as.obj_string->chars;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    
    std::string responseBody;
    std::unordered_map<std::string, std::string> responseHeaders;
    long httpCode = 0;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlHeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Neutron/1.0");
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        std::string error = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        throw std::runtime_error("HTTP HEAD failed: " + error);
    }
    
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    
    return createResponse(vm, static_cast<double>(httpCode), "", responseHeaders);
}

// HTTP PATCH request
Value http_patch(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 3) {
        throw std::runtime_error("Expected 1-3 arguments for http.patch().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("First argument for http.patch() must be a string URL.");
    }
    
    std::string url = arguments[0].as.obj_string->chars;
    std::string data = "";
    
    if (arguments.size() >= 2 && arguments[1].type == ValueType::OBJ_STRING) {
        data = arguments[1].as.obj_string->chars;
    }
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    
    std::string responseBody;
    std::unordered_map<std::string, std::string> responseHeaders;
    long httpCode = 0;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlHeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Neutron/1.0");
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        std::string error = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        throw std::runtime_error("HTTP PATCH failed: " + error);
    }
    
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    
    return createResponse(vm, static_cast<double>(httpCode), responseBody, responseHeaders);
}

// HTTP request with custom method
Value http_request(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() < 2) {
        throw std::runtime_error("Expected at least 2 arguments for http.request() (method, url).");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("First argument for http.request() must be a string method.");
    }
    if (arguments[1].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Second argument for http.request() must be a string URL.");
    }
    
    std::string method = arguments[0].as.obj_string->chars;
    std::string url = arguments[1].as.obj_string->chars;
    std::string data = "";
    std::unordered_map<std::string, std::string> headers;
    
    if (arguments.size() >= 3 && arguments[2].type == ValueType::OBJ_STRING) {
        data = arguments[2].as.obj_string->chars;
    }
    
    std::string responseBody = "Mock " + method + " request to " + url;
    if (!data.empty()) {
        responseBody += " with data: " + data;
    }
    
    return createResponse(vm, 200.0, responseBody, headers);
}

// Server thread function
static void serverThreadFunc(int port, std::string (*handler)(const std::string&)) {
    initSockets();
    
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        serverRunning = false;
        return;
    }
    
    #ifdef _WIN32
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    #else
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    #endif
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(serverSocket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        close(serverSocket);
        serverRunning = false;
        return;
    }
    
    if (listen(serverSocket, 10) < 0) {
        close(serverSocket);
        serverRunning = false;
        return;
    }
    
    while (serverRunning) {
        SOCKET clientSocket = accept(serverSocket, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (clientSocket == INVALID_SOCKET) continue;
        
        char buffer[4096] = {0};
        recv(clientSocket, buffer, 4096, 0);
        
        std::string request(buffer);
        std::string response;
        
        if (handler) {
            response = handler(request);
        } else {
            // Default behavior: Echo request info in JSON for testing
            std::string method = "GET";
            std::string path = "/";
            
            // Simple parsing
            size_t firstSpace = request.find(' ');
            if (firstSpace != std::string::npos) {
                method = request.substr(0, firstSpace);
                size_t secondSpace = request.find(' ', firstSpace + 1);
                if (secondSpace != std::string::npos) {
                    path = request.substr(firstSpace + 1, secondSpace - firstSpace - 1);
                }
            }
            
            response = "{\"method\": \"" + method + "\", \"path\": \"" + path + "\", \"message\": \"Hello from Neutron Server!\"}";
        }
        
        std::string httpResponse = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: " + std::to_string(response.length()) + "\r\n"
            "\r\n" + response;
        
        send(clientSocket, httpResponse.c_str(), httpResponse.length(), 0);
        close(clientSocket);
    }
}

// Create HTTP server
Value http_createServer(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() < 1) {
        throw std::runtime_error("Expected at least 1 argument for http.createServer() (handler function).");
    }
    
    auto serverObj = vm.allocate<JsonObject>();
    serverObj->properties[vm.internString("port")] = Value(0.0);
    serverObj->properties[vm.internString("running")] = Value(false);
    serverObj->properties[vm.internString("handler")] = arguments[0];
    
    return Value(serverObj);
}

// Start HTTP server (blocking)
Value http_listen(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() < 2) {
        throw std::runtime_error("Expected 2 arguments for http.listen() (server, port).");
    }
    
    if (arguments[0].type != ValueType::OBJECT) {
        throw std::runtime_error("First argument for http.listen() must be a server object.");
    }
    if (arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Second argument for http.listen() must be a port number.");
    }
    
    JsonObject* server = dynamic_cast<JsonObject*>(arguments[0].as.object);
    if (!server) {
        throw std::runtime_error("Invalid server object.");
    }
    
    int port = static_cast<int>(arguments[1].as.number);
    Value handler = server->properties[vm.internString("handler")];
    
    if (serverRunning) {
        throw std::runtime_error("Server is already running");
    }
    
    serverRunning = true;
    server->properties[vm.internString("port")] = Value(static_cast<double>(port));
    server->properties[vm.internString("running")] = Value(true);
    
    initSockets();
    
    // Setup socket
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        serverRunning = false;
        throw std::runtime_error("Socket creation failed");
    }
    
    #ifdef _WIN32
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    #else
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    #endif
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(serverSocket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        close(serverSocket);
        serverRunning = false;
        throw std::runtime_error("Bind failed");
    }
    
    if (listen(serverSocket, 10) < 0) {
        close(serverSocket);
        serverRunning = false;
        throw std::runtime_error("Listen failed");
    }
    
    // Main loop (blocking)
    while (serverRunning) {
        SOCKET clientSocket = accept(serverSocket, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (clientSocket == INVALID_SOCKET) continue;
        
        char buffer[4096] = {0};
        recv(clientSocket, buffer, 4096, 0);
        
        std::string rawRequest(buffer);
        
        // Parse method and path
        std::string method = "GET";
        std::string path = "/";
        
        size_t firstLineEnd = rawRequest.find("\r\n");
        if (firstLineEnd != std::string::npos) {
            std::string requestLine = rawRequest.substr(0, firstLineEnd);
            size_t firstSpace = requestLine.find(' ');
            if (firstSpace != std::string::npos) {
                method = requestLine.substr(0, firstSpace);
                size_t secondSpace = requestLine.find(' ', firstSpace + 1);
                if (secondSpace != std::string::npos) {
                    path = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
                }
            }
        }
        
        // Create request object
        auto reqObj = vm.allocate<JsonObject>();
        reqObj->properties[vm.internString("raw")] = Value(vm.internString(rawRequest));
        reqObj->properties[vm.internString("method")] = Value(vm.internString(method));
        reqObj->properties[vm.internString("path")] = Value(vm.internString(path));
        
        // Call handler
        vm.push(handler);
        vm.push(Value(reqObj));
        
        size_t frameCountBefore = vm.frames.size();
        vm.callValuePublic(handler, 1);
        
        // Only run if a new frame was pushed (interpreted function)
        if (vm.frames.size() > frameCountBefore) {
            vm.runPublic(frameCountBefore);
        }
        
        std::string responseBody;
        int status = 200;
        std::string contentType = "text/html";
        
        // Check if stack is empty
        if (vm.stack.empty()) {
            // Handler didn't return anything or stack issue
            responseBody = "Internal Server Error: Handler returned no value";
            status = 500;
        } else {
            Value responseVal = vm.pop();
            
            if (responseVal.type == ValueType::OBJ_STRING) {
                responseBody = responseVal.as.obj_string->chars;
            } else if (responseVal.type == ValueType::OBJECT) {
                JsonObject* resObj = dynamic_cast<JsonObject*>(responseVal.as.object);
                if (resObj) {
                    if (resObj->properties.count(vm.internString("body")) && resObj->properties[vm.internString("body")].type == ValueType::OBJ_STRING) {
                        responseBody = resObj->properties[vm.internString("body")].as.obj_string->chars;
                    }
                    if (resObj->properties.count(vm.internString("status")) && resObj->properties[vm.internString("status")].type == ValueType::NUMBER) {
                        status = static_cast<int>(resObj->properties[vm.internString("status")].as.number);
                    }
                    // TODO: Handle headers
                }
            }
        }
        
        std::string httpResponse = 
            "HTTP/1.1 " + std::to_string(status) + " OK\r\n"
            "Content-Type: " + contentType + "\r\n"
            "Content-Length: " + std::to_string(responseBody.length()) + "\r\n"
            "Connection: close\r\n"
            "\r\n" + responseBody;
        
        send(clientSocket, httpResponse.c_str(), httpResponse.length(), 0);
        close(clientSocket);
    }
    
    return Value(true);
}

// Simple server start (just takes port, no handler)
Value http_startServer(VM& /*vm*/, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for http.startServer() (port).");
    }
    
    if (arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("Argument for http.startServer() must be a port number.");
    }
    
    int port = static_cast<int>(arguments[0].as.number);
    
    if (serverRunning) {
        throw std::runtime_error("Server is already running");
    }
    
    serverRunning = true;
    
    // Start server in background thread
    serverThread = new std::thread(serverThreadFunc, port, nullptr);
    serverThread->detach();
    
    return Value(true);
}

// Stop HTTP server
Value http_stopServer(VM& /*vm*/, std::vector<Value> /*arguments*/) {
    if (serverRunning) {
        serverRunning = false;
        if (serverSocket != INVALID_SOCKET) {
            close(serverSocket);
            serverSocket = INVALID_SOCKET;
        }
    }
    return Value(true);
}

// URL encode
Value http_urlEncode(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for http.urlEncode().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for http.urlEncode() must be a string.");
    }
    
    std::string str = arguments[0].as.obj_string->chars;
    std::string encoded;
    
    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else if (c == ' ') {
            encoded += '+';
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
            encoded += hex;
        }
    }
    
    return Value(vm.internString(encoded));
}

// URL decode
Value http_urlDecode(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for http.urlDecode().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for http.urlDecode() must be a string.");
    }
    
    std::string str = arguments[0].as.obj_string->chars;
    std::string decoded;
    
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int hex;
            sscanf(str.substr(i + 1, 2).c_str(), "%x", &hex);
            decoded += static_cast<char>(hex);
            i += 2;
        } else if (str[i] == '+') {
            decoded += ' ';
        } else {
            decoded += str[i];
        }
    }
    
    return Value(vm.internString(decoded));
}

// Parse query string
Value http_parseQuery(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for http.parseQuery().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for http.parseQuery() must be a string.");
    }
    
    std::string query = arguments[0].as.obj_string->chars;
    auto params = vm.allocate<JsonObject>();
    
    size_t pos = 0;
    while (pos < query.length()) {
        size_t eq = query.find('=', pos);
        if (eq == std::string::npos) break;
        
        size_t amp = query.find('&', eq);
        if (amp == std::string::npos) amp = query.length();
        
        std::string key = query.substr(pos, eq - pos);
        std::string value = query.substr(eq + 1, amp - eq - 1);
        
        params->properties[vm.internString(key)] = Value(vm.internString(value));
        pos = amp + 1;
    }
    
    return Value(params);
}

// Serve static HTML content
Value http_serveHTML(VM& /*vm*/, std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for http.serveHTML() (port, html).");
    }
    
    if (arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("First argument for http.serveHTML() must be a port number.");
    }
    if (arguments[1].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Second argument for http.serveHTML() must be a string HTML content.");
    }
    
    int port = static_cast<int>(arguments[0].as.number);
    std::string html_content = arguments[1].as.obj_string->chars;
    
    if (serverRunning) {
        throw std::runtime_error("Server is already running");
    }
    
    serverRunning = true;
    
    // Start server in background thread with captured HTML content
    serverThread = new std::thread([port, html_content]() {
        initSockets();
        
        struct sockaddr_in address;
        int opt = 1;
        int addrlen = sizeof(address);
        
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == INVALID_SOCKET) {
            serverRunning = false;
            return;
        }
        
        #ifdef _WIN32
            setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
        #else
            setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
        #endif
        
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);
        
        if (bind(serverSocket, (struct sockaddr*)&address, sizeof(address)) < 0) {
            close(serverSocket);
            serverRunning = false;
            return;
        }
        
        if (listen(serverSocket, 10) < 0) {
            close(serverSocket);
            serverRunning = false;
            return;
        }
        
        while (serverRunning) {
            SOCKET clientSocket = accept(serverSocket, (struct sockaddr*)&address, (socklen_t*)&addrlen);
            if (clientSocket == INVALID_SOCKET) continue;
            
            char buffer[4096] = {0};
            recv(clientSocket, buffer, 4096, 0);
            
            std::string httpResponse = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=UTF-8\r\n"
                "Content-Length: " + std::to_string(html_content.length()) + "\r\n"
                "Connection: close\r\n"
                "\r\n" + html_content;
            
            send(clientSocket, httpResponse.c_str(), httpResponse.length(), 0);
            close(clientSocket);
        }
    });
    serverThread->detach();
    
    return Value(true);
}

void register_http_functions(VM& vm, std::shared_ptr<Environment> env) {
    // REST API methods
    env->define("get", Value(vm.allocate<NativeFn>(http_get, -1, true)));
    env->define("post", Value(vm.allocate<NativeFn>(http_post, -1, true)));
    env->define("put", Value(vm.allocate<NativeFn>(http_put, -1, true)));
    env->define("delete", Value(vm.allocate<NativeFn>(http_delete, -1, true)));
    env->define("head", Value(vm.allocate<NativeFn>(http_head, -1, true)));
    env->define("patch", Value(vm.allocate<NativeFn>(http_patch, -1, true)));
    env->define("request", Value(vm.allocate<NativeFn>(http_request, -1, true)));
    
    // Server functions
    env->define("createServer", Value(vm.allocate<NativeFn>(http_createServer, -1, true)));
    env->define("listen", Value(vm.allocate<NativeFn>(http_listen, 2, true))); // Needs VM
    env->define("startServer", Value(vm.allocate<NativeFn>(http_startServer, 1, true)));
    env->define("stopServer", Value(vm.allocate<NativeFn>(http_stopServer, 0, true)));
    env->define("serveHTML", Value(vm.allocate<NativeFn>(http_serveHTML, 2, true)));
    
    // Utility functions
    env->define("urlEncode", Value(vm.allocate<NativeFn>(http_urlEncode, 1, true)));
    env->define("urlDecode", Value(vm.allocate<NativeFn>(http_urlDecode, 1, true)));
    env->define("parseQuery", Value(vm.allocate<NativeFn>(http_parseQuery, 1, true)));
}

} // namespace neutron

extern "C" void neutron_init_http_module(neutron::VM* vm) {
    auto http_env = std::make_shared<neutron::Environment>();
    neutron::register_http_functions(*vm, http_env);
    auto http_module = vm->allocate<neutron::Module>("http", http_env);
    vm->define_module("http", http_module);
}

#else // !HAVE_CURL

// Stub implementation when CURL is not available
#include "native.h"
#include "vm.h"
#include "runtime/environment.h"

namespace neutron {

void register_http_functions(VM& vm, std::shared_ptr<Environment> env) {
    (void)vm;
    (void)env;
    // HTTP module not available without CURL - functions will throw runtime errors
}

} // namespace neutron

extern "C" void neutron_init_http_module(neutron::VM* vm) {
    auto http_env = std::make_shared<neutron::Environment>();
    neutron::register_http_functions(*vm, http_env);
    auto http_module = vm->allocate<neutron::Module>("http", http_env);
    vm->define_module("http", http_module);
}

#endif // HAVE_CURL