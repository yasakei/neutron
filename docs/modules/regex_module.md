# Regex Module Documentation

The `regex` module provides powerful regular expression functionality for pattern matching, searching, and text manipulation in Neutron programs. It uses **C++ std::regex** (ECMAScript grammar) for efficient and reliable regex operations.

## Usage

```neutron
use regex;

// Test if text matches a pattern
if (regex.test("hello@example.com", "\\w+@\\w+\\.\\w+")) {
    say("Valid email format!");
}
```

## Functions

### Core Functions

### `regex.test(text, pattern)`
Tests if the entire string matches a regex pattern (full match).

**Parameters:**
- `text` (string): The text to test
- `pattern` (string): Regular expression pattern

**Returns:** `true` if the entire string matches, `false` otherwise

**Example:**
```neutron
use regex;

// Exact match
if (regex.test("hello", "hello")) {
    say("Exact match!"); // This prints
}

// Pattern match
if (regex.test("12345", "\\d+")) {
    say("All digits!"); // This prints
}

// Partial match fails with test()
if (!regex.test("hello world", "hello")) {
    say("test() requires full match"); // This prints
}
```

---

### `regex.search(text, pattern)`
Searches for a pattern anywhere in the text (partial match).

**Parameters:**
- `text` (string): The text to search
- `pattern` (string): Regular expression pattern

**Returns:** `true` if pattern is found anywhere, `false` otherwise

**Example:**
```neutron
use regex;

var text = "The quick brown fox";

if (regex.search(text, "quick")) {
    say("Found 'quick'!"); // This prints
}

if (regex.search(text, "\\bfox\\b")) {
    say("Found word 'fox'!"); // This prints
}

if (!regex.search(text, "slow")) {
    say("'slow' not found"); // This prints
}
```

---

### `regex.find(text, pattern)`
Finds the first match and returns detailed information including position and capture groups.

**Parameters:**
- `text` (string): The text to search
- `pattern` (string): Regular expression pattern (can include capture groups)

**Returns:** Object with match details, or `nil` if no match

**Return Object Properties:**
- `matched` (string): The matched text
- `position` (number): Starting position of the match
- `length` (number): Length of the matched text
- `groups` (array): Array of capture groups (index 0 is the full match)

**Example:**
```neutron
use regex;

// Simple find
var text = "Email: test@example.com";
var result = regex.find(text, "[a-z]+@[a-z]+\\.[a-z]+");
if (result != nil) {
    say("Found: " + result.matched); // Found: test@example.com
    say("Position: " + result.position); // Position: 7
}

// With capture groups
var date = "Date: 2025-12-01";
var dateResult = regex.find(date, "(\\d+)-(\\d+)-(\\d+)");
if (dateResult != nil) {
    say("Year: " + dateResult.groups[1]); // Year: 2025
    say("Month: " + dateResult.groups[2]); // Month: 12
    say("Day: " + dateResult.groups[3]); // Day: 01
}
```

---

### `regex.findAll(text, pattern)`
Finds all matches in the text and returns an array of match objects.

**Parameters:**
- `text` (string): The text to search
- `pattern` (string): Regular expression pattern

**Returns:** Array of match objects (same structure as `find()`)

**Example:**
```neutron
use regex;

var text = "Phone: 123-456-7890, Fax: 098-765-4321";
var numbers = regex.findAll(text, "\\d{3}-\\d{3}-\\d{4}");

say("Found " + numbers.length + " numbers"); // Found 2 numbers

var i = 0;
while (i < numbers.length) {
    say("Number " + (i + 1) + ": " + numbers[i].matched);
    i = i + 1;
}
// Number 1: 123-456-7890
// Number 2: 098-765-4321
```

---

### `regex.replace(text, pattern, replacement)`
Replaces all matches of a pattern with a replacement string.

**Parameters:**
- `text` (string): The text to process
- `pattern` (string): Regular expression pattern
- `replacement` (string): Replacement text (supports backreferences like `$1`, `$2`)

**Returns:** New string with replacements

**Example:**
```neutron
use regex;

// Simple replacement
var text = "Hello World";
var result = regex.replace(text, "World", "Neutron");
say(result); // Hello Neutron

// Pattern replacement
var numbers = "1-2-3-4-5";
var result2 = regex.replace(numbers, "-", ", ");
say(result2); // 1, 2, 3, 4, 5

// Using backreferences
var phone = "1234567890";
var formatted = regex.replace(phone, "(\\d{3})(\\d{3})(\\d{4})", "($1) $2-$3");
say(formatted); // (123) 456-7890
```

---

### `regex.split(text, pattern)`
Splits a string by a regex pattern.

**Parameters:**
- `text` (string): The text to split
- `pattern` (string): Regular expression pattern to split on

