// Only compile HTTP module if CURL is available
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
    
    std::unordered_map<std::string, std::string> responseHeaders;
    long httpCode = 0;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
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

void register_http_functions(VM& vm, std::shared_ptr<Environment> env) {
    env->define("get", Value(vm.allocate<NativeFn>(http_get, -1, true)));
    env->define("post", Value(vm.allocate<NativeFn>(http_post, -1, true)));
    env->define("put", Value(vm.allocate<NativeFn>(http_put, -1, true)));
    env->define("delete", Value(vm.allocate<NativeFn>(http_delete, -1, true)));
    env->define("head", Value(vm.allocate<NativeFn>(http_head, -1, true)));
    env->define("patch", Value(vm.allocate<NativeFn>(http_patch, -1, true)));
    env->define("urlEncode", Value(vm.allocate<NativeFn>(http_urlEncode, 1, true)));
    env->define("urlDecode", Value(vm.allocate<NativeFn>(http_urlDecode, 1, true)));
    env->define("parseQuery", Value(vm.allocate<NativeFn>(http_parseQuery, 1, true)));
    env->define("stopServer", Value(vm.allocate<NativeFn>(http_stopServer, 0, true)));
}

} // namespace neutron

#else // !HAVE_CURL

// Stub implementation when CURL is not available
#include "native.h"
#include "vm.h"
#include "runtime/environment.h"

namespace neutron {

void register_http_functions(VM& vm, std::shared_ptr<Environment> env) {
    (void)vm;
    (void)env;
    // HTTP module not available without CURL
}

} // namespace neutron

#endif // HAVE_CURL
