/*
 * Neutron Programming Language - Process System Implementation
 * Erlang-style lightweight processes with message passing
 */

#include "runtime/process.h"
#include "core/vm.h"
#include "types/function.h"
#include "types/array.h"
#include "runtime/environment.h"
#include <chrono>
#include <algorithm>

namespace neutron {

// Thread-local current process ID
thread_local PID tls_currentPID = 0;

// ============================================================================
// Process Implementation
// ============================================================================

Process::Process(PID pid, Function* func, std::vector<Value> args)
    : function(func)
    , arguments(std::move(args))
    , result(Value())
    , reductionsLeft(DEFAULT_REDUCTIONS)
    , pid(pid)
    , state(ProcessState::READY)
{
}

Process::~Process() = default;

void Process::sendMessage(const Message& msg) {
    std::lock_guard<std::mutex> lock(mailboxMutex);
    mailbox.push(msg);
    mailboxCV.notify_one();
}

bool Process::hasMessages() const {
    std::lock_guard<std::mutex> lock(mailboxMutex);
    return !mailbox.empty();
}

Message Process::receiveMessage() {
    std::unique_lock<std::mutex> lock(mailboxMutex);
    mailboxCV.wait(lock, [this]{ return !mailbox.empty() || state == ProcessState::DEAD; });
    
    if (mailbox.empty()) {
        return Message(0, Value());  // Process was killed
    }
    
    Message msg = mailbox.front();
    mailbox.pop();
    return msg;
}

bool Process::tryReceiveMessage(Message& msg, int64_t timeoutMs) {
    std::unique_lock<std::mutex> lock(mailboxMutex);
    
    if (timeoutMs < 0) {
        // Blocking wait
        mailboxCV.wait(lock, [this]{ return !mailbox.empty() || state == ProcessState::DEAD; });
    } else if (timeoutMs > 0) {
        // Timed wait
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        if (!mailboxCV.wait_until(lock, deadline, [this]{ return !mailbox.empty(); })) {
            return false;  // Timeout
        }
    } else {
        // Non-blocking
        if (mailbox.empty()) {
            return false;
        }
    }
    
    if (mailbox.empty()) {
        return false;
    }
    
    msg = mailbox.front();
    mailbox.pop();
    return true;
}

// ============================================================================
// ProcessScheduler Implementation
// ============================================================================

ProcessScheduler::ProcessScheduler() = default;

ProcessScheduler::~ProcessScheduler() {
    stop();
}

ProcessScheduler& ProcessScheduler::instance() {
    static ProcessScheduler scheduler;
    return scheduler;
}

void ProcessScheduler::start(size_t numWorkers) {
    if (running.exchange(true)) {
        return;  // Already running
    }
    
    if (numWorkers == 0) {
        numWorkers = std::max(1u, std::thread::hardware_concurrency());
    }
    
    workers.reserve(numWorkers);
    for (size_t i = 0; i < numWorkers; ++i) {
        workers.emplace_back(&ProcessScheduler::workerLoop, this, i);
    }
}

void ProcessScheduler::stop() {
    if (!running.exchange(false)) {
        return;  // Already stopped
    }
    
    // Wake up all workers
    readyCV.notify_all();
    
    // Wake up all waiting processes and their mailboxes
    {
        std::lock_guard<std::mutex> lock(processesMutex);
        for (auto& pair : processes) {
            pair.second->setState(ProcessState::DEAD);
            pair.second->notifyMailbox();  // Wake up any process waiting on receive
        }
    }
    
    // Join all workers with timeout protection
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.detach();  // Detach instead of join to avoid deadlock on exit
        }
    }
    workers.clear();
}

PID ProcessScheduler::spawn(Function* func, std::vector<Value> args) {
    // Auto-start scheduler on first spawn
    if (!running) {
        start();
    }
    
    PID pid = nextPID.fetch_add(1);
    
    auto process = std::make_unique<Process>(pid, func, std::move(args));
    
    {
        std::lock_guard<std::mutex> lock(processesMutex);
        processes[pid] = std::move(process);
    }
    
    stats.processesSpawned++;
    schedule(pid);
    
    return pid;
}

