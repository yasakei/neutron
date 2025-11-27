#include "native.h"
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
    
    std::string url = std::get<std::string>(arguments[0].as);
    
    // Handle headers if provided
    std::unordered_map<std::string, std::string> headers;
    if (arguments.size() == 2) {
        // In a real implementation, we would process headers here
        // For now, we'll just acknowledge they were provided
        headers["X-Neutron-Client"] = "1.0";
    }
    
    // In a real implementation, you would make an HTTP GET request here
    // For now, we'll just return a mock response
    std::string responseBody;
    
    if (url.find("api.github.com") != std::string::npos) {
        // Return a mock JSON response for GitHub API
        responseBody = "{"
            "\"name\": \"vscode\","
            "\"stargazers_count\": 160000,"
            "\"forks_count\": 28000,"
            "\"language\": \"TypeScript\""
        "}";
        headers["Content-Type"] = "application/json";
    } else {
        responseBody = "Mock GET response for " + url;
    }
    
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
    
    std::string url = std::get<std::string>(arguments[0].as);
    std::string data = "";
    
    // Handle data if provided
    if (arguments.size() >= 2) {
        if (arguments[1].type != ValueType::STRING) {
            throw std::runtime_error("Second argument for http.post() must be a string data.");
        }
        data = std::get<std::string>(arguments[1].as);
    }
    
    // Handle headers if provided
    std::unordered_map<std::string, std::string> headers;
    if (arguments.size() == 3) {
        // In a real implementation, we would process headers here
        // For now, we'll just acknowledge they were provided
        headers["X-Neutron-Client"] = "1.0";
    }
    
    // In a real implementation, you would make an HTTP POST request here
    // For now, we'll just return a mock response
    std::string responseBody = "Mock POST response for " + url + " with data: " + data;
    
    return createResponse(200.0, responseBody, headers);
}

// HTTP PUT request
Value http_put(std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 3) {
        throw std::runtime_error("Expected 1-3 arguments for http.put().");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("First argument for http.put() must be a string URL.");
    }
    
    std::string url = std::get<std::string>(arguments[0].as);
    std::string data = "";
    
    // Handle data if provided
    if (arguments.size() >= 2) {
        if (arguments[1].type != ValueType::STRING) {
            throw std::runtime_error("Second argument for http.put() must be a string data.");
        }
        data = std::get<std::string>(arguments[1].as);
    }
    
    // Handle headers if provided
    std::unordered_map<std::string, std::string> headers;
    if (arguments.size() == 3) {
        // In a real implementation, we would process headers here
        // For now, we'll just acknowledge they were provided
        headers["X-Neutron-Client"] = "1.0";
    }
    
    // In a real implementation, you would make an HTTP PUT request here
    // For now, we'll just return a mock response
    std::string responseBody = "Mock PUT response for " + url + " with data: " + data;
    
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
    
    std::string url = std::get<std::string>(arguments[0].as);
    
    // Handle headers if provided
    std::unordered_map<std::string, std::string> headers;
    if (arguments.size() == 2) {
        // In a real implementation, we would process headers here
        // For now, we'll just acknowledge they were provided
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
    
    std::string url = std::get<std::string>(arguments[0].as);
    
    // Handle headers if provided
    std::unordered_map<std::string, std::string> headers;
    if (arguments.size() == 2) {
        // In a real implementation, we would process headers here
        // For now, we'll just acknowledge they were provided
        headers["X-Neutron-Client"] = "1.0";
    }
    
    // In a real implementation, you would make an HTTP HEAD request here
    // For now, we'll just return a mock response
    std::string responseBody = "";
    headers["Content-Length"] = "0";
    
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
    
    std::string url = std::get<std::string>(arguments[0].as);
    std::string data = "";
    
    // Handle data if provided
    if (arguments.size() >= 2) {
        if (arguments[1].type != ValueType::STRING) {
            throw std::runtime_error("Second argument for http.patch() must be a string data.");
        }
        data = std::get<std::string>(arguments[1].as);
    }
    
    // Handle headers if provided
    std::unordered_map<std::string, std::string> headers;
    if (arguments.size() == 3) {
        // In a real implementation, we would process headers here
        // For now, we'll just acknowledge they were provided
        headers["X-Neutron-Client"] = "1.0";
    }
    
    // In a real implementation, you would make an HTTP PATCH request here
    // For now, we'll just return a mock response
    std::string responseBody = "Mock PATCH response for " + url + " with data: " + data;
    
    return createResponse(200.0, responseBody, headers);
}

void register_http_functions(std::shared_ptr<Environment> env) {
    env->define("get", Value(new NativeFn(http_get, -1))); // 1-2 arguments
    env->define("post", Value(new NativeFn(http_post, -1))); // 1-3 arguments
    env->define("put", Value(new NativeFn(http_put, -1))); // 1-3 arguments
    env->define("delete", Value(new NativeFn(http_delete, -1))); // 1-2 arguments
    env->define("head", Value(new NativeFn(http_head, -1))); // 1-2 arguments
    env->define("patch", Value(new NativeFn(http_patch, -1))); // 1-3 arguments
}

} // namespace neutron

extern "C" void neutron_init_http_module(neutron::VM* vm) {
    auto http_env = std::make_shared<neutron::Environment>();
    neutron::register_http_functions(http_env);
    auto http_module = new neutron::Module("http", http_env);
    vm->define_module("http", http_module);
}