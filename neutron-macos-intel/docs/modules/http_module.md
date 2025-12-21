# HTTP Module Documentation

The `http` module provides HTTP client and server functionality for Neutron programs. It uses **libcurl** for real HTTP client operations and **POSIX sockets** for real HTTP server functionality. All network operations are fully implemented - no mock implementations.

## Usage

```neutron
use http;

// Make HTTP requests
var response = http.get("https://api.example.com/data");
say("Status: " + response["status"]);
say("Body: " + response["body"]);
```

## Response Object Structure

All HTTP functions return a response object with the following structure:

```neutron
{
    "status": 200,           // HTTP status code (number)
    "body": "response text", // Response body (string)
    "headers": {             // Response headers (object)
        "content-type": "application/json",
        "content-length": "123"
    }
}
```

## HTTP Methods

### `http.get(url, [headers])`
Performs an HTTP GET request to retrieve data from a server.

**Parameters:**
- `url` (string): The URL to send the GET request to
- `headers` (object, optional): Additional HTTP headers to send

**Returns:** Response object with status, body, and headers

**Example:**
```neutron
use http;

// Simple GET request
var response = http.get("https://api.example.com/users");
say("Status: " + response["status"]); // Output: Status: 200
say("Users data: " + response["body"]);

// GET request with custom headers
var headers = {"Authorization": "Bearer token123"};
var authResponse = http.get("https://api.example.com/profile", headers);
if (authResponse["status"] == 200) {
    say("Profile loaded successfully");
}
```

**Common Use Cases:**
- Fetching data from APIs
- Downloading web content
- Checking server status
- Retrieving configuration files

---

### `http.post(url, [data], [headers])`
Performs an HTTP POST request to send data to a server.

**Parameters:**
- `url` (string): The URL to send the POST request to
- `data` (string, optional): The data to send in the request body
- `headers` (object, optional): Additional HTTP headers to send

**Returns:** Response object with status, body, and headers

**Example:**
```neutron
use http;
use json;

// Simple POST request
var response = http.post("https://api.example.com/users");
say("Status: " + response["status"]);

// POST request with JSON data
var userData = {"name": "Alice", "email": "alice@example.com"};
var jsonData = json.stringify(userData);
var headers = {"Content-Type": "application/json"};

var createResponse = http.post("https://api.example.com/users", jsonData, headers);
if (createResponse["status"] == 201) {
    say("User created successfully");
    say("Response: " + createResponse["body"]);
}

// POST request with form data
var formData = "name=Bob&email=bob@example.com";
var formHeaders = {"Content-Type": "application/x-www-form-urlencoded"};
var formResponse = http.post("https://api.example.com/submit", formData, formHeaders);
```

**Common Use Cases:**
- Creating new resources
- Submitting forms
- User authentication
- File uploads

---

### `http.put(url, [data], [headers])`
Performs an HTTP PUT request to update or create a resource.

**Parameters:**
- `url` (string): The URL to send the PUT request to
- `data` (string, optional): The data to send in the request body
- `headers` (object, optional): Additional HTTP headers to send

**Returns:** Response object with status, body, and headers

**Example:**
```neutron
use http;
use json;

// Update user profile
var updatedProfile = {
    "name": "Alice Smith",
    "email": "alice.smith@example.com",
    "active": true
};

var jsonData = json.stringify(updatedProfile);
var headers = {"Content-Type": "application/json"};

var response = http.put("https://api.example.com/users/123", jsonData, headers);
if (response["status"] == 200) {
    say("Profile updated successfully");
} else if (response["status"] == 201) {
    say("Profile created successfully");
}
```

**Common Use Cases:**
- Updating existing resources
- Creating resources with specific IDs
- Replacing entire resource representations

---

### `http.delete(url, [headers])`
Performs an HTTP DELETE request to remove a resource.

**Parameters:**
- `url` (string): The URL to send the DELETE request to
- `headers` (object, optional): Additional HTTP headers to send

**Returns:** Response object with status, body, and headers

