# Convert Module Documentation

The `convert` module provides data type conversion and string manipulation utilities for Neutron programs. These functions are also available globally without importing the module.

## Usage

```neutron
use convert;

// Module functions (optional - these are also available globally)
var ascii = convert.char_to_int("A");
var character = convert.int_to_char(65);

// Global functions (available without import)
var ascii2 = char_to_int("B");
var character2 = int_to_char(66);
```

## Character and ASCII Functions

### `char_to_int(character)`
Converts a single character to its ASCII/Unicode code point value.

**Parameters:**
- `character` (string): A single-character string

**Returns:** Number representing the ASCII/Unicode value

**Example:**
```neutron
var aValue = char_to_int("A");
say(aValue); // Output: 65

var zValue = char_to_int("z");
say(zValue); // Output: 122

var spaceValue = char_to_int(" ");
say(spaceValue); // Output: 32

var digitValue = char_to_int("5");
say(digitValue); // Output: 53

// Special characters
var newlineValue = char_to_int("\n");
say(newlineValue); // Output: 10

var tabValue = char_to_int("\t");
say(tabValue); // Output: 9
```

**Throws:** Runtime error if argument is not a single-character string

**Use Cases:**
- Character encoding and decoding
- Text processing and analysis
- Implementing custom string algorithms
- Password strength analysis

---

### `int_to_char(number)`
Converts an ASCII/Unicode code point value to its corresponding character.

**Parameters:**
- `number` (number): ASCII/Unicode value (typically 0-127 for standard ASCII)

**Returns:** String containing the single character

**Example:**
```neutron
var letterA = int_to_char(65);
say(letterA); // Output: A

var letterz = int_to_char(122);
say(letterz); // Output: z

var space = int_to_char(32);
say("'" + space + "'"); // Output: ' '

var digit = int_to_char(53);
say(digit); // Output: 5

// Building strings from ASCII values
var hello = "";
hello = hello + int_to_char(72); // H
hello = hello + int_to_char(101); // e
hello = hello + int_to_char(108); // l
hello = hello + int_to_char(108); // l
hello = hello + int_to_char(111); // o
say(hello); // Output: Hello
```

**Throws:** Runtime error if argument is not a number

**Use Cases:**
- Generating characters from numeric codes
- Text encryption and decryption
- Creating dynamic strings
- Character set conversions

## String Manipulation Functions

### `string_length(text)`
Returns the length of a string (number of characters).

**Parameters:**
- `text` (string): The string to measure

**Returns:** Number representing the length of the string

**Example:**
```neutron
var len1 = string_length("Hello");
say(len1); // Output: 5

var len2 = string_length("Hello, World!");
say(len2); // Output: 13

var emptyLen = string_length("");
say(emptyLen); // Output: 0

var spaceLen = string_length("   ");
say(spaceLen); // Output: 3

// Unicode characters count as single characters
var unicodeLen = string_length("café");
say(unicodeLen); // Output: 4
```

**Throws:** Runtime error if argument is not a string

**Use Cases:**
- Input validation (checking minimum/maximum lengths)
- String processing loops
- Memory allocation calculations
- Text formatting and alignment

---

### `string_get_char_at(text, index)`
Retrieves the character at a specific position in a string.

**Parameters:**
- `text` (string): The string to extract from
- `index` (number): Zero-based index of the character to retrieve

**Returns:** String containing the single character at the specified position

**Example:**
```neutron
var text = "Hello, World!";

var firstChar = string_get_char_at(text, 0);
say(firstChar); // Output: H

var fifthChar = string_get_char_at(text, 4);
say(fifthChar); // Output: o

var lastChar = string_get_char_at(text, 12);
say(lastChar); // Output: !

// Loop through string characters
var word = "Neutron";
var length = string_length(word);
for (var i = 0; i < length; i = i + 1) {
    var char = string_get_char_at(word, i);
    say("Character " + i + ": " + char);
}
// Output:
// Character 0: N
// Character 1: e
// Character 2: u
// Character 3: t
// Character 4: r
// Character 5: o
// Character 6: n
```

**Throws:** 
- Runtime error if first argument is not a string
- Runtime error if second argument is not a number
- Runtime error if index is out of bounds (negative or >= string length)

**Use Cases:**
- String parsing and analysis
- Character-by-character processing
- Text validation
- String searching algorithms

## Type Conversion Functions

### `str(value)`
Converts a number to its string representation.

**Parameters:**
- `value` (number): The number to convert to string

**Returns:** String representation of the number

**Example:**
```neutron
var numStr = str(42);
say(numStr); // Output: 42
say("Number: " + numStr); // Output: Number: 42

var floatStr = str(3.14159);
say(floatStr); // Output: 3.14159

var negativeStr = str(-17);
say(negativeStr); // Output: -17

var zeroStr = str(0);
say(zeroStr); // Output: 0

// Large numbers
var largeStr = str(1234567.89);
say(largeStr); // Output: 1234567.89
```

