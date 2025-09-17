# Nexus Module

Nexus is a REST API framework for building web services in Neutron.

## Usage

To use the Nexus module in your Neutron scripts, include it at the top of your file:

```neutron
use nexus;
```

## Classes

### Router
The main class for defining API routes.

**Methods:**
- `get(path, handler)` - Register a GET route
- `post(path, handler)` - Register a POST route
- `put(path, handler)` - Register a PUT route
- `delete(path, handler)` - Register a DELETE route
- `handle_request(method, path, data)` - Handle an incoming request

### Request
Represents an HTTP request.

### Response
Represents an HTTP response.

**Methods:**
- `status_code(code)` - Set response status code
- `set_header(key, value)` - Set a response header
- `send(data)` - Set response body
- `json(data)` - Send JSON response

### MockServer
A mock server for testing APIs.

**Methods:**
- `listen()` - Start the server
- `handle_request(method, path, data)` - Handle an incoming request

## Functions

- `create_router()` - Create a new Router instance
- `create_response()` - Create a new Response instance
- `create_server(router, port)` - Create a new Server instance
- `parse_json(data)` - Parse JSON data (simplified)
- `stringify_json(data)` - Stringify data to JSON (simplified)
- `parse_params(url)` - Parse URL parameters

## Examples

```neutron
use nexus;

// Create a new router
var router = nexus.create_router();

// Define routes
router.get("/users", fun(data) {
    return {
        "status": 200,
        "headers": {"Content-Type": "application/json"},
        "body": "[{\"id\": 1, \"name\": \"Alice\"}]"
    };
});

// Create and start the server
var server = nexus.create_server(router, 3000);
server.listen();
```

For more examples, see:
- [nexus_api_example.nt](examples/nexus_api_example.nt) - Basic REST API example
- [nexus_advanced_example.nt](examples/nexus_advanced_example.nt) - Advanced example with middleware