**Example:**
```neutron
use http;

// Delete a user
var response = http.delete("https://api.example.com/users/123");
if (response["status"] == 204) {
    say("User deleted successfully");
} else if (response["status"] == 404) {
    say("User not found");
}

// Delete with authentication
var headers = {"Authorization": "Bearer token123"};
var authResponse = http.delete("https://api.example.com/posts/456", headers);
say("Delete status: " + authResponse["status"]);
```

**Common Use Cases:**
- Removing resources
- Cleanup operations
- User account deletion

---

### `http.head(url, [headers])`
Performs an HTTP HEAD request to retrieve only the headers (no body).

**Parameters:**
- `url` (string): The URL to send the HEAD request to
- `headers` (object, optional): Additional HTTP headers to send

**Returns:** Response object with status, empty body, and headers

**Example:**
```neutron
use http;

// Check if resource exists without downloading content
var response = http.head("https://api.example.com/large-file.zip");
if (response["status"] == 200) {
    say("File exists");
    var contentLength = response["headers"]["Content-Length"];
    say("File size: " + contentLength + " bytes");
} else {
    say("File not found");
}

// Check last modified date
var headers = {"If-Modified-Since": "Wed, 21 Oct 2015 07:28:00 GMT"};
var modResponse = http.head("https://api.example.com/data.json", headers);
if (modResponse["status"] == 304) {
    say("Resource not modified");
}
```

**Common Use Cases:**
- Checking resource existence
- Getting file sizes
- Checking last modified dates
- Validating URLs

---

### `http.patch(url, [data], [headers])`
Performs an HTTP PATCH request to partially update a resource.

**Parameters:**
- `url` (string): The URL to send the PATCH request to
- `data` (string, optional): The partial data to send in the request body
- `headers` (object, optional): Additional HTTP headers to send

**Returns:** Response object with status, body, and headers

**Example:**
```neutron
use http;
use json;

// Partially update user profile
var partialUpdate = {
    "email": "newemail@example.com",
    "active": false
};

var jsonData = json.stringify(partialUpdate);
var headers = {"Content-Type": "application/json"};

var response = http.patch("https://api.example.com/users/123", jsonData, headers);
if (response["status"] == 200) {
    say("Profile updated successfully");
    say("Updated data: " + response["body"]);
}

// Update specific fields
var statusUpdate = {"status": "inactive"};
var statusJson = json.stringify(statusUpdate);
var patchResponse = http.patch("https://api.example.com/accounts/456", statusJson, headers);
```

**Common Use Cases:**
- Partial resource updates
- Status changes
- Incremental modifications
- Field-specific updates

## Common Usage Patterns

### API Client Class
```neutron
use http;
use json;

class ApiClient {
    var baseUrl;
    var defaultHeaders;
    
    fun initialize(url, token) {
        this.baseUrl = url;
        this.defaultHeaders = {
            "Content-Type": "application/json",
            "Authorization": "Bearer " + token
        };
    }
    
    fun get(endpoint) {
        var fullUrl = this.baseUrl + endpoint;
        return http.get(fullUrl, this.defaultHeaders);
    }
    
    fun post(endpoint, data) {
        var fullUrl = this.baseUrl + endpoint;
        var jsonData = json.stringify(data);
        return http.post(fullUrl, jsonData, this.defaultHeaders);
    }
    
    fun put(endpoint, data) {
        var fullUrl = this.baseUrl + endpoint;
        var jsonData = json.stringify(data);
        return http.put(fullUrl, jsonData, this.defaultHeaders);
    }
    
    fun delete(endpoint) {
        var fullUrl = this.baseUrl + endpoint;
        return http.delete(fullUrl, this.defaultHeaders);
    }
}

var client = ApiClient();
client.initialize("https://api.example.com", "your-token-here");

var users = client.get("/users");
var newUser = client.post("/users", {"name": "Alice", "email": "alice@example.com"});
```

### Error Handling and Retry Logic
```neutron
use http;

fun makeRequestWithRetry(url, maxRetries) {
    var attempts = 0;
    
    while (attempts < maxRetries) {
        var response = http.get(url);
        
        if (response["status"] == 200) {
            return response;
        } else if (response["status"] >= 500) {
            say("Server error, retrying... Attempt " + (attempts + 1));
            attempts = attempts + 1;
        } else {
            say("Client error: " + response["status"]);
            return response;
        }
    }
    
    say("Max retries exceeded");
    return nil;
}

var response = makeRequestWithRetry("https://api.example.com/data", 3);
if (response != nil) {
    say("Success: " + response["body"]);
}
```

