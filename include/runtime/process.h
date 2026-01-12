/*
 * Neutron Programming Language - Erlang-style Process System
 * Lightweight processes with message passing for concurrent execution
 */

#ifndef NEUTRON_PROCESS_H
#define NEUTRON_PROCESS_H

#include <vector>
#include <queue>
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

#include "types/value.h"

namespace neutron {

class VM;
class Function;

// Process ID type
using PID = uint64_t;

// Process states
enum class ProcessState {
    READY,      // Ready to run
    RUNNING,    // Currently executing
    WAITING,    // Waiting for message
    FINISHED,   // Completed execution
    DEAD        // Terminated with error
};

// Message for inter-process communication
struct Message {
    PID sender;
    Value data;
    uint64_t timestamp;
    
    Message(PID s, Value d) : sender(s), data(d), timestamp(0) {}
};

// A lightweight process (like Erlang process)
class Process {
public:
    Process(PID pid, Function* func, std::vector<Value> args);
    ~Process();
    
    PID getPid() const { return pid; }
    ProcessState getState() const { return state; }
    void setState(ProcessState s) { state = s; }
    
    // Message queue operations
    void sendMessage(const Message& msg);
    bool hasMessages() const;
    Message receiveMessage();
    bool tryReceiveMessage(Message& msg, int64_t timeoutMs = -1);
    void notifyMailbox() { mailboxCV.notify_all(); }  // Wake up waiting receivers
    
    // Process execution state
    Function* function;
    std::vector<Value> arguments;
    Value result;
    std::string errorMessage;
    
    // Execution quota (for fair scheduling)
    int64_t reductionsLeft;
    static constexpr int64_t DEFAULT_REDUCTIONS = 2000;
    
private:
    PID pid;
    ProcessState state;
    
    // Message mailbox
    std::queue<Message> mailbox;
    mutable std::mutex mailboxMutex;
    std::condition_variable mailboxCV;
};

// Process Scheduler - manages multiple processes across threads
class ProcessScheduler {
public:
    ProcessScheduler();
    ~ProcessScheduler();
    
    // Initialize with number of worker threads (0 = auto-detect)
    void start(size_t numWorkers = 0);
    void stop();
    
    // Process management
    PID spawn(Function* func, std::vector<Value> args);
    void kill(PID pid);
    bool isAlive(PID pid);
    
    // Message passing
    bool send(PID to, PID from, Value message);
    bool receive(PID pid, Message& msg, int64_t timeoutMs = -1);
    
    // Get process info
    Process* getProcess(PID pid);
    size_t processCount() const;
    
    // Current process context (thread-local)
    static PID currentPID();
    static void setCurrentPID(PID pid);
    
    // Singleton access
    static ProcessScheduler& instance();
    
    // Statistics
    struct Stats {
        std::atomic<uint64_t> processesSpawned{0};
        std::atomic<uint64_t> messagesDelivered{0};
        std::atomic<uint64_t> contextSwitches{0};
    };
    Stats stats;
    
private:
    // Process table
    std::unordered_map<PID, std::unique_ptr<Process>> processes;
    mutable std::mutex processesMutex;
    
    // Ready queue for scheduling
    std::queue<PID> readyQueue;
    std::mutex readyMutex;
    std::condition_variable readyCV;
    
    // Worker threads
    std::vector<std::thread> workers;
    std::atomic<bool> running{false};
    
    // PID counter
    std::atomic<PID> nextPID{1};
    
    // Worker thread function
    void workerLoop(size_t workerId);
    
    // Schedule a process
    void schedule(PID pid);
    
    // Execute a process for one time slice
    void executeProcess(Process* proc, VM& vm);
};

// Process-aware VM extensions
class ProcessVM {
public:
    // Spawn a new process running a function
    static Value spawn(VM& vm, std::vector<Value> args);
    
    // Send a message to a process
    static Value send(VM& vm, std::vector<Value> args);
    
    // Receive a message (blocking or with timeout)
    static Value receive(VM& vm, std::vector<Value> args);
    
    // Get current process ID
    static Value self(VM& vm, std::vector<Value> args);
    
    // Check if process is alive
    static Value isAlive(VM& vm, std::vector<Value> args);
    
    // Kill a process
    static Value kill(VM& vm, std::vector<Value> args);
    
    // Get number of active processes
    static Value processCount(VM& vm, std::vector<Value> args);
    
    // Sleep current process
    static Value sleep(VM& vm, std::vector<Value> args);
    
    // Register process functions with VM
    static void registerFunctions(VM& vm, std::shared_ptr<class Environment> env);
};

} // namespace neutron

#endif // NEUTRON_PROCESS_H
