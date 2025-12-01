# JSON Module Documentation

The `json` module provides JSON parsing and serialization functionality for Neutron programs, allowing you to work with JSON data structures.

## Usage

```neutron
use json;

// Now you can use JSON functions
var jsonString = json.stringify({"name": "Alice", "age": 30});
var parsed = json.parse(jsonString);
```

## Functions

### `json.stringify(value, [pretty])`
Converts a Neutron value to a JSON string representation.

**Parameters:**
- `value` (any): The value to convert to JSON
- `pretty` (boolean, optional): If `true`, formats JSON with indentation and newlines (default: `false`)

**Returns:** String containing the JSON representation

**Supported Value Types:**
- `nil` → `"null"`
- `boolean` → `"true"` or `"false"`
- `number` → numeric string (e.g., `"42"`, `"3.14"`)
- `string` → escaped JSON string (e.g., `"\"hello\""`)
- `object` → JSON object with key-value pairs

**Example:**
```neutron
use json;

// Basic values
say(json.stringify(nil));        // Output: null
say(json.stringify(true));       // Output: true
say(json.stringify(42));         // Output: 42
say(json.stringify("hello"));    // Output: "hello"

// Objects
var person = {
    "name": "Alice",
    "age": 30,
    "active": true
};

var compact = json.stringify(person);
say(compact); // Output: {"name":"Alice","age":30,"active":true}

var pretty = json.stringify(person, true);
say(pretty);
// Output:
// {
//   "name": "Alice",
//   "age": 30,
//   "active": true
// }
```

**Special Characters:** The function properly escapes special characters in strings:
```neutron
use json;

var text = "Line 1\nLine 2\tTabbed\"Quoted\"";
var escaped = json.stringify(text);
say(escaped); // Output: "Line 1\nLine 2\tTabbed\"Quoted\""
```

**Throws:** Runtime error if value cannot be serialized

---

### `json.parse(jsonString)`
Parses a JSON string and returns the corresponding Neutron value.

**Parameters:**
- `jsonString` (string): Valid JSON string to parse

**Returns:** Neutron value representing the parsed JSON

**Supported JSON Types:**
- `null` → `nil`
- `true`/`false` → boolean values
- Numbers → number values
- Strings → string values
- Objects → Neutron objects
- Arrays → Neutron array objects

**Example:**
```neutron
use json;

// Basic values
var nullValue = json.parse("null");
say(nullValue == nil); // Output: true

var boolValue = json.parse("true");
say(boolValue); // Output: true

var numValue = json.parse("42.5");
say(numValue); // Output: 42.5

var strValue = json.parse("\"hello world\"");
say(strValue); // Output: hello world

// Objects
var jsonStr = "{\"name\":\"Bob\",\"age\":25,\"skills\":[\"coding\",\"design\"]}";
var parsed = json.parse(jsonStr);
say("Name: " + json.get(parsed, "name")); // Output: Name: Bob
```

**Throws:** 
- Runtime error if JSON string is malformed
- Runtime error if unexpected end of input
- Runtime error for invalid JSON syntax

---

### `json.get(object, key)`
Retrieves a value from a JSON object by key.

**Parameters:**
- `object` (object): The JSON object to get value from
- `key` (string): The key to look up

**Returns:** Value associated with the key, or `nil` if key doesn't exist

**Example:**
```neutron
use json;

var data = json.parse("{\"user\":\"alice\",\"score\":100,\"active\":true}");

var username = json.get(data, "user");
say("Username: " + username); // Output: Username: alice

var score = json.get(data, "score");
say("Score: " + score); // Output: Score: 100

var missing = json.get(data, "nonexistent");
say(missing == nil); // Output: true
```

**Throws:** 
- Runtime error if first argument is not an object
- Runtime error if second argument is not a string

## Common Usage Patterns

### Configuration Files
```neutron
use json;
use sys;

// Save configuration
var config = {
    "database": {
        "host": "localhost",
        "port": 5432,
        "name": "myapp"
    },
    "debug": true,
    "version": "1.0.0"
};

var configJson = json.stringify(config, true);
sys.write("config.json", configJson);

// Load configuration
if (sys.exists("config.json")) {
    var configData = sys.read("config.json");
    var loadedConfig = json.parse(configData);
    
    var dbHost = json.get(json.get(loadedConfig, "database"), "host");
    say("Database host: " + dbHost);
}
```