**Throws:** Runtime error if argument is not a number

**Use Cases:**
- Number formatting for display
- String concatenation with numbers
- Logging and debugging
- Creating numeric identifiers as strings

---

### `int(text)`
Converts a string to its numeric representation.

**Parameters:**
- `text` (string): String containing a valid number

**Returns:** Number parsed from the string

**Example:**
```neutron
var num1 = int("42");
say(num1 + 8); // Output: 50

var num2 = int("3.14159");
say(num2); // Output: 3.14159

var negative = int("-17");
say(negative); // Output: -17

var zero = int("0");
say(zero); // Output: 0

// With whitespace (may or may not be handled)
var trimmed = int("  123  ");
say(trimmed); // Behavior depends on implementation
```

**Throws:** 
- Runtime error if argument is not a string
- Runtime error if string does not contain a valid number
- Runtime error if number is out of range

**Use Cases:**
- User input processing
- Configuration file parsing
- Data conversion from text files
- String-based calculations

## Binary Conversion Functions

### `int_to_bin(number)`
Converts an integer to its binary string representation.

**Parameters:**
- `number` (number): Integer to convert to binary

**Returns:** String containing the binary representation (32-bit)

**Example:**
```neutron
var bin1 = int_to_bin(10);
say(bin1); // Output: 00000000000000000000000000001010

var bin2 = int_to_bin(255);
say(bin2); // Output: 00000000000000000000000011111111

var bin3 = int_to_bin(0);
say(bin3); // Output: 00000000000000000000000000000000

var bin4 = int_to_bin(1);
say(bin4); // Output: 00000000000000000000000000000001

// Negative numbers (two's complement)
var binNeg = int_to_bin(-1);
say(binNeg); // Output: 11111111111111111111111111111111
```

**Throws:** Runtime error if argument is not a number

**Use Cases:**
- Binary data analysis
- Bit manipulation visualization
- Educational purposes (showing binary representation)
- Low-level programming tasks

---

### `bin_to_int(binaryString)`
Converts a binary string to its integer representation.

**Parameters:**
- `binaryString` (string): String containing binary digits (0s and 1s)

**Returns:** Number representing the integer value

**Example:**
```neutron
var num1 = bin_to_int("1010");
say(num1); // Output: 10

var num2 = bin_to_int("11111111");
say(num2); // Output: 255

var num3 = bin_to_int("0");
say(num3); // Output: 0

var num4 = bin_to_int("1");
say(num4); // Output: 1

// Longer binary strings
var num5 = bin_to_int("101010101010");
say(num5); // Output: 2730
```

**Throws:** 
- Runtime error if argument is not a string
- Runtime error if string contains non-binary characters
- Runtime error if binary value is out of range

**Use Cases:**
- Binary data processing
- Bit manipulation operations
- Protocol implementation
- Educational binary arithmetic

## Common Usage Patterns

### Text Processing and Analysis
```neutron
fun analyzeText(text) {
    var length = string_length(text);
    var uppercase = 0;
    var lowercase = 0;
    var digits = 0;
    var spaces = 0;
    var special = 0;
    
    say("Analyzing text: '" + text + "'");
    say("Length: " + length + " characters");
    
    for (var i = 0; i < length; i = i + 1) {
        var char = string_get_char_at(text, i);
        var code = char_to_int(char);
        
        if (code >= 65 and code <= 90) {  // A-Z
            uppercase = uppercase + 1;
        } else if (code >= 97 and code <= 122) {  // a-z
            lowercase = lowercase + 1;
        } else if (code >= 48 and code <= 57) {  // 0-9
            digits = digits + 1;
        } else if (code == 32) {  // space
            spaces = spaces + 1;
        } else {
            special = special + 1;
        }
    }
    
    say("Uppercase letters: " + uppercase);
    say("Lowercase letters: " + lowercase);
    say("Digits: " + digits);
    say("Spaces: " + spaces);
    say("Special characters: " + special);
}

analyzeText("Hello World 123!");
```

### Simple Encryption (Caesar Cipher)
```neutron
fun caesarEncode(text, shift) {
    var result = "";
    var length = string_length(text);
    
    for (var i = 0; i < length; i = i + 1) {
        var char = string_get_char_at(text, i);
        var code = char_to_int(char);
        
        // Only shift letters
        if (code >= 65 and code <= 90) {  // A-Z
            var shifted = ((code - 65 + shift) % 26) + 65;
            result = result + int_to_char(shifted);
        } else if (code >= 97 and code <= 122) {  // a-z
            var shifted = ((code - 97 + shift) % 26) + 97;
            result = result + int_to_char(shifted);
        } else {
            result = result + char;  // Keep non-letters unchanged
        }
    }
    
    return result;
}

var original = "Hello World";
var encoded = caesarEncode(original, 3);
say("Original: " + original);
say("Encoded: " + encoded);  // Output: Khoor Zruog
```