void ProcessScheduler::kill(PID pid) {
    std::lock_guard<std::mutex> lock(processesMutex);
    auto it = processes.find(pid);
    if (it != processes.end()) {
        it->second->setState(ProcessState::DEAD);
    }
}

bool ProcessScheduler::isAlive(PID pid) {
    std::lock_guard<std::mutex> lock(processesMutex);
    auto it = processes.find(pid);
    if (it == processes.end()) {
        return false;
    }
    auto state = it->second->getState();
    return state != ProcessState::DEAD && state != ProcessState::FINISHED;
}

bool ProcessScheduler::send(PID to, PID from, Value message) {
    std::lock_guard<std::mutex> lock(processesMutex);
    auto it = processes.find(to);
    if (it == processes.end()) {
        return false;
    }
    
    Process* proc = it->second.get();
    if (proc->getState() == ProcessState::DEAD || proc->getState() == ProcessState::FINISHED) {
        return false;
    }
    
    proc->sendMessage(Message(from, message));
    stats.messagesDelivered++;
    
    // If process was waiting, make it ready
    if (proc->getState() == ProcessState::WAITING) {
        proc->setState(ProcessState::READY);
        schedule(to);
    }
    
    return true;
}

bool ProcessScheduler::receive(PID pid, Message& msg, int64_t timeoutMs) {
    Process* proc = getProcess(pid);
    if (!proc) {
        return false;
    }
    
    return proc->tryReceiveMessage(msg, timeoutMs);
}

Process* ProcessScheduler::getProcess(PID pid) {
    std::lock_guard<std::mutex> lock(processesMutex);
    auto it = processes.find(pid);
    return (it != processes.end()) ? it->second.get() : nullptr;
}

size_t ProcessScheduler::processCount() const {
    std::lock_guard<std::mutex> lock(processesMutex);
    size_t count = 0;
    for (const auto& pair : processes) {
        auto state = pair.second->getState();
        if (state != ProcessState::DEAD && state != ProcessState::FINISHED) {
            count++;
        }
    }
    return count;
}

PID ProcessScheduler::currentPID() {
    return tls_currentPID;
}

void ProcessScheduler::setCurrentPID(PID pid) {
    tls_currentPID = pid;
}

void ProcessScheduler::schedule(PID pid) {
    {
        std::lock_guard<std::mutex> lock(readyMutex);
        readyQueue.push(pid);
    }
    readyCV.notify_one();
}

void ProcessScheduler::workerLoop(size_t workerId) {
    (void)workerId;  // May be used for debugging
    
    // Each worker has its own VM instance for executing processes
    VM workerVM;
    
    while (running) {
        PID pid = 0;
        
        // Get a process from the ready queue
        {
            std::unique_lock<std::mutex> lock(readyMutex);
            readyCV.wait(lock, [this]{ return !readyQueue.empty() || !running; });
            
            if (!running) {
                break;
            }
            
            if (readyQueue.empty()) {
                continue;
            }
            
            pid = readyQueue.front();
            readyQueue.pop();
        }
        
        // Get the process
        Process* proc = getProcess(pid);
        if (!proc || proc->getState() == ProcessState::DEAD || proc->getState() == ProcessState::FINISHED) {
            continue;
        }
        
        // Execute the process
        executeProcess(proc, workerVM);
        stats.contextSwitches++;
    }
}

void ProcessScheduler::executeProcess(Process* proc, VM& vm) {
    setCurrentPID(proc->getPid());
    proc->setState(ProcessState::RUNNING);
    
    try {
        // Ensure process module is loaded in worker VM
        vm.load_module("process");
        
        // Push the function and arguments onto the stack
        vm.stack.clear();
        vm.frames.clear();
        
        // Push function
        vm.push(Value(static_cast<Callable*>(proc->function)));
        
        // Push arguments
        for (const auto& arg : proc->arguments) {
            vm.push(arg);
        }
        
        // Call the function
        if (vm.callValuePublic(Value(static_cast<Callable*>(proc->function)), 
                               static_cast<int>(proc->arguments.size()))) {
            vm.runPublic();
        }
        
        // Get result from stack
        if (!vm.stack.empty()) {
            proc->result = vm.stack.back();
        }
        
        proc->setState(ProcessState::FINISHED);
        
    } catch (const std::exception& e) {
        proc->errorMessage = e.what();
        proc->setState(ProcessState::DEAD);
    }
    
    setCurrentPID(0);
}