### Response Processing
```neutron
use http;
use json;

fun processApiResponse(response) {
    if (response["status"] >= 200 and response["status"] < 300) {
        say("Success! Status: " + response["status"]);
        
        // Try to parse JSON response
        var data = json.parse(response["body"]);
        return data;
    } else if (response["status"] == 404) {
        say("Resource not found");
        return nil;
    } else if (response["status"] >= 400 and response["status"] < 500) {
        say("Client error: " + response["status"]);
        say("Error details: " + response["body"]);
        return nil;
    } else if (response["status"] >= 500) {
        say("Server error: " + response["status"]);
        return nil;
    }
    
    return nil;
}

var response = http.get("https://api.example.com/users/123");
var userData = processApiResponse(response);
if (userData != nil) {
    var name = json.get(userData, "name");
    say("User name: " + name);
}
```

### File Download Simulation
```neutron
use http;
use sys;

fun downloadFile(url, filename) {
    say("Downloading " + url + "...");
    
    // Check file size first
    var headResponse = http.head(url);
    if (headResponse["status"] != 200) {
        say("File not accessible: " + headResponse["status"]);
        return false;
    }
    
    // Download the file
    var response = http.get(url);
    if (response["status"] == 200) {
        sys.write(filename, response["body"]);
        say("Downloaded to " + filename);
        return true;
    }
    
    say("Download failed: " + response["status"]);
    return false;
}

var success = downloadFile("https://example.com/data.json", "local_data.json");
if (success) {
    say("File downloaded successfully");
}
```

### REST API CRUD Operations
```neutron
use http;
use json;

class UserManager {
    var apiUrl;
    var headers;
    
    fun initialize() {
        this.apiUrl = "https://api.example.com/users";
        this.headers = {"Content-Type": "application/json"};
    }
    
    fun createUser(userData) {
        var jsonData = json.stringify(userData);
        var response = http.post(this.apiUrl, jsonData, this.headers);
        return response["status"] == 201;
    }
    
    fun getUser(userId) {
        var url = this.apiUrl + "/" + userId;
        var response = http.get(url);
        if (response["status"] == 200) {
            return json.parse(response["body"]);
        }
        return nil;
    }
    
    fun updateUser(userId, userData) {
        var url = this.apiUrl + "/" + userId;
        var jsonData = json.stringify(userData);
        var response = http.put(url, jsonData, this.headers);
        return response["status"] == 200;
    }
    
    fun deleteUser(userId) {
        var url = this.apiUrl + "/" + userId;
        var response = http.delete(url);
        return response["status"] == 204;
    }
}

var userMgr = UserManager();
userMgr.initialize();

// Create user
var created = userMgr.createUser({"name": "Alice", "email": "alice@example.com"});
if (created) {
    say("User created successfully");
}
```

## Status Code Handling

Common HTTP status codes and their meanings:

```neutron
use http;

fun handleStatusCode(status) {
    if (status == 200) {
        return "OK - Request successful";
    } else if (status == 201) {
        return "Created - Resource created successfully";
    } else if (status == 204) {
        return "No Content - Request successful, no response body";
    } else if (status == 400) {
        return "Bad Request - Invalid request data";
    } else if (status == 401) {
        return "Unauthorized - Authentication required";
    } else if (status == 403) {
        return "Forbidden - Access denied";
    } else if (status == 404) {
        return "Not Found - Resource doesn't exist";
    } else if (status == 500) {
        return "Internal Server Error - Server error";
    } else {
        return "Status: " + status;
    }
}

var response = http.get("https://api.example.com/test");
var statusMessage = handleStatusCode(response["status"]);
say(statusMessage);
```

## Utility Functions

### `http.request(method, url, [data], [headers])`
Makes a custom HTTP request with specified method.

**Parameters:**
- `method` (string): HTTP method (GET, POST, PUT, DELETE, etc.)
- `url` (string): Target URL
- `data` (string, optional): Request body
- `headers` (object, optional): Request headers

**Returns:** Response object

