# Async Module

The `async` module provides basic asynchronous functionality for Neutron, enabling non-blocking operations and concurrent execution patterns.

## Importing the Module

```neutron
use async;
```

## Functions

### `async.run(function)`

Executes a function asynchronously in a background context and returns a task object representing the async operation.

**Parameters:**
- `function` - The function to execute asynchronously

**Returns:**
- An object representing the async task

**Example:**
```neutron
use async;

var task = async.run(fun() {
    // Some time-consuming operation
    return "async result";
});

say("Task started: " + task);
```

### `async.await(task)`

Waits for an async task to complete and returns its result.

**Parameters:**
- `task` - The async task object returned by `async.run()`

**Returns:**
- The result of the completed async operation

**Example:**
```neutron
use async;

var task = async.run(fun() {
    return 42;
});

var result = async.await(task);
say("Async result: " + result);
```

### `async.sleep(milliseconds)`

Pauses execution for the specified number of milliseconds.

>[!NOTE]
> Currently, this function holds the VM lock during sleep to ensure thread safety, which means it will block other Neutron threads from executing bytecode.

**Parameters:**
- `milliseconds` - Number of milliseconds to sleep

**Returns:**
- `true` when sleep completes

**Example:**
```neutron
use async;

say("Before sleep");
async.sleep(1000);  // Sleep for 1 second
say("After sleep");
```

### `async.timer(function, delay_ms)`

Executes a function after a specified delay.

**Parameters:**
- `function` - The function to execute after the delay
- `delay_ms` - Delay in milliseconds before executing the function

**Returns:**
- An object representing the async task (can be awaited)

**Example:**
```neutron
use async;

var task = async.timer(fun() {
    say("Timer executed!");
}, 2000);  // Execute after 2 seconds

say("Timer scheduled");
async.await(task); // Wait for timer to complete
```

## Examples

### Basic Async Execution
```neutron
use async;

var task = async.run(fun() {
    async.sleep(500);
    return "Hello from async!";
});

var result = async.await(task);
say(result);  // Output: "Hello from async!"
```

### Timing Operations
```neutron
use async;

say("Starting...");

// Schedule delayed execution
async.timer(fun() {
    say("Delayed execution completed");
}, 1000);

// Continue with other work
async.sleep(1500);
say("Main execution completed");
```

### Sleep Example
```neutron
use async;

say("Starting long operation...");
async.sleep(2000);  // Simulate 2 second wait
say("Operation completed");
```

## Notes

- The async module uses native OS threads for concurrent execution.
- A Global Interpreter Lock (GIL) ensures thread safety for VM operations.
- `async.run` spawns a new thread that acquires the GIL only when executing Neutron code.
- `async.await` and `async.sleep` release the GIL, allowing other threads to run.
- This model allows for true parallelism for blocking I/O operations or native code that releases the GIL, but Neutron bytecode execution is serialized.
- The implementation provides a foundation for async programming in Neutron, with potential for enhancement in future versions