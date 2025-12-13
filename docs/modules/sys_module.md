# Sys Module Documentation

The `sys` module provides system-level operations, file I/O functionality, environment access, and process control for Neutron programs.

## Usage

**Important:** As of the latest version, the `sys` module requires explicit import using the `use` statement:

```neutron
use sys;

// Now you can use sys functions
var currentDir = sys.cwd();
sys.write("example.txt", "Hello, world!");
```

The module is no longer auto-loaded, ensuring explicit dependencies and better code organization.

## Implementation Status

The `sys` module is fully implemented as a native C++ module located in `libs/sys/native.cpp`. All documented features are available and working.

## File Operations

### `sys.read(filepath)`
Reads the entire contents of a file as a string.

**Parameters:**
- `filepath` (string): Path to the file to read

**Returns:** String containing the file contents

**Example:**
```neutron
use sys;

var content = sys.read("data.txt");
say("File content: " + content);
```

**Throws:** Runtime error if file cannot be opened or read

---

### `sys.write(filepath, content)`
Writes content to a file, overwriting existing content.

**Parameters:**
- `filepath` (string): Path to the file to write
- `content` (string): Content to write to the file

**Returns:** `true` on success

**Example:**
```neutron
use sys;

sys.write("output.txt", "Hello, Neutron!");
say("File written successfully");
```

**Throws:** Runtime error if file cannot be opened or written

---

### `sys.append(filepath, content)`
Appends content to the end of a file.

**Parameters:**
- `filepath` (string): Path to the file to append to
- `content` (string): Content to append to the file

**Returns:** `true` on success

**Example:**
```neutron
use sys;

sys.write("log.txt", "First entry\n");
sys.append("log.txt", "Second entry\n");
var content = sys.read("log.txt");
say(content); // "First entry\nSecond entry\n"
```

**Throws:** Runtime error if file cannot be opened or written

---

### `sys.cp(source, destination)`
Copies a file from source to destination.

**Parameters:**
- `source` (string): Path to the source file
- `destination` (string): Path to the destination file

**Returns:** `true` on success

**Example:**
```neutron
use sys;

sys.write("original.txt", "Important data");
sys.cp("original.txt", "backup.txt");
say("File copied successfully");
```

**Throws:** Runtime error if copy operation fails

---

### `sys.mv(source, destination)`
Moves/renames a file from source to destination.

**Parameters:**
- `source` (string): Path to the source file
- `destination` (string): Path to the destination file

**Returns:** `true` on success

**Example:**
```neutron
use sys;

sys.write("temp.txt", "Temporary data");
sys.mv("temp.txt", "permanent.txt");
say("File moved successfully");
```

**Throws:** Runtime error if move operation fails

---

### `sys.rm(filepath)`
Removes/deletes a file.

**Parameters:**
- `filepath` (string): Path to the file to remove

**Returns:** `true` if file was removed, `false` if file didn't exist

**Example:**
```neutron
use sys;

sys.write("unwanted.txt", "Delete me");
var success = sys.rm("unwanted.txt");
if (success) {
    say("File deleted successfully");
}
```

**Throws:** Runtime error if deletion fails

---

### `sys.exists(path)`
Checks if a file or directory exists.

**Parameters:**
- `path` (string): Path to check

**Returns:** `true` if path exists, `false` otherwise

**Example:**
```neutron
use sys;

if (sys.exists("config.txt")) {
    var config = sys.read("config.txt");
    say("Config loaded: " + config);
} else {
    say("Config file not found");
}
```

## Directory Operations

### `sys.mkdir(path)`
Creates a directory.

**Parameters:**
- `path` (string): Path of the directory to create

**Returns:** `true` if directory was created

**Example:**
```neutron
use sys;

sys.mkdir("data");
sys.write("data/file.txt", "Content in subdirectory");
```

**Throws:** Runtime error if directory creation fails

---

### `sys.rmdir(path, recursive?)`
Removes a directory.

**Parameters:**
- `path` (string): Path of the directory to remove
- `recursive` (boolean, optional): If `true`, removes directory and all contents recursively. Default is `false` (only removes empty directories)

**Returns:** `true` if directory was removed

**Examples:**
```neutron
use sys;

// Remove empty directory
sys.mkdir("temp_dir");
sys.rmdir("temp_dir");

// Remove directory with contents (recursive)
sys.mkdir("data");
sys.write("data/file.txt", "content");
sys.rmdir("data", true);  // Removes directory and all files inside
```

