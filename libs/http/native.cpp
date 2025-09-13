#include "libs/http/native.h"
#include <string>
#include <unordered_map>

namespace neutron {

// Helper function to create a response object
Value createResponse(double status, const std::string& body, const std::unordered_map<std::string, std::string>& headers = {}) {
    auto responseObject = new JsonObject();
    responseObject->properties["status"] = Value(status);
    responseObject->properties["body"] = Value(body);
    
    // Create headers object
    auto headersObject = new JsonObject();
    for (const auto& header : headers) {
        headersObject->properties[header.first] = Value(header.second);
    }
    responseObject->properties["headers"] = Value(headersObject);
    
    return Value(responseObject);
}

// HTTP GET request
Value http_get(std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::runtime_error("Expected 1-2 arguments for http.get().");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("First argument for http.get() must be a string URL.");
    }
    
    std::string url = *arguments[0].as.string;
    
    // Handle headers if provided
    std::unordered_map<std::string, std::string> headers;
    if (arguments.size() == 2) {
        // In a real implementation, we would process headers here
        // For now, we'll just acknowledge they were provided
        headers["X-Neutron-Client"] = "1.0";
    }
    
    // In a real implementation, you would make an HTTP GET request here
    // For now, we'll just return a mock response
    std::string responseBody = "Mock GET response for " + url;
    
    return createResponse(200.0, responseBody, headers);
}

// HTTP POST request
Value http_post(std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 3) {
        throw std::runtime_error("Expected 1-3 arguments for http.post().");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("First argument for http.post() must be a string URL.");
    }
    
    std::string url = *arguments[0].as.string;
    std::string data = "";
    
    // Handle data if provided
    if (arguments.size() >= 2) {
        if (arguments[1].type == ValueType::STRING) {
            data = *arguments[1].as.string;
        }
    }
    
    // Handle headers if provided
    std::unordered_map<std::string, std::string> headers;
    if (arguments.size() == 3) {
        // In a real implementation, we would process headers here
        headers["X-Neutron-Client"] = "1.0";
        headers["Content-Type"] = "application/x-www-form-urlencoded";
    } else if (!data.empty()) {
        headers["Content-Type"] = "application/x-www-form-urlencoded";
    }
    
    // In a real implementation, you would make an HTTP POST request here
    // For now, we'll just return a mock response
    std::string responseBody = "Mock POST response for " + url;
    if (!data.empty()) {
        responseBody += " with data: " + data;
    }
    
    return createResponse(201.0, responseBody, headers);
}

// HTTP PUT request
Value http_put(std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 3) {
        throw std::runtime_error("Expected 1-3 arguments for http.put().");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("First argument for http.put() must be a string URL.");
    }
    
    std::string url = *arguments[0].as.string;
    std::string data = "";
    
    // Handle data if provided
    if (arguments.size() >= 2) {
        if (arguments[1].type == ValueType::STRING) {
            data = *arguments[1].as.string;
        }
    }
    
    // Handle headers if provided
    std::unordered_map<std::string, std::string> headers;
    if (arguments.size() == 3) {
        // In a real implementation, we would process headers here
        headers["X-Neutron-Client"] = "1.0";
        headers["Content-Type"] = "application/x-www-form-urlencoded";
    } else if (!data.empty()) {
        headers["Content-Type"] = "application/x-www-form-urlencoded";
    }
    
    // In a real implementation, you would make an HTTP PUT request here
    // For now, we'll just return a mock response
    std::string responseBody = "Mock PUT response for " + url;
    if (!data.empty()) {
        responseBody += " with data: " + data;
    }
    
    return createResponse(200.0, responseBody, headers);
}

// HTTP DELETE request
Value http_delete(std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::runtime_error("Expected 1-2 arguments for http.delete().");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("First argument for http.delete() must be a string URL.");
    }
    
    std::string url = *arguments[0].as.string;
    
    // Handle headers if provided
    std::unordered_map<std::string, std::string> headers;
    if (arguments.size() == 2) {
        // In a real implementation, we would process headers here
        headers["X-Neutron-Client"] = "1.0";
    }
    
    // In a real implementation, you would make an HTTP DELETE request here
    // For now, we'll just return a mock response
    std::string responseBody = "Mock DELETE response for " + url;
    
    return createResponse(200.0, responseBody, headers);
}

// HTTP HEAD request
Value http_head(std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::runtime_error("Expected 1-2 arguments for http.head().");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("First argument for http.head() must be a string URL.");
    }
    
    std::string url = *arguments[0].as.string;
    
    // Handle headers if provided
    std::unordered_map<std::string, std::string> headers;
    if (arguments.size() == 2) {
        // In a real implementation, we would process headers here
        headers["X-Neutron-Client"] = "1.0";
    }
    
    // In a real implementation, you would make an HTTP HEAD request here
    // For now, we'll just return a mock response with headers but no body
    std::string responseBody = "";
    
    // Add some mock headers
    headers["Content-Type"] = "application/json";
    headers["Content-Length"] = "1024";
    headers["Server"] = "Neutron-Mock-Server";
    
    return createResponse(200.0, responseBody, headers);
}

// HTTP PATCH request
Value http_patch(std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 3) {
        throw std::runtime_error("Expected 1-3 arguments for http.patch().");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("First argument for http.patch() must be a string URL.");
    }
    
    std::string url = *arguments[0].as.string;
    std::string data = "";
    
    // Handle data if provided
    if (arguments.size() >= 2) {
        if (arguments[1].type == ValueType::STRING) {
            data = *arguments[1].as.string;
        }
    }
    
    // Handle headers if provided
    std::unordered_map<std::string, std::string> headers;
    if (arguments.size() == 3) {
        // In a real implementation, we would process headers here
        headers["X-Neutron-Client"] = "1.0";
        headers["Content-Type"] = "application/x-www-form-urlencoded";
    } else if (!data.empty()) {
        headers["Content-Type"] = "application/x-www-form-urlencoded";
    }
    
    // In a real implementation, you would make an HTTP PATCH request here
    // For now, we'll just return a mock response
    std::string responseBody = "Mock PATCH response for " + url;
    if (!data.empty()) {
        responseBody += " with data: " + data;
    }
    
    return createResponse(200.0, responseBody, headers);
}

// Register HTTP functions in the environment
void register_http_functions(std::shared_ptr<Environment> env) {
    // Create an HTTP module
    auto httpEnv = std::make_shared<Environment>();
    httpEnv->define("get", Value(new NativeFn(http_get, -1)));   // 1-2 arguments
    httpEnv->define("post", Value(new NativeFn(http_post, -1))); // 1-3 arguments
    httpEnv->define("put", Value(new NativeFn(http_put, -1)));   // 1-3 arguments
    httpEnv->define("delete", Value(new NativeFn(http_delete, -1))); // 1-2 arguments
    httpEnv->define("head", Value(new NativeFn(http_head, -1))); // 1-2 arguments
    httpEnv->define("patch", Value(new NativeFn(http_patch, -1))); // 1-3 arguments
    
    auto httpModule = new Module("http", httpEnv, std::vector<std::unique_ptr<Stmt>>());
    env->define("http", Value(httpModule));
}

} // namespace neutron