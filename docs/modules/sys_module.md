# Sys Module Documentation

The `sys` module provides system-level operations, file I/O functionality, environment access, and process control for Neutron programs.

## Usage

```neutron
use sys;

// Now you can use sys functions
var currentDir = sys.cwd();
sys.write("example.txt", "Hello, world!");
```

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

### `sys.rmdir(path)`
Removes an empty directory.

**Parameters:**
- `path` (string): Path of the directory to remove

**Returns:** `true` if directory was removed

**Example:**
```neutron
use sys;

sys.mkdir("temp_dir");
sys.rmdir("temp_dir");
say("Directory removed");
```

**Throws:** Runtime error if directory removal fails

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
say("Program arguments: " + args);
```

**Note:** Currently returns an empty array in this implementation

---

### `sys.info()`
Gets system information.

**Returns:** Object containing system information

**Example:**
```neutron
use sys;

var info = sys.info();
say("Platform: " + info["platform"]);
say("Architecture: " + info["arch"]);
say("Current directory: " + info["cwd"]);
```

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
