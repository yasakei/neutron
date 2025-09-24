# Time Module Documentation

The `time` module provides time-related functionality for Neutron programs, including timestamp generation, time formatting, and sleep operations.

## Usage

```neutron
use time;

// Get current timestamp
var now = time.now();
say("Current time: " + now);

// Format timestamp
var formatted = time.format(now);
say("Formatted: " + formatted);
```

## Functions

### `time.now()`
Returns the current system time as a timestamp in milliseconds since the Unix epoch (January 1, 1970, 00:00:00 UTC).

**Parameters:** None

**Returns:** Number representing the current timestamp in milliseconds

**Example:**
```neutron
use time;

var timestamp = time.now();
say("Current timestamp: " + timestamp);
// Output: Current timestamp: 1703097625000 (example)

// Use for timing operations
var startTime = time.now();
// ... do some work ...
var endTime = time.now();
var duration = endTime - startTime;
say("Operation took " + duration + " milliseconds");
```

**Use Cases:**
- Performance timing and benchmarking
- Creating unique identifiers based on time
- Logging with timestamps
- Rate limiting and throttling

---

### `time.format(timestamp, [format])`
Formats a timestamp into a human-readable string representation.

**Parameters:**
- `timestamp` (number): Timestamp in milliseconds to format
- `format` (string, optional): Format string using strftime format specifiers (default: `"%Y-%m-%d %H:%M:%S"`)

**Returns:** String containing the formatted time

**Default Format:** `"YYYY-MM-DD HH:MM:SS"` (e.g., "2024-01-15 14:30:25")

**Common Format Specifiers:**
- `%Y` - 4-digit year (2024)
- `%y` - 2-digit year (24)
- `%m` - Month as number (01-12)
- `%d` - Day of month (01-31)
- `%H` - Hour in 24-hour format (00-23)
- `%I` - Hour in 12-hour format (01-12)
- `%M` - Minute (00-59)
- `%S` - Second (00-59)
- `%p` - AM/PM
- `%A` - Full weekday name (Monday)
- `%a` - Abbreviated weekday name (Mon)
- `%B` - Full month name (January)
- `%b` - Abbreviated month name (Jan)

**Example:**
```neutron
use time;

var now = time.now();

// Default format
var defaultFormat = time.format(now);
say(defaultFormat); // Output: 2024-01-15 14:30:25

// Date only
var dateOnly = time.format(now, "%Y-%m-%d");
say(dateOnly); // Output: 2024-01-15

// Time only
var timeOnly = time.format(now, "%H:%M:%S");
say(timeOnly); // Output: 14:30:25

// Custom format with weekday
var customFormat = time.format(now, "%A, %B %d, %Y at %I:%M %p");
say(customFormat); // Output: Monday, January 15, 2024 at 02:30 PM

// ISO 8601 format
var isoFormat = time.format(now, "%Y-%m-%dT%H:%M:%S");
say(isoFormat); // Output: 2024-01-15T14:30:25

// Filename-safe format
var filenameFormat = time.format(now, "%Y%m%d_%H%M%S");
say(filenameFormat); // Output: 20240115_143025
```

**Throws:** Runtime error if timestamp is not a number or format string is invalid

---

### `time.sleep(milliseconds)`
Pauses program execution for the specified number of milliseconds.

**Parameters:**
- `milliseconds` (number): Number of milliseconds to sleep

**Returns:** `nil`

**Example:**
```neutron
use time;

say("Starting process...");
time.sleep(1000); // Sleep for 1 second
say("Process continued after 1 second");

// Short delay
time.sleep(500); // Sleep for 0.5 seconds
say("Half second later");

// Longer delay
say("Waiting 3 seconds...");
time.sleep(3000);
say("Done waiting!");
```

**Use Cases:**
- Adding delays between operations
- Rate limiting requests
- Creating animations or timed sequences
- Preventing overwhelming external services
- Debugging with deliberate pauses

**Throws:** Runtime error if argument is not a number

## Common Usage Patterns