### Binary Data Processing
```neutron
fun processBinaryData(numbers) {
    say("Processing binary representations:");
    
    // Convert numbers to binary and analyze
    for (var i = 0; i < 5; i = i + 1) {  // Process first 5 numbers
        var num = numbers[i];  // Assuming array access works
        var binary = int_to_bin(num);
        
        // Count 1s and 0s
        var ones = 0;
        var zeros = 0;
        var binLength = string_length(binary);
        
        for (var j = 0; j < binLength; j = j + 1) {
            var bit = string_get_char_at(binary, j);
            if (bit == "1") {
                ones = ones + 1;
            } else {
                zeros = zeros + 1;
            }
        }
        
        say("Number: " + num);
        say("Binary: " + binary);
        say("Ones: " + ones + ", Zeros: " + zeros);
        say("");
    }
}

// Example usage (simplified without actual arrays)
say("Binary analysis for number 10:");
var binary10 = int_to_bin(10);
say("10 in binary: " + binary10);
var decimal = bin_to_int("1010");
say("1010 in decimal: " + decimal);
```

### String Building and Formatting
```neutron
fun buildFormattedString(title, value, width) {
    var result = title + ": ";
    var valueStr = str(value);
    var padding = width - string_length(result) - string_length(valueStr);
    
    // Add spaces for alignment
    for (var i = 0; i < padding; i = i + 1) {
        result = result + " ";
    }
    
    result = result + valueStr;
    return result;
}

say(buildFormattedString("Score", 1250, 20));
say(buildFormattedString("Level", 5, 20));
say(buildFormattedString("Health", 85, 20));
// Output:
// Score:          1250
// Level:             5
// Health:           85
```

### Input Validation
```neutron
fun validateInput(input, minLength, maxLength) {
    var length = string_length(input);
    
    if (length < minLength) {
        say("Error: Input too short (minimum " + minLength + " characters)");
        return false;
    }
    
    if (length > maxLength) {
        say("Error: Input too long (maximum " + maxLength + " characters)");
        return false;
    }
    
    // Check for valid characters
    for (var i = 0; i < length; i = i + 1) {
        var char = string_get_char_at(input, i);
        var code = char_to_int(char);
        
        // Allow letters, numbers, and basic punctuation
        if (!(code >= 32 and code <= 126)) {
            say("Error: Invalid character at position " + i);
            return false;
        }
    }
    
    say("Input validation passed");
    return true;
}

validateInput("Hello123", 5, 50);      // Valid
validateInput("Hi", 5, 50);            // Too short
validateInput("ValidInput!", 5, 50);   // Valid
```

### Character Frequency Analysis
```neutron
fun analyzeCharacterFrequency(text) {
    var length = string_length(text);
    say("Character frequency analysis for: '" + text + "'");
    
    // Simple frequency counting (would use arrays in full implementation)
    var aCount = 0;
    var eCount = 0;
    var spaceCount = 0;
    
    for (var i = 0; i < length; i = i + 1) {
        var char = string_get_char_at(text, i);
        var lowerChar = char;  // In full implementation, would convert to lowercase
        
        if (char == "a" or char == "A") {
            aCount = aCount + 1;
        } else if (char == "e" or char == "E") {
            eCount = eCount + 1;
        } else if (char == " ") {
            spaceCount = spaceCount + 1;
        }
    }
    
    say("'a' appears " + aCount + " times");
    say("'e' appears " + eCount + " times");
    say("Space appears " + spaceCount + " times");
}

analyzeCharacterFrequency("The apple and the eagle");
```

## Error Handling Best Practices

```neutron
// Safe character conversion
fun safeCharToInt(char) {
    if (string_length(char) != 1) {
        say("Error: Expected single character, got: '" + char + "'");
        return -1;
    }
    return char_to_int(char);
}

// Safe string indexing
fun safeGetCharAt(text, index) {
    var length = string_length(text);
    if (index < 0 or index >= length) {
        say("Error: Index " + index + " out of bounds for string of length " + length);
        return "";
    }
    return string_get_char_at(text, index);
}

// Safe number parsing
fun safeParseInt(text) {
    if (string_length(text) == 0) {
        say("Error: Cannot parse empty string");
        return 0;
    }
    
    // Basic validation (check for digits)
    var length = string_length(text);
    for (var i = 0; i < length; i = i + 1) {
        var char = string_get_char_at(text, i);
        var code = char_to_int(char);
        if (!(code >= 48 and code <= 57) and char != "-" and char != ".") {
            say("Error: Invalid character '" + char + "' in number string");
            return 0;
        }
    }
    
    return int(text);
}
```

## Performance Considerations

- String operations can be expensive for very large strings
- Character-by-character processing requires loops and may be slow
- Binary conversions are computed operations, not table lookups
- Consider caching results if performing the same conversions repeatedly

## Compatibility

All convert module functions are available both as module functions and as global built-in functions. They work consistently in both interpreter mode and compiled binaries, providing reliable data conversion across all execution modes of Neutron programs.