**Safety:** Cannot delete directories outside the current working directory. Attempts to delete parent directories or absolute paths outside cwd will throw an error.

**Throws:** Runtime error if directory removal fails or if trying to delete outside current working directory

## Advanced File Operations

### `sys.listdir(path)`
Lists files and directories in the specified path.

**Parameters:**
- `path` (string): Path to the directory to list

**Returns:** Array of strings containing file/directory names

**Example:**
```neutron
use sys;

var files = sys.listdir(".");
for (var i = 0; i < files.length; i = i + 1) {
    say(files[i]);
}
```

**Throws:** Runtime error if listing fails

---

### `sys.stat(path)`
Gets information about a file or directory.

**Parameters:**
- `path` (string): Path to the file/directory

**Returns:** Object with properties:
- `size` (number): Size in bytes
- `mtime` (string): Last modification time
- `is_file` (boolean): True if it is a regular file
- `is_directory` (boolean): True if it is a directory

**Example:**
```neutron
use sys;

var info = sys.stat("file.txt");
say("Size: " + info.size);
say("Modified: " + info.mtime);
```

**Throws:** Runtime error if file not found

---

### `sys.chmod(path, mode)`
Changes the permissions of a file or directory.

**Parameters:**
- `path` (string): Path to the file/directory
- `mode` (number): Permission mode (integer representation of permissions, e.g., 420 for 0644)

**Returns:** `true` on success

**Example:**
```neutron
use sys;

// Set read/write for owner, read for others (0644 octal = 420 decimal)
sys.chmod("file.txt", 420); 
```

**Throws:** Runtime error if operation fails

---

### `sys.tmpfile()`
Creates a temporary file path.

**Returns:** String containing a unique temporary file path

**Example:**
```neutron
use sys;

var tmp = sys.tmpfile();
sys.write(tmp, "temp data");
```

**Throws:** Runtime error if fails

## System Information

### `sys.cwd()`
Gets the current working directory.

**Returns:** String containing the current working directory path

**Example:**
```neutron
use sys;

var currentDir = sys.cwd();
say("Working in: " + currentDir);
```

**Throws:** Runtime error if unable to get current directory

---

### `sys.chdir(path)`
Changes the current working directory.

**Parameters:**
- `path` (string): Path to change to

**Returns:** `true` on success

**Example:**
```neutron
use sys;

var originalDir = sys.cwd();
sys.chdir("subdirectory");
say("Changed to: " + sys.cwd());
sys.chdir(originalDir);
```

**Throws:** Runtime error if directory change fails

---

### `sys.env(name)`
Gets the value of an environment variable.

**Parameters:**
- `name` (string): Name of the environment variable

**Returns:** String value of the environment variable, or `nil` if not found

**Example:**
```neutron
use sys;

var home = sys.env("HOME");
if (home != nil) {
    say("Home directory: " + home);
}

var path = sys.env("PATH");
say("PATH: " + path);
```

---

### `sys.args()`
Gets command line arguments passed to the program.

**Returns:** Array of command line arguments

**Example:**
```neutron
use sys;

var args = sys.args();
say("Program arguments: ");
// Note: Currently returns an empty array - command line argument passing
// will be implemented in a future version
```

**Implementation Note:** The underlying VM does not yet pass command line arguments to scripts. This function currently returns an empty array but the interface is provided for future compatibility.

---

### `sys.info()`
Gets system information.

**Returns:** Object containing system information with the following properties:
- `platform`: Operating system ("linux", "macos", "windows", or "unknown")
- `arch`: CPU architecture ("x64", "x86", "arm64", or "unknown")
- `cwd`: Current working directory

**Example:**
```neutron
use sys;

var info = sys.info();
say("Platform: " + info.platform);
say("Architecture: " + info.arch);
say("Current directory: " + info.cwd);
```

**Note:** Access object properties using dot notation (e.g., `info.platform`), not square bracket notation.

## User Input

### `sys.input([prompt])`
Reads a line of input from the user.

**Parameters:**
- `prompt` (string, optional): Prompt to display to the user

**Returns:** String containing the user input

**Example:**
```neutron
use sys;

var name = sys.input("Enter your name: ");
say("Hello, " + name + "!");

var age = sys.input();
say("You entered: " + age);
```

## Process Control