### Performance Timing
```neutron
use time;

class Timer {
    var startTime;
    var endTime;
    
    fun start() {
        this.startTime = time.now();
        say("Timer started");
    }
    
    fun stop() {
        this.endTime = time.now();
        var duration = this.endTime - this.startTime;
        say("Timer stopped. Duration: " + duration + "ms");
        return duration;
    }
    
    fun elapsed() {
        var currentTime = time.now();
        return currentTime - this.startTime;
    }
}

var timer = Timer();
timer.start();

// Simulate some work
time.sleep(1500);

var duration = timer.stop();
// Output: Timer stopped. Duration: 1500ms
```

### Logging with Timestamps
```neutron
use time;
use sys;

class Logger {
    var logFile;
    
    fun initialize(filename) {
        this.logFile = filename;
    }
    
    fun log(level, message) {
        var timestamp = time.now();
        var formattedTime = time.format(timestamp, "%Y-%m-%d %H:%M:%S");
        var logEntry = "[" + formattedTime + "] " + level + ": " + message + "\n";
        
        sys.append(this.logFile, logEntry);
        say(logEntry);
    }
    
    fun info(message) {
        this.log("INFO", message);
    }
    
    fun error(message) {
        this.log("ERROR", message);
    }
    
    fun debug(message) {
        this.log("DEBUG", message);
    }
}

var logger = Logger();
logger.initialize("app.log");

logger.info("Application started");
logger.debug("Processing user input");
logger.error("Failed to connect to database");
```

### Rate Limiting
```neutron
use time;

class RateLimiter {
    var lastRequestTime;
    var minInterval; // Minimum milliseconds between requests
    
    fun initialize(intervalMs) {
        this.minInterval = intervalMs;
        this.lastRequestTime = 0;
    }
    
    fun canMakeRequest() {
        var currentTime = time.now();
        var timeSinceLastRequest = currentTime - this.lastRequestTime;
        return timeSinceLastRequest >= this.minInterval;
    }
    
    fun waitForNextRequest() {
        var currentTime = time.now();
        var timeSinceLastRequest = currentTime - this.lastRequestTime;
        var waitTime = this.minInterval - timeSinceLastRequest;
        
        if (waitTime > 0) {
            say("Rate limit: waiting " + waitTime + "ms");
            time.sleep(waitTime);
        }
        
        this.lastRequestTime = time.now();
    }
}

// Allow one request per second
var limiter = RateLimiter();
limiter.initialize(1000);

// Make multiple requests with rate limiting
for (var i = 0; i < 3; i = i + 1) {
    limiter.waitForNextRequest();
    say("Making request " + (i + 1));
    // ... make actual request here ...
}
```

### Scheduled Tasks Simulation
```neutron
use time;

class Scheduler {
    var tasks;
    
    fun initialize() {
        // In a real implementation, this would be an array
        say("Scheduler initialized");
    }
    
    fun scheduleTask(delayMs, taskName) {
        say("Scheduling task '" + taskName + "' to run in " + delayMs + "ms");
        time.sleep(delayMs);
        this.executeTask(taskName);
    }
    
    fun executeTask(taskName) {
        var timestamp = time.now();
        var formattedTime = time.format(timestamp, "%H:%M:%S");
        say("Executing task '" + taskName + "' at " + formattedTime);
    }
}

var scheduler = Scheduler();
scheduler.initialize();

scheduler.scheduleTask(1000, "Backup Database");
scheduler.scheduleTask(2000, "Send Email Reports");
scheduler.scheduleTask(500, "Check System Health");
```

### Time-based File Names
```neutron
use time;
use sys;

fun createTimestampedFile(baseName, content) {
    var timestamp = time.now();
    var timeStr = time.format(timestamp, "%Y%m%d_%H%M%S");
    var filename = baseName + "_" + timeStr + ".txt";
    
    sys.write(filename, content);
    say("Created file: " + filename);
    return filename;
}

// Create backup files with timestamps
var backupFile = createTimestampedFile("backup", "Important data here");
// Creates: backup_20240115_143025.txt

var logFile = createTimestampedFile("log", "Application log data");
// Creates: log_20240115_143026.txt
```

