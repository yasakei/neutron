# Common Pitfalls and Mistakes

This guide covers common mistakes developers make when writing Neutron code and how to avoid them.

## 1. Using `.length()` Instead of `.length`

### ❌ Wrong
```neutron
var text = "Hello";
say(text.length());  // RuntimeError: Can only call functions and classes

var arr = [1, 2, 3];
say(arr.length());   // RuntimeError: Can only call functions and classes
```

### ✅ Correct
```neutron
var text = "Hello";
say(text.length);    // 5

var arr = [1, 2, 3];
say(arr.length);     // 3
```

### Why This Matters
In Neutron, `.length` is a **property**, not a method. Attempting to call it as a function `()` will result in a runtime error.

**Key Points**:
- Strings have a `.length` property
- Arrays have a `.length` property  
- Do NOT add parentheses: use `.length`, not `.length()`

### String Interpolation
```neutron
var items = [1, 2, 3, 4, 5];
say("Total items: ${items.length}");  // ✅ Correct

// NOT this:
say("Total items: ${items.length()}");  // ❌ Error
```

### In Loops
```neutron
var data = [10, 20, 30, 40];
var i = 0;
while (i < data.length) {  // ✅ Correct
    say(data[i]);
    i = i + 1;
}
```

## 2. Forgetting to use Modules

### ❌ Wrong
```neutron
// Trying to use sys functions without useing
var content = sys.read("file.txt");  // Error: Undefined variable 'sys'
```

### ✅ Correct
```neutron
use sys;

var content = sys.read("file.txt");
say(content);
```

### Common Module Names
- `use sys;` - File I/O and system operations
- `use json;` - JSON parsing and manipulation
- `use http;` - HTTP client and server
- `use regex;` - Regular expressions
- `use math;` - Mathematical functions
- `use arrays;` - Array utilities
- `use time;` - Time and date functions
- `use fmt;` - Type conversion and formatting

## 3. Incorrect String Concatenation in Loops

### ❌ Inefficient
```neutron
var result = "";
var i = 0;
while (i < 1000) {
    result = result + "text";  // Creates many intermediate strings
    i = i + 1;
}
```

### ✅ Better (Use Array Join)
```neutron
var parts = [];
var i = 0;
while (i < 1000) {
    parts.push("text");
    i = i + 1;
}
var result = parts.join("");  // More efficient
```

## 4. Modifying Arrays While Iterating

### ❌ Dangerous
```neutron
var items = [1, 2, 3, 4, 5];
var i = 0;
while (i < items.length) {
    if (items[i] > 2) {
        items.pop();  // Modifying array during iteration
    }
    i = i + 1;
}
```

### ✅ Safer Approach
```neutron
var items = [1, 2, 3, 4, 5];
var filtered = [];
var i = 0;
while (i < items.length) {
    if (items[i] <= 2) {
        filtered.push(items[i]);
    }
    i = i + 1;
}
items = filtered;
```

Or use the `.filter()` method:
```neutron
var items = [1, 2, 3, 4, 5];
items = items.filter(fun(x) { return x <= 2; });
```

## 5. Mixing Up Assignment `=` and Comparison `==`

### ❌ Wrong
```neutron
var x = 10;
if (x = 5) {  // This assigns 5 to x, not comparing!
    say("x is 5");
}
```

### ✅ Correct
```neutron
var x = 10;
if (x == 5) {  // Comparison
    say("x is 5");
}
```

## 6. Not Checking Return Values

### ❌ Risky
```neutron
use sys;

var content = sys.read("missing.txt");  // Returns nil if file doesn't exist
say(content.length);  // Error if content is nil
```

### ✅ Safe
```neutron
use sys;

var content = sys.read("missing.txt");
if (content != nil) {
    say("File length: ${content.length}");
} else {
    say("File not found");
}
```

## 7. Incorrect JSON Access

### ❌ Wrong
```neutron
use json;

var data = json.parse('{"name": "Alice", "age": 30}');
say(data.name);  // May not work with bracket syntax in some contexts
```

### ✅ Correct
```neutron
use json;

var data = json.parse('{"name": "Alice", "age": 30}');
say(data["name"]);  // Always use bracket notation for JSON objects
say(data["age"]);
```

## 8. Forgetting `return` in Functions

### ❌ Returns `nil` Unexpectedly
```neutron
fun add(a, b) {
    a + b;  // Result is calculated but not returned!
}

var result = add(5, 3);
say(result);  // nil
```

### ✅ Correct
```neutron
fun add(a, b) {
    return a + b;
}

var result = add(5, 3);
say(result);  // 8
```

## 9. Using Wrong Comparison Operators

### Equality Operators
- `==` - Equals
- `!=` - Not equals
- `<` - Less than
- `>` - Greater than
- `<=` - Less than or equal
- `>=` - Greater than or equal

### Logical Operators
- `and` - Logical AND (not `&&`)
- `or` - Logical OR (not `||`)
- `not` - Logical NOT (not `!`)

### ❌ Wrong
```neutron
if (x > 5 && y < 10) {  // Syntax error
    say("Valid range");
}
```

### ✅ Correct
```neutron
if (x > 5 and y < 10) {
    say("Valid range");
}
```

## 10. Incorrect Module Function Calls

Some functions require the module prefix, even if useed:

### ❌ Wrong
```neutron
use arrays;

var arr = [3, 1, 2];
arr.sort();  // Error: sort is not a method
```

### ✅ Correct
```neutron
use arrays;

var arr = [3, 1, 2];
arrays.sort(arr);  // Pass array as parameter
say(arr);  // [1, 2, 3]
```

**Note**: Built-in array methods like `.push()`, `.pop()`, `.map()`, `.filter()` work directly on arrays. Module functions like `arrays.sort()` require the module prefix.

## 8. HTTP Handler Must Return a Value

When using `http.createServer(handler)`, your handler function **must** return a value. If it returns `nil` (or nothing), the server will respond with a 500 Internal Server Error.

### ❌ Wrong
```neutron
fun handler(req) {
    say("Request received");
    // No return statement -> returns nil
}
```

### ✅ Correct
```neutron
fun handler(req) {
    say("Request received");
    return "OK"; // Returns string body
}
```

## Quick Reference Checklist

- [ ] Use `.length` (property) not `.length()` (method call)
- [ ] use all required modules at the top of your file
- [ ] Use `==` for comparison, `=` for assignment
- [ ] Use `and`/`or`/`not` for logical operations
- [ ] Always use `return` to return values from functions
- [ ] Check for `nil` before accessing properties
- [ ] Use bracket notation `obj["key"]` for JSON objects
- [ ] Don't modify arrays while iterating over them
- [ ] Remember which operations are methods vs. module functions

## See Also

- [Quick Start Guide](quickstart.md) - Get started with Neutron
- [Type System](../type-system.md) - Understanding Neutron types
- [Language Reference](../reference/language_reference.md) - Complete syntax guide
- [Module Documentation](../readme.md#module-api-documentation) - All available modules
