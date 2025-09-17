# Known Issues

This document lists known issues and limitations in the Neutron interpreter and language.

## Language Issues

### 1. Logical OR Operator (`||`) Parsing Bug
**Status:** Open
**Description:** The `||` (logical OR) operator causes an "Unexpected character" runtime error when used in conditional statements.
**Workaround:** Use separate `if` statements instead of the `||` operator:

```neutron
// Instead of this (which causes an error):
if (x < 1 || x > 10) {
    x = 1;
}

// Use this:
var valid = true;
if (x < 1) {
    valid = false;
}
if (x > 10) {
    valid = false;
}
if (valid == false) {
    x = 1;
}
```

### 2. Logical AND Operator (`&&`) Parsing Bug
**Status:** Unknown
**Description:** The `&&` (logical AND) operator may have similar parsing issues as the `||` operator.
**Workaround:** Use separate `if` statements when combining multiple conditions.

### 3. Empty String to Integer Conversion
**Status:** Open
**Description:** Converting an empty string to an integer with `int()` causes a runtime error.
**Workaround:** Always check for empty strings before conversion:

```neutron
var input_str = sys.input("Enter a number: ");
if (input_str == "") {
    input_str = "0";  // or another default value
}
var number = int(input_str);
```

## Interpreter Issues

### 1. Object Literal Memory Management
**Status:** Under investigation
**Description:** Complex programs using multiple object literals may experience memory issues or unexpected behavior.

### 2. Nested Conditional Statements
**Status:** Under investigation
**Description:** Deeply nested conditional statements may cause variable scoping issues in some cases.

## Module Issues

### 1. JSON Module Limitations
**Status:** Known limitation
**Description:** The `json.get()` function may not work correctly with deeply nested objects or arrays.

### 2. Sys Module Input Handling
**Status:** Known limitation
**Description:** The `sys.input()` function returns an empty string when the user presses Enter without typing, which requires special handling.

## Workarounds and Best Practices

1. Always validate user input before converting to other types
2. Avoid using `||` and `&&` operators; use separate if statements instead
3. Handle empty string inputs explicitly
4. Test complex object literal usage thoroughly
5. Keep conditional nesting to a reasonable depth

## Reporting New Issues

If you encounter any issues not listed here, please report them to the project maintainers with:
- A minimal code example that reproduces the issue
- The exact error message
- Your operating system and Neutron version