### Session Duration Tracking
```neutron
use time;

class Session {
    var sessionId;
    var startTime;
    var lastActivity;
    var isActive;
    
    fun initialize(id) {
        this.sessionId = id;
        this.startTime = time.now();
        this.lastActivity = this.startTime;
        this.isActive = true;
        
        var formattedStart = time.format(this.startTime, "%Y-%m-%d %H:%M:%S");
        say("Session " + id + " started at " + formattedStart);
    }
    
    fun updateActivity() {
        this.lastActivity = time.now();
        var formattedTime = time.format(this.lastActivity, "%H:%M:%S");
        say("Session " + this.sessionId + " activity at " + formattedTime);
    }
    
    fun getDuration() {
        var currentTime = time.now();
        return currentTime - this.startTime;
    }
    
    fun getIdleTime() {
        var currentTime = time.now();
        return currentTime - this.lastActivity;
    }
    
    fun end() {
        var endTime = time.now();
        var totalDuration = endTime - this.startTime;
        var formattedEnd = time.format(endTime, "%Y-%m-%d %H:%M:%S");
        
        this.isActive = false;
        say("Session " + this.sessionId + " ended at " + formattedEnd);
        say("Total duration: " + totalDuration + "ms");
    }
}

var session = Session();
session.initialize("USER123");

time.sleep(1000);
session.updateActivity();

time.sleep(2000);
var duration = session.getDuration();
say("Session has been active for " + duration + "ms");

session.end();
```

### Retry with Exponential Backoff
```neutron
use time;

fun retryWithBackoff(taskName, maxRetries) {
    var attempt = 0;
    var baseDelay = 1000; // Start with 1 second
    
    while (attempt < maxRetries) {
        say("Attempting " + taskName + " (attempt " + (attempt + 1) + ")");
        
        // Simulate task that might fail
        var success = (attempt >= 2); // Succeed on third attempt
        
        if (success) {
            say(taskName + " succeeded!");
            return true;
        }
        
        attempt = attempt + 1;
        
        if (attempt < maxRetries) {
            // Exponential backoff: 1s, 2s, 4s, 8s, etc.
            var delay = baseDelay * (attempt * attempt); // Quadratic for demo
            say("Task failed, retrying in " + delay + "ms");
            time.sleep(delay);
        }
    }
    
    say(taskName + " failed after " + maxRetries + " attempts");
    return false;
}

retryWithBackoff("Connect to Database", 4);
```

## Time Zone Considerations

The time module uses the local system time zone for formatting. Be aware that:

- `time.now()` returns UTC timestamp (milliseconds since Unix epoch)
- `time.format()` uses local time zone for display
- Different systems may show different formatted times for the same timestamp

```neutron
use time;

var utcTimestamp = time.now();
say("UTC timestamp: " + utcTimestamp);

var localTime = time.format(utcTimestamp, "%Y-%m-%d %H:%M:%S %Z");
say("Local time: " + localTime);
```

## Performance Considerations

- `time.now()` is a fast operation (system call)
- `time.format()` involves string processing and may be slower for complex formats
- `time.sleep()` blocks the entire program - use sparingly in performance-critical code
- Consider caching formatted times if displaying the same timestamp multiple times

## Best Practices

1. **Use timestamps for unique IDs**: `time.now()` provides good uniqueness for file names and identifiers
2. **Format consistently**: Use consistent time formats throughout your application
3. **Handle time zones**: Be explicit about whether you're working with local or UTC time
4. **Avoid busy waiting**: Use `time.sleep()` instead of busy loops for delays
5. **Log with timestamps**: Always include timestamps in log messages for debugging

## Compatibility

The time module is available in both interpreter mode and compiled binaries, providing consistent time operations across all execution modes of Neutron programs. Time functions work on all platforms but may have slight variations in precision and format output.
