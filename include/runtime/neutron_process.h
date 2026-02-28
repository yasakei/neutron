/*
 * Neutron Programming Language - Erlang-style Process System
 * Lightweight processes with message passing for concurrent execution
 * Cross-platform support: Windows (native APIs) and POSIX (pthreads)
 */

#ifndef NEUTRON_PROCESS_H
#define NEUTRON_PROCESS_H

#include <vector>
#include <queue>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdint>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    // Undefine Windows macros that conflict with our code
    // NOTE: Do NOT undefine FAR, NEAR, IN, OUT as they are needed by Windows headers
    #undef TRUE
    #undef FALSE
    #undef DELETE
    #undef ERROR
    #undef OPTIONAL
    #undef interface
    #undef small
#else
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/time.h>
#endif

#include "types/value.h"

namespace neutron {

class VM;
class Function;

// Cross-platform threading primitives
#ifdef _WIN32
    typedef CRITICAL_SECTION nt_mutex_t;
    typedef CONDITION_VARIABLE nt_cond_t;
    typedef HANDLE nt_thread_t;
    typedef volatile LONG nt_atomic_bool_t;
    typedef volatile LONG64 nt_atomic_uint64_t;
#else
    typedef pthread_mutex_t nt_mutex_t;
    typedef pthread_cond_t nt_cond_t;
    typedef pthread_t nt_thread_t;
    typedef volatile int nt_atomic_bool_t;
    typedef volatile uint64_t nt_atomic_uint64_t;
#endif

// Cross-platform mutex operations
void nt_mutex_init(nt_mutex_t* mtx);
void nt_mutex_destroy(nt_mutex_t* mtx);
void nt_mutex_lock(nt_mutex_t* mtx);
void nt_mutex_unlock(nt_mutex_t* mtx);

// Cross-platform condition variable operations
void nt_cond_init(nt_cond_t* cond);
void nt_cond_destroy(nt_cond_t* cond);
void nt_cond_wait(nt_cond_t* cond, nt_mutex_t* mtx);
bool nt_cond_timedwait(nt_cond_t* cond, nt_mutex_t* mtx, int64_t timeout_ms);
void nt_cond_signal(nt_cond_t* cond);
void nt_cond_broadcast(nt_cond_t* cond);

// Cross-platform atomic operations
bool nt_atomic_load_bool(nt_atomic_bool_t* val);
void nt_atomic_store_bool(nt_atomic_bool_t* val, bool newval);
bool nt_atomic_exchange_bool(nt_atomic_bool_t* val, bool newval);
uint64_t nt_atomic_load_uint64(nt_atomic_uint64_t* val);
uint64_t nt_atomic_fetch_add_uint64(nt_atomic_uint64_t* val, uint64_t add);

// Cross-platform thread operations
typedef void* (*nt_thread_func_t)(void* arg);
bool nt_thread_create(nt_thread_t* thread, nt_thread_func_t func, void* arg);
void nt_thread_join(nt_thread_t thread);
void nt_thread_detach(nt_thread_t thread);
unsigned int nt_hardware_concurrency();
void nt_sleep_ms(int64_t ms);

// Process ID type
using PID = uint64_t;

// Process states
enum class ProcessState {
    READY,
    RUNNING,
    WAITING,
    FINISHED,
    DEAD
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
    void notifyMailbox();
    
    // Process execution state
    Function* function;
    std::vector<Value> arguments;
    Value result;
    std::string errorMessage;
    
    // Execution quota
    int64_t reductionsLeft;
    static constexpr int64_t DEFAULT_REDUCTIONS = 2000;
    
private:
    PID pid;
    ProcessState state;
    
    // Message mailbox
    std::queue<Message> mailbox;
    mutable nt_mutex_t mailboxMutex;
    nt_cond_t mailboxCV;
};

// Process Scheduler
class ProcessScheduler {
public:
    ProcessScheduler();
    ~ProcessScheduler();
    
    void start(size_t numWorkers = 0);
    void stop();
    
    PID spawn(Function* func, std::vector<Value> args);
    void kill(PID pid);
    bool isAlive(PID pid);
    
    bool send(PID to, PID from, Value message);
    bool receive(PID pid, Message& msg, int64_t timeoutMs = -1);
    
    Process* getProcess(PID pid);
    size_t processCount() const;
    
    static PID currentPID();
    static void setCurrentPID(PID pid);
    
    static ProcessScheduler& instance();
    
    struct Stats {
        nt_atomic_uint64_t processesSpawned;
        nt_atomic_uint64_t messagesDelivered;
        nt_atomic_uint64_t contextSwitches;
    };
    Stats stats;
    
private:
    std::unordered_map<PID, std::unique_ptr<Process>> processes;
    mutable nt_mutex_t processesMutex;
    
    std::queue<PID> readyQueue;
    nt_mutex_t readyMutex;
    nt_cond_t readyCV;
    
    std::vector<nt_thread_t> workers;
    nt_atomic_bool_t running;
    
    nt_atomic_uint64_t nextPID;
    
    static void* workerLoopStatic(void* arg);
    void workerLoop(size_t workerId);
    void schedule(PID pid);
    void executeProcess(Process* proc, VM& vm);
};

// Process-aware VM extensions
class ProcessVM {
public:
    static Value spawn(VM& vm, std::vector<Value> args);
    static Value send(VM& vm, std::vector<Value> args);
    static Value receive(VM& vm, std::vector<Value> args);
    static Value self(VM& vm, std::vector<Value> args);
    static Value isAlive(VM& vm, std::vector<Value> args);
    static Value kill(VM& vm, std::vector<Value> args);
    static Value processCount(VM& vm, std::vector<Value> args);
    static Value sleep(VM& vm, std::vector<Value> args);
    static void registerFunctions(VM& vm, std::shared_ptr<class Environment> env);
};

} // namespace neutron

#endif // NEUTRON_PROCESS_H