// ============================================================================
// ProcessVM - Native functions for process management
// ============================================================================

Value ProcessVM::spawn(VM& vm, std::vector<Value> args) {
    (void)vm; // Mark unused
    if (args.empty()) {
        throw std::runtime_error("spawn() requires a function argument");
    }
    
    if (args[0].type != ValueType::CALLABLE) {
        throw std::runtime_error("First argument to spawn() must be a function");
    }
    
    Function* func = dynamic_cast<Function*>(args[0].as.callable);
    if (!func) {
        throw std::runtime_error("spawn() requires a user-defined function");
    }
    
    // Remaining args are passed to the function
    std::vector<Value> funcArgs(args.begin() + 1, args.end());
    
    // Ensure scheduler is running
    auto& scheduler = ProcessScheduler::instance();
    static std::once_flag startFlag;
    std::call_once(startFlag, [&scheduler]{ scheduler.start(); });
    
    PID pid = scheduler.spawn(func, funcArgs);
    
    return Value(static_cast<double>(pid));
}

Value ProcessVM::send(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() < 2) {
        throw std::runtime_error("send() requires 2 arguments: (pid, message)");
    }
    
    if (args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("First argument to send() must be a process ID");
    }
    
    PID to = static_cast<PID>(args[0].as.number);
    PID from = ProcessScheduler::currentPID();
    
    bool success = ProcessScheduler::instance().send(to, from, args[1]);
    
    return Value(success);
}

Value ProcessVM::receive(VM& vm, std::vector<Value> args) {
    (void)vm;
    int64_t timeout = -1;  // Blocking by default
    
    if (!args.empty() && args[0].type == ValueType::NUMBER) {
        timeout = static_cast<int64_t>(args[0].as.number);
    }
    
    PID pid = ProcessScheduler::currentPID();
    if (pid == 0) {
        // Main thread - create a temporary process for receiving
        throw std::runtime_error("receive() can only be called from a spawned process");
    }
    
    Message msg(0, Value());
    if (ProcessScheduler::instance().receive(pid, msg, timeout)) {
        // Return just the message data for simplicity
        return msg.data;
    }
    
    return Value();  // Timeout or no message
}

Value ProcessVM::self(VM& vm, std::vector<Value> args) {
    (void)vm;
    (void)args;
    return Value(static_cast<double>(ProcessScheduler::currentPID()));
}

Value ProcessVM::isAlive(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.empty() || args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("is_alive() requires a process ID");
    }
    
    PID pid = static_cast<PID>(args[0].as.number);
    return Value(ProcessScheduler::instance().isAlive(pid));
}

Value ProcessVM::kill(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.empty() || args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("kill() requires a process ID");
    }
    
    PID pid = static_cast<PID>(args[0].as.number);
    ProcessScheduler::instance().kill(pid);
    return Value();
}

Value ProcessVM::processCount(VM& vm, std::vector<Value> args) {
    (void)vm;
    (void)args;
    return Value(static_cast<double>(ProcessScheduler::instance().processCount()));
}

Value ProcessVM::sleep(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.empty() || args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("sleep() requires a duration in milliseconds");
    }
    
    int64_t ms = static_cast<int64_t>(args[0].as.number);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return Value();
}

void ProcessVM::registerFunctions(VM& vm, std::shared_ptr<Environment> env) {
    // Register process functions (all need VM access)
    env->define("spawn", Value(vm.allocate<NativeFn>(spawn, -1, true)));
    env->define("send", Value(vm.allocate<NativeFn>(send, 2, true)));
    env->define("receive", Value(vm.allocate<NativeFn>(receive, -1, true)));
    env->define("self", Value(vm.allocate<NativeFn>(self, 0, true)));
    env->define("is_alive", Value(vm.allocate<NativeFn>(isAlive, 1, true)));
    env->define("kill", Value(vm.allocate<NativeFn>(kill, 1, true)));
    env->define("process_count", Value(vm.allocate<NativeFn>(processCount, 0, true)));
    env->define("process_sleep", Value(vm.allocate<NativeFn>(sleep, 1, true)));
}

} // namespace neutron