### `sys.checkpoint(filepath)`
Saves the current execution state (stack, heap, globals) to a file. The program can be resumed later from this exact point using `neutron --resume <filepath>`.

**Parameters:**
- `filepath` (string): Path to save the snapshot file (e.g., "backup.snap")

**Returns:** `true` on success

**Example:**
```neutron
use sys;

// Save state
sys.checkpoint("backup.snap");

// If resumed, execution continues here
say("Resumed!");
```

**See Also:** [Durable Execution Guide](../guides/durable-execution.md)

---

### `sys.exit([code])`
Exits the program with an optional exit code.

**Parameters:**
- `code` (number, optional): Exit code (default: 0)

**Returns:** Never returns (exits the program)

**Example:**
```neutron
use sys;

if (!sys.exists("config.txt")) {
    say("Error: Config file not found!");
    sys.exit(1);
}

say("Program completed successfully");
sys.exit(0);
```

---

### `sys.exec(command)`
Executes a shell command and returns the output.

**Parameters:**
- `command` (string): Shell command to execute

**Returns:** String containing the command output

**Example:**
```neutron
use sys;

var output = sys.exec("ls -la");
say("Directory listing:");
say(output);

var date = sys.exec("date");
say("Current date: " + date);
```

**Throws:** Runtime error if command execution fails

## Common Usage Patterns

### File Processing
```neutron
use sys;

// Read, process, and write data
if (sys.exists("input.txt")) {
    var data = sys.read("input.txt");
    var processedData = "Processed: " + data;
    sys.write("output.txt", processedData);
    say("File processed successfully");
}
```

### Configuration Management
```neutron
use sys;

// Load configuration with fallback
var configFile = "app.config";
var config;

if (sys.exists(configFile)) {
    config = sys.read(configFile);
} else {
    config = "default_config_here";
    sys.write(configFile, config);
}

say("Using config: " + config);
```

### Environment-Based Behavior
```neutron
use sys;

var environment = sys.env("NODE_ENV");
if (environment == "development") {
    say("Running in development mode");
} else if (environment == "production") {
    say("Running in production mode");
} else {
    say("Environment not specified");
}
```

### Backup and Cleanup
```neutron
use sys;

// Create backup and clean up
var dataFile = "important_data.txt";
if (sys.exists(dataFile)) {
    sys.cp(dataFile, dataFile + ".backup");
    var newData = sys.input("Enter new data: ");
    sys.write(dataFile, newData);
    say("Data updated, backup created");
}
```

## Error Handling

Most sys module functions throw runtime errors when they fail. Always be prepared to handle these:

```neutron
use sys;

// Safe file operations
if (sys.exists("data.txt")) {
    var content = sys.read("data.txt");
    say("Content: " + content);
} else {
    say("File does not exist");
}

// Safe environment variable access
var dbUrl = sys.env("DATABASE_URL");
if (dbUrl != nil) {
    say("Database URL configured");
} else {
    say("Warning: DATABASE_URL not set");
}
```

## Platform Compatibility

The sys module works on Linux, macOS, and Windows, but some behaviors may vary:

- File paths use forward slashes on Unix-like systems, backslashes on Windows
- Environment variables may have different names across platforms
- Some shell commands in `sys.exec()` may not be available on all platforms

Always test your code on your target platforms when using system-dependent features.

## Build System

As of the latest version, Neutron uses CMake for cross-platform builds. The sys module is compiled as part of the Neutron runtime library.

To build Neutron with the sys module:

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

The compiled binaries will be:
- `build/neutron` - The Neutron interpreter
- `build/libneutron_runtime.so` - The runtime library (includes sys module)
- `neutron` and `libneutron_runtime.so` - Copies in the project root for convenience

## Module Loading

The sys module uses lazy loading - it's only initialized when you explicitly use `use sys;` in your code. This ensures:

1. Faster startup time for scripts that don't need sys
2. Explicit dependencies in your code
3. Better memory management
4. Clear module boundaries

All built-in modules (`sys`, `math`, `json`, `http`, `time`, `fmt`) follow this pattern.

## Changes from Previous Versions

**Breaking Changes:**
- sys module now requires explicit `use sys;` statement (previously auto-loaded)
- Object property access uses dot notation only (e.g., `obj.property` not `obj["property"]`)

**Improvements:**
- Full implementation of all documented sys functions
- Better error messages
- Cross-platform compatibility (Windows, Linux, macOS)
- CMake build system for easier compilation
