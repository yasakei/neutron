# JSON Module Documentation

The `json` module provides JSON parsing, serialization, file I/O, and object manipulation functionality for Neutron programs. All file operations use real filesystem I/O with proper error handling.

## Usage

```neutron
use json;

// Now you can use JSON functions
var jsonString = json.stringify({"name": "Alice", "age": 30});
var parsed = json.parse(jsonString);
```

## Functions

### Core Functions

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
- Objects → Neutron objects (supports `obj["key"]` access)
- Arrays → Neutron arrays (supports standard array methods like `map`, `filter`, etc.)

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
say("Name: " + parsed["name"]); // Output: Name: Bob
say("First Skill: " + parsed["skills"][0]); // Output: First Skill: coding
```

**Throws:** 
- Runtime error if JSON string is malformed
- Runtime error if unexpected end of input
- Runtime error for invalid JSON syntax

---

### `json.get(object, key)`
Retrieves a value from a JSON object by key.

> [!NOTE]
> You can also use direct indexing syntax `object["key"]` which is more idiomatic.

**Parameters:**
- `object` (object): The JSON object to get value from
- `key` (string): The key to look up

**Returns:** Value associated with the key, or `nil` if key doesn't exist

**Example:**
```neutron
use json;

var data = json.parse("{\"user\":\"alice\",\"score\":100,\"active\":true}");

// Using direct indexing (Preferred)
var username = data["user"];
say("Username: " + username); // Output: Username: alice

// Using json.get()
var score = json.get(data, "score");
say("Score: " + score); // Output: Score: 100

var missing = data["nonexistent"];
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
    
    var dbHost = loadedConfig["database"]["host"];
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

var name = userData["name"];
var email = userData["email"];

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

## File I/O Functions

### `json.readFile(filepath)`
Reads and parses a JSON file from the filesystem.

**Parameters:**
- `filepath` (string): Path to the JSON file to read

**Returns:** Parsed JSON object

**Features:**
- Uses real filesystem I/O with `std::ifstream`
- Automatically parses the file contents as JSON
- Throws descriptive errors for missing files or invalid JSON

**Example:**
```neutron
use json;

// Read configuration from file
var config = json.readFile("config.json");
say("App name: " + json.get(config, "appName"));
say("Version: " + json.get(config, "version"));

// Read user data
var users = json.readFile("data/users.json");
var user1 = json.get(users, "0");
say("First user: " + json.get(user1, "name"));
```

**Throws:**
- Runtime error if file doesn't exist
- Runtime error if file cannot be opened
- Runtime error if JSON is malformed

---

### `json.writeFile(filepath, data, [pretty])`
Writes a value to a JSON file.

**Parameters:**
- `filepath` (string): Path to write the JSON file
- `data` (any): Data to serialize and write
- `pretty` (boolean, optional): Format with indentation (default: `false`)

**Returns:** `true` on success

**Features:**
- Uses real filesystem I/O with `std::ofstream`
- Automatically stringifies the data
- Creates or overwrites the file
- Optional pretty formatting with indentation

**Example:**
```neutron
use json;

// Save configuration
var config = json.parse("{}");
json.set(config, "appName", "MyApp");
json.set(config, "version", "1.0.0");
json.set(config, "debug", true);

json.writeFile("config.json", config);
say("Configuration saved!");

// Save with pretty formatting
var userData = json.parse("{\"name\": \"Alice\", \"age\": 30}");
json.writeFile("user.json", userData, true);
say("User data saved (pretty formatted)!");
```

**Throws:**
- Runtime error if file cannot be opened for writing
- Runtime error if directory doesn't exist

---

## Advanced Object Manipulation

### `json.set(object, key, value)`
Sets a property on a JSON object.

**Parameters:**
- `object` (object): JSON object to modify
- `key` (string): Property key
- `value` (any): Value to set

**Returns:** The modified object

**Example:**
```neutron
use json;

var obj = json.parse("{\"name\": \"Alice\"}");
json.set(obj, "age", 30);
say(json.stringify(obj)); // {"name":"Alice","age":30}
```

---

### `json.has(object, key)`
Checks if an object has a specific key.

**Parameters:**
- `object` (object): JSON object to check
- `key` (string): Property key to look for

**Returns:** `true` if key exists, `false` otherwise

**Example:**
```neutron
use json;

var obj = json.parse("{\"name\": \"Alice\"}");
if (json.has(obj, "name")) {
    say("Name exists");
}
```

---

### `json.keys(object)`
Returns an array of all keys in an object.

**Parameters:**
- `object` (object): JSON object

**Returns:** Array of key strings

**Example:**
```neutron
use json;

var obj = json.parse("{\"name\": \"Alice\", \"age\": 30}");
var keys = json.keys(obj);
for (var i = 0; i < keys.length; i = i + 1) {
    say(keys[i]); // name, age
}
```

---

### `json.values(object)`
Returns an array of all values in an object.

**Parameters:**
- `object` (object): JSON object

**Returns:** Array of values

**Example:**
```neutron
use json;

var obj = json.parse("{\"name\": \"Alice\", \"age\": 30}");
var values = json.values(obj);
say(values[0]); // Alice
say(values[1]); // 30
```

---

### `json.merge(obj1, obj2)`
Merges two objects, with obj2 properties overwriting obj1.

**Parameters:**
- `obj1` (object): First object
- `obj2` (object): Second object (takes precedence)

**Returns:** New merged object

**Example:**
```neutron
use json;

var obj1 = json.parse("{\"name\": \"Alice\", \"age\": 30}");
var obj2 = json.parse("{\"age\": 31, \"city\": \"NYC\"}");
var merged = json.merge(obj1, obj2);
say(json.stringify(merged)); // {"name":"Alice","age":31,"city":"NYC"}
```

---

### `json.delete(object, key)`
Removes a property from an object.

**Parameters:**
- `object` (object): JSON object to modify
- `key` (string): Property key to remove

**Returns:** The modified object

**Example:**
```neutron
use json;

var obj = json.parse("{\"name\": \"Alice\", \"age\": 30}");
json.delete(obj, "age");
say(json.stringify(obj)); // {"name":"Alice"}
```

---

### `json.clone(value)`
Creates a deep copy of a value.

**Parameters:**
- `value` (any): Value to clone

**Returns:** Deep copy of the value

**Example:**
```neutron
use json;

var obj1 = json.parse("{\"name\": \"Alice\"}");
var obj2 = json.clone(obj1);
json.set(obj2, "name", "Bob");
say(obj1.name); // Alice (unchanged)
say(obj2.name); // Bob
```

---

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