**Example:**
```neutron
use http;

var response = http.request("OPTIONS", "https://api.example.com");
say("Status: " + response.status);
```

---

### `http.urlEncode(string)`
Encodes a string for safe use in URLs (percent-encoding).

**Parameters:**
- `string` (string): String to encode

**Returns:** URL-encoded string

**Example:**
```neutron
use http;

var encoded = http.urlEncode("hello world!");
say(encoded); // Output: hello+world%21
```

---

### `http.urlDecode(string)`
Decodes a URL-encoded string.

**Parameters:**
- `string` (string): URL-encoded string

**Returns:** Decoded string

**Example:**
```neutron
use http;

var decoded = http.urlDecode("hello+world%21");
say(decoded); // Output: hello world!
```

---

### `http.parseQuery(queryString)`
Parses a URL query string into an object.

**Parameters:**
- `queryString` (string): Query string (e.g., "name=John&age=30")

**Returns:** Object with key-value pairs

**Example:**
```neutron
use http;

var params = http.parseQuery("name=John&age=30&city=NYC");
say(params.name); // Output: John
say(params.age);  // Output: 30
```

---

## Server Functions

The HTTP server uses **POSIX sockets** for real TCP/IP networking.

### `http.createServer(handler)`
Creates an HTTP server object with a custom request handler.

**Parameters:**
- `handler` (function): A function that takes a request object and returns a response.

**Returns:** Server object

**Handler Function:**
The handler function receives a `request` object with the following properties:
- `method` (string): The HTTP method (e.g., "GET", "POST").
- `path` (string): The request path (e.g., "/api/users").
- `raw` (string): The raw HTTP request string.

The handler must return either:
1.  A **string** (which will be sent as the response body with status 200 OK).
2.  An **object** with:
    - `status` (number, optional): HTTP status code (default: 200).
    - `body` (string): The response body.

**Example:**
```neutron
use http;

fun handler(req) {
    say("Received " + req.method + " request for " + req.path);
    
    if (req.path == "/") {
        return "Welcome to the home page!";
    } else if (req.path == "/api") {
        return {
            "status": 200,
            "body": "{\"message\": \"API is working\"}"
        };
    } else {
        return {
            "status": 404,
            "body": "Not Found"
        };
    }
}

var server = http.createServer(handler);
http.listen(server, 8080);
```

---

### `http.listen(server, port)`
Starts the HTTP server and listens for incoming connections. This function blocks the current thread.

**Parameters:**
- `server` (object): Server object returned from `http.createServer`.
- `port` (number): Port number to listen on (e.g., 8080).

**Returns:** Does not return (blocking loop).

---

### `http.startServer(port)`
Starts a simple background HTTP server on the specified port. This is useful for simple static serving or testing, but `createServer` is recommended for custom logic.

**Parameters:**
- `port` (number): Port number to listen on.

**Returns:** `true` on success.

---

### `http.stopServer()`
Stops the currently running background HTTP server (started with `startServer`).

**Returns:** `true` on success.

---

## Implementation Details

### HTTP Client (libcurl)
- Uses **libcurl 8.17.0** for real HTTP/HTTPS requests
- Supports TLS/SSL encryption
- Handles redirects automatically
- 30-second default timeout
- Custom User-Agent: "Neutron/1.0"

### HTTP Server (POSIX Sockets)
- Real TCP/IP socket binding with `socket()`, `bind()`, `listen()`
- Background thread using C++ `std::thread`
- Accepts connections with `accept()`
- Reads HTTP requests from socket
- Returns basic "Neutron HTTP Server" responses

### Security Considerations
When using HTTP functionality:

- Always validate URLs and input data
- Use HTTPS for sensitive data (client supports it)
- Server listens on all interfaces (0.0.0.0) - configure firewall appropriately
- Validate server certificates when making requests
- Implement proper authentication for your endpoints

### Performance Tips

- Use HEAD requests to check resources without downloading content
- Server uses non-blocking I/O for better concurrency
- Client implements 30-second timeout to prevent hanging
- Close connections properly with `stopServer()`

## Compatibility

The HTTP module is available in both interpreter mode and compiled binaries. All functionality uses real network operations with no mock implementations.
