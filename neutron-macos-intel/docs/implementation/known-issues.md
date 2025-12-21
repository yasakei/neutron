# Known Issues

This document lists known issues and limitations in the Neutron interpreter and language.

## Recently Fixed Issues ✅

### Logical OR and AND Operators
**Status:** ✅ FIXED  
**Description:** The `or` and `and` operators (lowercase keywords) work correctly in all contexts.  
**Note:** Use `and` and `or` (lowercase), not `&&` or `||` which are not implemented.

```neutron
// This works:
if (x < 1 or x > 10) {
    x = 1;
}

// This also works:
if (x > 0 and x < 100) {
    say("Valid range");
}

// Works in loops too:
while (i < 10 and j > 0) {
    i = i + 1;
    j = j - 1;
}
```

### Empty String to Integer Conversion
**Status:** ✅ FIXED  
**Description:** Converting an empty string to an integer with `convert.int()` now returns `0` instead of throwing an error.

```neutron
use fmt;

var empty = "";
var num = fmt.to_int(empty);  // Returns 0
say(num);  // Output: 0
```

### While Loop Stack Overflow
**Status:** ✅ FIXED  
**Description:** While loops with many iterations previously caused stack overflow errors due to improper stack management in the interpreter. This has been fixed in the latest version.

```neutron
// This now works with any number of iterations:
var i = 0;
while (i < 1000000) {
    // do something
    i = i + 1;
}
say("Success! Completed " + fmt.to_str(i) + " iterations.");
```

### sys Module Functions
**Status:** ✅ FIXED  
**Description:** All sys module functions are now fully implemented and working:
- File operations: `read`, `write`, `append`, `cp`, `mv`, `rm`, `exists`
- Directory operations: `mkdir`, `rmdir`  
- System info: `cwd`, `chdir`, `env`, `args`, `info`
- Process control: `exit`, `exec`

```neutron
use sys;

sys.write("test.txt", "Hello!");
var content = sys.read("test.txt");
say(content);
```

### Break and Continue Statements
**Status:** ✅ FIXED (October 4, 2025)  
**Description:** `break` and `continue` statements are now fully implemented and working in both for-loops and while-loops.

```neutron
// Break example:
for (var i = 0; i < 10; i = i + 1) {
    if (i == 5) {
        break;  // Exit loop
    }
    say(i);
}

// Continue example:
for (var i = 0; i < 10; i = i + 1) {
    if (i % 2 == 0) {
        continue;  // Skip even numbers
    }
    say(i);
}
```

### Command Line Arguments
**Status:** ✅ FIXED (October 4, 2025)  
**Description:** Command line arguments are now passed to scripts and accessible via `sys.args()`.

```neutron
use sys;

var args = sys.args();
say("Script name: " + args[0]);
say("First argument: " + args[1]);
```

### Modulo Operator (%)
**Status:** ✅ FIXED (October 4, 2025)  
**Description:** The `%` operator is now available as an alias for modulo operation.

```neutron
var remainder = 10 % 3;  // Returns 1
say("10 % 3 = " + remainder);
```

### String Interpolation
**Status:** ✅ IMPLEMENTED (October 4, 2025)  
**Description:** String interpolation with `${...}` syntax is now fully implemented. You can embed variables, literals, and expressions directly in strings.

```neutron
var name = "World";
var count = 42;
say("Hello, ${name}! Count: ${count}");  // Output: Hello, World! Count: 42

var x = 5;
var y = 10;
say("Sum: ${x + y}");  // Output: Sum: 15
```

### Enhanced Array Operations
**Status:** ✅ IMPLEMENTED (October 4, 2025)  
**Description:** Comprehensive array methods are now available, including higher-order functions like map, filter, and find.

```neutron
var arr = [1, 2, 3, 4, 5];

// Property
say(arr.length);  // 5

// Basic methods
say(arr.slice(1, 3));        // [2, 3]
say(arr.indexOf(3));         // 2
say(arr.join(", "));         // "1, 2, 3, 4, 5"

// Advanced methods with callbacks
fun double(x) {
    return x * 2;
}
var doubled = arr.map(double);  // [2, 4, 6, 8, 10]

fun isEven(x) {
    return x % 2 == 0;
}
var evens = arr.filter(isEven);  // [2, 4]

fun greaterThan3(x) {
    return x > 3;
}
var found = arr.find(greaterThan3);  // 4
```

