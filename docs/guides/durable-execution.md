# Durable Execution (Checkpoint & Resume)

Neutron includes a powerful **Durable Execution** feature that allows you to save the entire state of a running program to a file and resume it later. This is similar to "saving a game" but for your scripts.

This feature is built directly into the language runtime, making it perfect for:
- Long-running automation tasks (that might take days).
- Fragile data pipelines (that might crash halfway through).
- "Serverless" style workflows where processes hibernate to save resources.

## How It Works

When you create a checkpoint, Neutron saves:
1.  **Call Stack**: Where the program is currently executing.
2.  **Variables**: All local and global variables.
3.  **Heap Memory**: All objects, arrays, strings, and data structures.

When you resume, the program continues **exactly** from the line after the checkpoint, as if it never stopped.

## Basic Usage

### 1. Saving State (`sys.checkpoint`)

Use the `sys.checkpoint(path)` function to save the state.

```neutron
use sys;

var progress = 0;

// Do some work
progress = 50;
say("Halfway done...");

// Save the state to 'backup.snap'
// The program continues running after this line.
sys.checkpoint("backup.snap"); 

// Do more work
progress = 100;
say("Finished!");
```

### 2. Resuming State (`--resume`)

To resume a saved checkpoint, run the `neutron` executable with the `--resume` flag:

```bash
./neutron --resume backup.snap
```

The output will be:
```text
Finished!
```
(Note: "Halfway done..." is not printed because execution resumes *after* that point).

## Real-World Examples

### Example 1: Fragile Data Pipeline

If you are downloading a huge dataset, you don't want to restart from zero if the script crashes during processing.

```neutron
use sys;

fun main() {
    // Step 1: Download (Expensive)
    var data = download_huge_file(); 
    say("Download complete.");
    
    // Checkpoint! 
    // If the script crashes after this, we skip the download next time.
    sys.checkpoint("after_download.snap");

    // Step 2: Process (Risk of crash)
    var results = process_data(data);
    
    // Step 3: Upload
    upload_results(results);
}
```

### Example 2: Long-Running Monitor

A script that runs forever but only needs to wake up once a day. Instead of keeping the process in RAM, you can "hibernate" it.

```neutron
use sys;
use time;

var days_running = 0;

while (true) {
    perform_daily_check();
    days_running = days_running + 1;
    
    say("Day " + days_running + " complete. Hibernating...");
    
    // Save state
    sys.checkpoint("monitor.snap");
    
    // Exit the process to free resources
    sys.exit(0);
    
    // ... (The next day, a cron job runs 'neutron --resume monitor.snap') ...
    
    // Resume here!
    say("Waking up for next day!");
}
```

## Limitations & Best Practices

1.  **External Resources**: Checkpoints save *memory*, not external connections.
    *   **Files**: Open file handles are closed when the process exits. You must re-open files after resuming.
    *   **Network**: Socket connections are lost. You must reconnect to databases or APIs.
    
    ```neutron
    var db = connect_db();
    sys.checkpoint("save.snap");
    
    // BAD: db connection is dead here if resumed!
    // db.query(...) -> Error
    
    // GOOD: Reconnect
    db = connect_db();
    ```

2.  **Security**: Snapshot files (`.snap`) contain a full dump of the program's memory. This includes any passwords, API keys, or sensitive data that was in variables. **Protect your snapshot files.**

3.  **Code Changes**: Resuming a snapshot using a *modified* version of the script is generally safe (Neutron saves the bytecode inside the snapshot), but logic changes in functions that haven't run yet will not be reflected if the old bytecode is used. It is best to resume with the same version of the application.
