#include "libs/http/native.h"
#include "libs/http/http_ops.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace neutron {

// Helper function to create a response object
Value createResponse(long status, const char* body, const struct HttpHeader* headers, size_t num_headers) {
    auto responseEnv = std::make_shared<Environment>();
    responseEnv->define("status", Value((double)status));
    responseEnv->define("body", Value(std::string(body)));

    auto headersEnv = std::make_shared<Environment>();
    for (size_t i = 0; i < num_headers; i++) {
        headersEnv->define(headers[i].key, Value(std::string(headers[i].value)));
    }
    auto headersModule = new Module("headers", headersEnv, {});
    responseEnv->define("headers", Value(headersModule));

    auto responseModule = new Module("response", responseEnv, {});
    return Value(responseModule);
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
    std::vector<HttpHeader> c_headers;
    if (arguments.size() == 2) {
        if (arguments[1].type == ValueType::OBJECT) {
            JsonObject* headers_obj = static_cast<JsonObject*>(arguments[1].as.object);
            for (const auto& pair : headers_obj->properties) {
                if (pair.second.type == ValueType::STRING) {
                    c_headers.push_back({(char*)pair.first.c_str(), (char*)pair.second.as.string->c_str()});
                }
            }
        }
    }
    
    HttpResponse* c_response = http_get_c(url.c_str(), c_headers.data(), c_headers.size());
    Value response = createResponse(c_response->status, c_response->body, c_response->headers, c_response->num_headers);
    free_http_response(c_response);

    return response;
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
    std::vector<HttpHeader> c_headers;
    if (arguments.size() == 3) {
        if (arguments[2].type == ValueType::OBJECT) {
            JsonObject* headers_obj = static_cast<JsonObject*>(arguments[2].as.object);
            for (const auto& pair : headers_obj->properties) {
                if (pair.second.type == ValueType::STRING) {
                    c_headers.push_back({(char*)pair.first.c_str(), (char*)pair.second.as.string->c_str()});
                }
            }
        }
    }
    
    HttpResponse* c_response = http_post_c(url.c_str(), data.c_str(), c_headers.data(), c_headers.size());
    Value response = createResponse(c_response->status, c_response->body, c_response->headers, c_response->num_headers);
    free_http_response(c_response);

    return response;
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
    std::vector<HttpHeader> c_headers;
    if (arguments.size() == 3) {
        if (arguments[2].type == ValueType::OBJECT) {
            JsonObject* headers_obj = static_cast<JsonObject*>(arguments[2].as.object);
            for (const auto& pair : headers_obj->properties) {
                if (pair.second.type == ValueType::STRING) {
                    c_headers.push_back({(char*)pair.first.c_str(), (char*)pair.second.as.string->c_str()});
                }
            }
        }
    }
    
    HttpResponse* c_response = http_put_c(url.c_str(), data.c_str(), c_headers.data(), c_headers.size());
    Value response = createResponse(c_response->status, c_response->body, c_response->headers, c_response->num_headers);
    free_http_response(c_response);

    return response;
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
    std::vector<HttpHeader> c_headers;
    if (arguments.size() == 2) {
        if (arguments[1].type == ValueType::OBJECT) {
            JsonObject* headers_obj = static_cast<JsonObject*>(arguments[1].as.object);
            for (const auto& pair : headers_obj->properties) {
                if (pair.second.type == ValueType::STRING) {
                    c_headers.push_back({(char*)pair.first.c_str(), (char*)pair.second.as.string->c_str()});
                }
            }
        }
    }
    
    HttpResponse* c_response = http_delete_c(url.c_str(), c_headers.data(), c_headers.size());
    Value response = createResponse(c_response->status, c_response->body, c_response->headers, c_response->num_headers);
    free_http_response(c_response);

    return response;
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
    std::vector<HttpHeader> c_headers;
    if (arguments.size() == 2) {
        if (arguments[1].type == ValueType::OBJECT) {
            JsonObject* headers_obj = static_cast<JsonObject*>(arguments[1].as.object);
            for (const auto& pair : headers_obj->properties) {
                if (pair.second.type == ValueType::STRING) {
                    c_headers.push_back({(char*)pair.first.c_str(), (char*)pair.second.as.string->c_str()});
                }
            }
        }
    }
    
    HttpResponse* c_response = http_head_c(url.c_str(), c_headers.data(), c_headers.size());
    Value response = createResponse(c_response->status, c_response->body, c_response->headers, c_response->num_headers);
    free_http_response(c_response);

    return response;
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
    std::vector<HttpHeader> c_headers;
    if (arguments.size() == 3) {
        if (arguments[2].type == ValueType::OBJECT) {
            JsonObject* headers_obj = static_cast<JsonObject*>(arguments[2].as.object);
            for (const auto& pair : headers_obj->properties) {
                if (pair.second.type == ValueType::STRING) {
                    c_headers.push_back({(char*)pair.first.c_str(), (char*)pair.second.as.string->c_str()});
                }
            }
        }
    }
    
    HttpResponse* c_response = http_patch_c(url.c_str(), data.c_str(), c_headers.data(), c_headers.size());
    Value response = createResponse(c_response->status, c_response->body, c_response->headers, c_response->num_headers);
    free_http_response(c_response);

    return response;
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