**Returns:** Array of string parts

**Example:**
```neutron
use regex;

// Split by multiple delimiters
var text = "apple,banana;orange|grape";
var parts = regex.split(text, "[,;|]");

var i = 0;
while (i < parts.length) {
    say(parts[i]); // apple, banana, orange, grape
    i = i + 1;
}

// Split by whitespace
var sentence = "The   quick  brown   fox";
var words = regex.split(sentence, "\\s+");
say(words.length); // 4
```

---

## Utility Functions

### `regex.isValid(pattern)`
Tests if a regex pattern is valid.

**Parameters:**
- `pattern` (string): Regular expression pattern to validate

**Returns:** `true` if valid, `false` if invalid

**Example:**
```neutron
use regex;

if (regex.isValid("\\d+")) {
    say("Valid pattern");
}

if (!regex.isValid("[invalid")) {
    say("Invalid pattern - unmatched bracket");
}
```

---

### `regex.escape(text)`
Escapes special regex characters in a string to make it literal.

**Parameters:**
- `text` (string): Text to escape

**Returns:** Escaped string safe for use in regex patterns

**Special characters escaped:** `\ ^ $ . | ? * + ( ) [ ] { }`

**Example:**
```neutron
use regex;

var literal = "a.b*c?d+e";
var escaped = regex.escape(literal);
say(escaped); // a\\.b\\*c\\?d\\+e

// Use escaped string in pattern
var text = "The price is $5.00";
var price = "5.00";
if (regex.search(text, regex.escape("$" + price))) {
    say("Found the price!");
}
```

---

## Common Patterns

### Email Validation
```neutron
use regex;

var email = "user@example.com";
var pattern = "[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}";
if (regex.test(email, pattern)) {
    say("Valid email");
}
```

### URL Extraction
```neutron
use regex;

var text = "Visit https://example.com and http://test.org";
var urls = regex.findAll(text, "https?://[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}");

var i = 0;
while (i < urls.length) {
    say("URL: " + urls[i].matched);
    i = i + 1;
}
```

### Phone Number Formatting
```neutron
use regex;

var phone = "1234567890";
var formatted = regex.replace(phone, "(\\d{3})(\\d{3})(\\d{4})", "($1) $2-$3");
say(formatted); // (123) 456-7890
```

### Extract Words
```neutron
use regex;

var text = "Hello, world! How are you?";
var words = regex.findAll(text, "\\w+");

var i = 0;
while (i < words.length) {
    say(words[i].matched);
    i = i + 1;
}
```

### Date Parsing
```neutron
use regex;

var text = "Meeting on 2025-12-01";
var result = regex.find(text, "(\\d{4})-(\\d{2})-(\\d{2})");

if (result != nil) {
    say("Year: " + result.groups[1]);
    say("Month: " + result.groups[2]);
    say("Day: " + result.groups[3]);
}
```

## Regex Syntax Reference

The regex module uses ECMAScript (JavaScript) regex syntax:

### Character Classes
- `.` - Any character except newline
- `\d` - Digit [0-9]
- `\D` - Non-digit
- `\w` - Word character [a-zA-Z0-9_]
- `\W` - Non-word character
- `\s` - Whitespace
- `\S` - Non-whitespace

### Quantifiers
- `*` - 0 or more
- `+` - 1 or more
- `?` - 0 or 1
- `{n}` - Exactly n times
- `{n,}` - n or more times
- `{n,m}` - Between n and m times

### Anchors
- `^` - Start of string
- `$` - End of string
- `\b` - Word boundary
- `\B` - Non-word boundary

### Groups
- `(...)` - Capture group
- `(?:...)` - Non-capturing group
- `[...]` - Character class
- `[^...]` - Negated character class

### Special
- `|` - Alternation (OR)
- `\` - Escape character

## Error Handling

Invalid regex patterns throw runtime errors:

```neutron
use regex;

// This will throw an error
var result = regex.find("text", "[invalid");
// RuntimeError: Invalid regex pattern: ...
```

## Performance Tips

1. **Compile once, use multiple times**: For repeated searches with the same pattern, the regex is compiled each time. Consider restructuring your code to minimize redundant pattern usage.

2. **Use anchors**: Patterns with `^` and `$` can be faster as they limit where the regex engine searches.

3. **Avoid excessive backtracking**: Patterns like `(a+)+` can cause performance issues. Use atomic groups or possessive quantifiers when available.

4. **Test patterns**: Use `regex.isValid()` to validate patterns before using them in production code.

## Compatibility

The regex module is available in both interpreter mode and compiled binaries. It uses the C++ standard library `<regex>` with ECMAScript grammar for consistent, portable behavior across platforms.