### API Data Processing
```neutron
use json;
use http;

// Process API response
var response = http.get("https://api.example.com/users/1");
var userData = json.parse(response["body"]);

var name = json.get(userData, "name");
var email = json.get(userData, "email");

say("User: " + name + " (" + email + ")");

// Create API request payload
var requestData = {
    "name": "New User",
    "email": "user@example.com",
    "active": true
};

var payload = json.stringify(requestData);
var createResponse = http.post("https://api.example.com/users", payload);
```

### Data Storage and Retrieval
```neutron
use json;
use sys;

// Save user data
var users = [
    {"id": 1, "name": "Alice", "role": "admin"},
    {"id": 2, "name": "Bob", "role": "user"},
    {"id": 3, "name": "Carol", "role": "user"}
];

var usersJson = json.stringify(users, true);
sys.write("users.json", usersJson);

// Load and filter users
var userData = sys.read("users.json");
var userList = json.parse(userData);

// Find admin users (simplified - would require array iteration in practice)
say("User data loaded successfully");
```

### Settings Management
```neutron
use json;
use sys;

class Settings {
    var data;
    
    fun initialize() {
        this.load();
    }
    
    fun load() {
        if (sys.exists("settings.json")) {
            var settingsData = sys.read("settings.json");
            this.data = json.parse(settingsData);
        } else {
            this.data = {
                "theme": "dark",
                "language": "en",
                "notifications": true
            };
            this.save();
        }
    }
    
    fun save() {
        var settingsJson = json.stringify(this.data, true);
        sys.write("settings.json", settingsJson);
    }
    
    fun get(key) {
        return json.get(this.data, key);
    }
    
    fun set(key, value) {
        // In a full implementation, you'd update the object
        // For now, just demonstrate the concept
        say("Setting " + key + " to " + value);
        this.save();
    }
}

var settings = Settings();
settings.initialize();
say("Theme: " + settings.get("theme"));
```

### Data Validation
```neutron
use json;

fun validateUser(jsonString) {
    var user = json.parse(jsonString);
    
    var name = json.get(user, "name");
    var email = json.get(user, "email");
    var age = json.get(user, "age");
    
    if (name == nil) {
        say("Error: Name is required");
        return false;
    }
    
    if (email == nil) {
        say("Error: Email is required");
        return false;
    }
    
    if (age == nil or age < 0) {
        say("Error: Valid age is required");
        return false;
    }
    
    say("User validation passed");
    return true;
}

var validUser = "{\"name\":\"Alice\",\"email\":\"alice@example.com\",\"age\":30}";
var invalidUser = "{\"name\":\"Bob\"}";

validateUser(validUser);   // Validation passes
validateUser(invalidUser); // Validation fails
```

## Error Handling

JSON operations can fail for various reasons. Always be prepared to handle errors:

```neutron
use json;

// Safe JSON parsing
fun safeJsonParse(jsonString) {
    // In a full implementation, you'd use try-catch
    // For now, validate basic structure
    if (jsonString == "") {
        say("Error: Empty JSON string");
        return nil;
    }
    
    return json.parse(jsonString);
}

// Safe JSON access
fun safeJsonGet(object, key) {
    var value = json.get(object, key);
    if (value == nil) {
        say("Warning: Key '" + key + "' not found");
    }
    return value;
}

var data = safeJsonParse("{\"name\":\"Alice\"}");
if (data != nil) {
    var name = safeJsonGet(data, "name");
    var age = safeJsonGet(data, "age"); // Will show warning
}
```

## JSON Limitations

Current implementation limitations:

1. **Arrays**: Basic array support is available but may have limited functionality
2. **Nested Objects**: Deep nesting is supported but access requires multiple `json.get()` calls
3. **Unicode**: Basic Unicode escape sequences are handled but full Unicode support may be limited
4. **Number Precision**: Very large numbers may lose precision due to double-precision floating-point representation

## Best Practices

1. **Always validate JSON structure** before accessing nested properties
2. **Use pretty printing** for configuration files and human-readable output
3. **Handle missing keys gracefully** using `nil` checks
4. **Escape special characters** when building JSON strings manually
5. **Keep JSON structures simple** for better performance and reliability

## Performance Considerations

- Large JSON strings may take longer to parse
- Pretty-printing adds overhead for formatting
- Nested object access requires multiple function calls
- Consider caching parsed JSON objects if used frequently

## Compatibility

The JSON module is available in both interpreter mode and compiled binaries, providing consistent JSON processing across all execution modes of Neutron programs.