## Current Known Issues

### 1. Not-Equal Operator (!=) Produces Incorrect Results  
**Status:** ⚠️ KNOWN BUG  
**Severity:** Medium  
**Description:** The `!=` operator produces inverted/incorrect boolean results in comparisons.

**Example:**
```neutron
var x = 5;
if (x != 5) {
    say("This incorrectly executes!");  // Should not run, but does
}

if (x != 6) {
    say("This should execute");  // Works correctly
}

// Nil comparisons also affected
var y = nil;
if (y != nil) {
    say("This incorrectly executes!");  // Should not run
}
```

**Workaround:** Use `==` with logical negation:
```neutron
// Instead of: if (x != 5)
var isEqual = x == 5;
if (!isEqual) {
    say("x is not 5");
}

// Or for nil checks:
var isNil = response == nil;
if (!isNil) {
    say("Response is not nil");
}
```

**Status:** This is a parser/compiler bug that will be fixed in a future release.

### 2. Symbol Operators Not Implemented  
**Status:** By Design  
**Description:** C-style operators `&&`, `||`, `++`, `--` are not implemented. Use keyword equivalents instead.

**Workaround:** Use the keyword operators:
- Use `and` instead of `&&`
- Use `or` instead of `||`
- Use `x = x + 1` instead of `x++`
- Use `x = x - 1` instead of `x--`

### 3. Else-If Chain Complexity
**Status:** Under Investigation  
**Description:** Extremely complex else-if chains (10+ levels) may cause performance issues.

**Workaround:** Keep else-if chains reasonable or use nested if-else:

```neutron
// Reasonable else-if chain (works fine):
if (percentage >= 90) {
  say("Outstanding!");
} else if (percentage >= 75) {
  say("Great job!");
} else if (percentage >= 60) {
  say("Good effort!");
} else {
  say("Keep trying!");
}
```



## Interpreter Issues

### 1. Array Indexing with Variables
**Status:** Limited Support  
**Description:** Complex expressions as array indices may not work in all contexts.

**Workaround:** Use simple variables or pre-calculated indices:

```neutron
var arr = [1, 2, 3, 4, 5];

// Works:
var idx = 2;
say(arr[idx]);

// If complex expression doesn't work, pre-calculate:
var calculatedIdx = (x + y) * 2;
say(arr[calculatedIdx]);
```

## Module Issues

### 1. Square Bracket Property Access
**Status:** By Design  
**Description:** JsonObject properties must be accessed using dot notation, not square brackets.

**Workaround:** Use dot notation:

```neutron
use sys;

var info = sys.info();

// Works:
say(info.platform);
say(info.arch);

// Doesn't work:
// say(info["platform"]);  // Error: Only arrays support index access
```

### 2. Module Import Requirement
**Status:** By Design  
**Description:** All built-in modules must be explicitly imported with `use modulename;` before use.

**Note:** This is intentional for lazy loading. Always import modules at the top of your file:

```neutron
use sys;
use json;
use fmt;

// Now you can use them
sys.write("file.txt", "data");
```

**Workaround:** Ensure the `build/` directory with the static library is included in release packages.

**Status:** Fix requires updating release workflow to include build directory in all release packages.

## Box Package Manager Issues

### 1. Windows API Compatibility
**Status:** ⚠️ KNOWN BUG  
**Severity:** High  
**Description:** The box package manager does not work properly on Windows, and cannot build external libraries when used locally. This affects module development and distribution on Windows platforms.

**Workaround:** Use alternative build methods on Windows or use WSL.

**Status:** Fix requires addressing Windows API compatibility in box module build system.

## Best Practices

1. Always import required modules at the top of your file
2. Use `and` and `or` keywords for logical operations
3. Handle empty string inputs when using `fmt.to_int()`
4. Use dot notation for object property access
5. Keep else-if chains reasonable (<10 levels)
6. Use environment variables instead of command line args for now

## Reporting New Issues

If you encounter any issues not listed here, please report them to the project maintainers with:
- A minimal code example that reproduces the issue
- The exact error message
- Your operating system and Neutron version