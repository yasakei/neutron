/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, for both open source and commercial purposes.
 * 
 * Conditions:
 * 
 * 1. The above copyright notice and this permission notice shall be included
 *    in all copies or substantial portions of the Software.
 * 
 * 2. Attribution is appreciated but NOT required.
 *    Suggested (optional) credit:
 *    "Built using Neutron Programming Language (c) yasakei"
 * 
 * 3. The name "Neutron" and the name of the copyright holder may not be used
 *    to endorse or promote products derived from this Software without prior
 *    written permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Neutron Programming Language - Process System Implementation
 * Cross-platform: Windows (native APIs) and POSIX (pthreads)
 */

// Include neutron_process.h first to get Windows headers with proper undefs
#include "runtime/neutron_process.h"

// Now safe to include other headers
#include "core/vm.h"
#include "types/function.h"
#include "types/array.h"
#include "runtime/environment.h"

namespace neutron {

// ============================================================================
// Cross-platform Threading Primitives Implementation
// ============================================================================

#ifdef _WIN32

void nt_mutex_init(nt_mutex_t* mtx) {
    InitializeCriticalSection(mtx);
}

void nt_mutex_destroy(nt_mutex_t* mtx) {
    DeleteCriticalSection(mtx);
}

void nt_mutex_lock(nt_mutex_t* mtx) {
    EnterCriticalSection(mtx);
}

void nt_mutex_unlock(nt_mutex_t* mtx) {
    LeaveCriticalSection(mtx);
}

void nt_cond_init(nt_cond_t* cond) {
    InitializeConditionVariable(cond);
}

void nt_cond_destroy(nt_cond_t* cond) {
    (void)cond; // Windows condition variables don't need destruction
}

void nt_cond_wait(nt_cond_t* cond, nt_mutex_t* mtx) {
    SleepConditionVariableCS(cond, mtx, INFINITE);
}

bool nt_cond_timedwait(nt_cond_t* cond, nt_mutex_t* mtx, int64_t timeout_ms) {
    return SleepConditionVariableCS(cond, mtx, (DWORD)timeout_ms) != 0;
}

void nt_cond_signal(nt_cond_t* cond) {
    WakeConditionVariable(cond);
}

void nt_cond_broadcast(nt_cond_t* cond) {
    WakeAllConditionVariable(cond);
}

bool nt_atomic_load_bool(nt_atomic_bool_t* val) {
    return InterlockedCompareExchange(val, 0, 0) != 0;
}

void nt_atomic_store_bool(nt_atomic_bool_t* val, bool newval) {
    InterlockedExchange(val, newval ? 1 : 0);
}

bool nt_atomic_exchange_bool(nt_atomic_bool_t* val, bool newval) {
    return InterlockedExchange(val, newval ? 1 : 0) != 0;
}

uint64_t nt_atomic_load_uint64(nt_atomic_uint64_t* val) {
    return InterlockedCompareExchange64(val, 0, 0);
}

uint64_t nt_atomic_fetch_add_uint64(nt_atomic_uint64_t* val, uint64_t add) {
    return InterlockedExchangeAdd64(val, add);
}

static DWORD WINAPI win_thread_wrapper(LPVOID arg);

struct WinThreadData {
    nt_thread_func_t func;
    void* arg;
};

static DWORD WINAPI win_thread_wrapper(LPVOID arg) {
    WinThreadData* data = (WinThreadData*)arg;
    nt_thread_func_t func = data->func;
    void* funcArg = data->arg;
    delete data;
    func(funcArg);
    return 0;
}

bool nt_thread_create(nt_thread_t* thread, nt_thread_func_t func, void* arg) {
    WinThreadData* data = new WinThreadData{func, arg};
    *thread = CreateThread(NULL, 0, win_thread_wrapper, data, 0, NULL);
    if (*thread == NULL) {
        delete data;
        return false;
    }
    return true;
}

void nt_thread_join(nt_thread_t thread) {
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
}

void nt_thread_detach(nt_thread_t thread) {
    CloseHandle(thread);
}

unsigned int nt_hardware_concurrency() {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
}

void nt_sleep_ms(int64_t ms) {
    Sleep((DWORD)ms);
}

#else // POSIX

void nt_mutex_init(nt_mutex_t* mtx) {
    pthread_mutex_init(mtx, NULL);
}

void nt_mutex_destroy(nt_mutex_t* mtx) {
    pthread_mutex_destroy(mtx);
}

void nt_mutex_lock(nt_mutex_t* mtx) {
    pthread_mutex_lock(mtx);
}

void nt_mutex_unlock(nt_mutex_t* mtx) {
    pthread_mutex_unlock(mtx);
}

void nt_cond_init(nt_cond_t* cond) {
    pthread_cond_init(cond, NULL);
}

void nt_cond_destroy(nt_cond_t* cond) {
    pthread_cond_destroy(cond);
}

void nt_cond_wait(nt_cond_t* cond, nt_mutex_t* mtx) {
    pthread_cond_wait(cond, mtx);
}

bool nt_cond_timedwait(nt_cond_t* cond, nt_mutex_t* mtx, int64_t timeout_ms) {
    struct timespec ts;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec + timeout_ms / 1000;
    ts.tv_nsec = tv.tv_usec * 1000 + (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    return pthread_cond_timedwait(cond, mtx, &ts) == 0;
}

void nt_cond_signal(nt_cond_t* cond) {
    pthread_cond_signal(cond);
}

void nt_cond_broadcast(nt_cond_t* cond) {
    pthread_cond_broadcast(cond);
}

bool nt_atomic_load_bool(nt_atomic_bool_t* val) {
    return __sync_fetch_and_add(val, 0) != 0;
}

void nt_atomic_store_bool(nt_atomic_bool_t* val, bool newval) {
    __sync_lock_test_and_set(val, newval ? 1 : 0);
}

bool nt_atomic_exchange_bool(nt_atomic_bool_t* val, bool newval) {
    return __sync_lock_test_and_set(val, newval ? 1 : 0) != 0;
}

uint64_t nt_atomic_load_uint64(nt_atomic_uint64_t* val) {
    return __sync_fetch_and_add(val, 0);
}

uint64_t nt_atomic_fetch_add_uint64(nt_atomic_uint64_t* val, uint64_t add) {
    return __sync_fetch_and_add(val, add);
}

bool nt_thread_create(nt_thread_t* thread, nt_thread_func_t func, void* arg) {
    return pthread_create(thread, NULL, func, arg) == 0;
}

void nt_thread_join(nt_thread_t thread) {
    pthread_join(thread, NULL);
}

void nt_thread_detach(nt_thread_t thread) {
    pthread_detach(thread);
}

unsigned int nt_hardware_concurrency() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

void nt_sleep_ms(int64_t ms) {
    usleep(ms * 1000);
}

#endif

// ============================================================================
// Thread-local current process ID
// ============================================================================

#ifdef _WIN32
static __declspec(thread) PID tls_currentPID = 0;
#else
static __thread PID tls_currentPID = 0;
#endif

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
    nt_mutex_init(&mailboxMutex);
    nt_cond_init(&mailboxCV);
}

Process::~Process() {
    nt_mutex_destroy(&mailboxMutex);
    nt_cond_destroy(&mailboxCV);
}

void Process::sendMessage(const Message& msg) {
    nt_mutex_lock(&mailboxMutex);
    mailbox.push(msg);
    nt_cond_signal(&mailboxCV);
    nt_mutex_unlock(&mailboxMutex);
}

bool Process::hasMessages() const {
    nt_mutex_lock(&mailboxMutex);
    bool hasMsg = !mailbox.empty();
    nt_mutex_unlock(&mailboxMutex);
    return hasMsg;
}

Message Process::receiveMessage() {
    nt_mutex_lock(&mailboxMutex);
    while (mailbox.empty() && state != ProcessState::DEAD) {
        nt_cond_wait(&mailboxCV, &mailboxMutex);
    }
    
    if (mailbox.empty()) {
        nt_mutex_unlock(&mailboxMutex);
        return Message(0, Value());
    }
    
    Message msg = mailbox.front();
    mailbox.pop();
    nt_mutex_unlock(&mailboxMutex);
    return msg;
}

bool Process::tryReceiveMessage(Message& msg, int64_t timeoutMs) {
    nt_mutex_lock(&mailboxMutex);
    
    if (timeoutMs < 0) {
        while (mailbox.empty() && state != ProcessState::DEAD) {
            nt_cond_wait(&mailboxCV, &mailboxMutex);
        }
    } else if (timeoutMs > 0) {
        if (mailbox.empty()) {
            if (!nt_cond_timedwait(&mailboxCV, &mailboxMutex, timeoutMs)) {
                nt_mutex_unlock(&mailboxMutex);
                return false;
            }
        }
    }
    
    if (mailbox.empty()) {
        nt_mutex_unlock(&mailboxMutex);
        return false;
    }
    
    msg = mailbox.front();
    mailbox.pop();
    nt_mutex_unlock(&mailboxMutex);
    return true;
}

void Process::notifyMailbox() {
    nt_cond_broadcast(&mailboxCV);
}

// ============================================================================
// ProcessScheduler Implementation
// ============================================================================

ProcessScheduler::ProcessScheduler() {
    nt_mutex_init(&processesMutex);
    nt_mutex_init(&readyMutex);
    nt_cond_init(&readyCV);
    nt_atomic_store_bool(&running, false);
    stats.processesSpawned = 0;
    stats.messagesDelivered = 0;
    stats.contextSwitches = 0;
    nextPID = 1;
}

ProcessScheduler::~ProcessScheduler() {
    stop();
    nt_mutex_destroy(&processesMutex);
    nt_mutex_destroy(&readyMutex);
    nt_cond_destroy(&readyCV);
}

ProcessScheduler& ProcessScheduler::instance() {
    static ProcessScheduler scheduler;
    return scheduler;
}

struct WorkerArg {
    ProcessScheduler* scheduler;
    size_t workerId;
};

void* ProcessScheduler::workerLoopStatic(void* arg) {
    WorkerArg* wa = (WorkerArg*)arg;
    wa->scheduler->workerLoop(wa->workerId);
    delete wa;
    return NULL;
}

void ProcessScheduler::start(size_t numWorkers) {
    if (nt_atomic_exchange_bool(&running, true)) {
        return;
    }
    
    if (numWorkers == 0) {
        numWorkers = nt_hardware_concurrency();
        if (numWorkers == 0) numWorkers = 1;
    }
    
    workers.resize(numWorkers);
    for (size_t i = 0; i < numWorkers; ++i) {
        WorkerArg* arg = new WorkerArg{this, i};
        nt_thread_create(&workers[i], workerLoopStatic, arg);
    }
}

void ProcessScheduler::stop() {
    if (!nt_atomic_exchange_bool(&running, false)) {
        return;
    }
    
    nt_cond_broadcast(&readyCV);
    
    nt_mutex_lock(&processesMutex);
    for (auto& pair : processes) {
        pair.second->setState(ProcessState::DEAD);
        pair.second->notifyMailbox();
    }
    nt_mutex_unlock(&processesMutex);
    
    for (auto& worker : workers) {
        nt_thread_detach(worker);
    }
    workers.clear();
}

PID ProcessScheduler::spawn(Function* func, std::vector<Value> args) {
    if (!nt_atomic_load_bool(&running)) {
        start();
    }
    
    PID pid = nt_atomic_fetch_add_uint64(&nextPID, 1);
    
    auto process = std::make_unique<Process>(pid, func, std::move(args));
    
    nt_mutex_lock(&processesMutex);
    processes[pid] = std::move(process);
    nt_mutex_unlock(&processesMutex);
    
    nt_atomic_fetch_add_uint64(&stats.processesSpawned, 1);
    schedule(pid);
    
    return pid;
}

void ProcessScheduler::kill(PID pid) {
    nt_mutex_lock(&processesMutex);
    auto it = processes.find(pid);
    if (it != processes.end()) {
        it->second->setState(ProcessState::DEAD);
    }
    nt_mutex_unlock(&processesMutex);
}

bool ProcessScheduler::isAlive(PID pid) {
    nt_mutex_lock(&processesMutex);
    auto it = processes.find(pid);
    if (it == processes.end()) {
        nt_mutex_unlock(&processesMutex);
        return false;
    }
    auto state = it->second->getState();
    nt_mutex_unlock(&processesMutex);
    return state != ProcessState::DEAD && state != ProcessState::FINISHED;
}

bool ProcessScheduler::send(PID to, PID from, Value message) {
    nt_mutex_lock(&processesMutex);
    auto it = processes.find(to);
    if (it == processes.end()) {
        nt_mutex_unlock(&processesMutex);
        return false;
    }
    
    Process* proc = it->second.get();
    if (proc->getState() == ProcessState::DEAD || proc->getState() == ProcessState::FINISHED) {
        nt_mutex_unlock(&processesMutex);
        return false;
    }
    
    proc->sendMessage(Message(from, message));
    nt_atomic_fetch_add_uint64(&stats.messagesDelivered, 1);
    
    if (proc->getState() == ProcessState::WAITING) {
        proc->setState(ProcessState::READY);
        nt_mutex_unlock(&processesMutex);
        schedule(to);
    } else {
        nt_mutex_unlock(&processesMutex);
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
    nt_mutex_lock(&processesMutex);
    auto it = processes.find(pid);
    Process* proc = (it != processes.end()) ? it->second.get() : nullptr;
    nt_mutex_unlock(&processesMutex);
    return proc;
}

size_t ProcessScheduler::processCount() const {
    nt_mutex_lock(&processesMutex);
    size_t count = 0;
    for (const auto& pair : processes) {
        auto state = pair.second->getState();
        if (state != ProcessState::DEAD && state != ProcessState::FINISHED) {
            count++;
        }
    }
    nt_mutex_unlock(&processesMutex);
    return count;
}

PID ProcessScheduler::currentPID() {
    return tls_currentPID;
}

void ProcessScheduler::setCurrentPID(PID pid) {
    tls_currentPID = pid;
}

void ProcessScheduler::schedule(PID pid) {
    nt_mutex_lock(&readyMutex);
    readyQueue.push(pid);
    nt_cond_signal(&readyCV);
    nt_mutex_unlock(&readyMutex);
}

void ProcessScheduler::workerLoop(size_t workerId) {
    (void)workerId;
    
    VM workerVM;
    
    while (nt_atomic_load_bool(&running)) {
        PID pid = 0;
        
        nt_mutex_lock(&readyMutex);
        while (readyQueue.empty() && nt_atomic_load_bool(&running)) {
            nt_cond_wait(&readyCV, &readyMutex);
        }
        
        if (!nt_atomic_load_bool(&running)) {
            nt_mutex_unlock(&readyMutex);
            break;
        }
        
        if (!readyQueue.empty()) {
            pid = readyQueue.front();
            readyQueue.pop();
        }
        nt_mutex_unlock(&readyMutex);
        
        if (pid == 0) continue;
        
        Process* proc = getProcess(pid);
        if (!proc || proc->getState() == ProcessState::DEAD || proc->getState() == ProcessState::FINISHED) {
            continue;
        }
        
        executeProcess(proc, workerVM);
        nt_atomic_fetch_add_uint64(&stats.contextSwitches, 1);
    }
}

void ProcessScheduler::executeProcess(Process* proc, VM& vm) {
    setCurrentPID(proc->getPid());
    proc->setState(ProcessState::RUNNING);
    
    try {
        vm.load_module("process");
        vm.stack.clear();
        vm.frames.clear();
        
        vm.push(Value(static_cast<Callable*>(proc->function)));
        
        for (const auto& arg : proc->arguments) {
            vm.push(arg);
        }
        
        if (vm.callValuePublic(Value(static_cast<Callable*>(proc->function)), 
                               static_cast<int>(proc->arguments.size()))) {
            vm.runPublic();
        }
        
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
// ProcessVM - Native functions
// ============================================================================

Value ProcessVM::spawn(VM& vm, std::vector<Value> args) {
    (void)vm;
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
    
    std::vector<Value> funcArgs(args.begin() + 1, args.end());
    
    PID pid = ProcessScheduler::instance().spawn(func, funcArgs);
    
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
    int64_t timeout = -1;
    
    if (!args.empty() && args[0].type == ValueType::NUMBER) {
        timeout = static_cast<int64_t>(args[0].as.number);
    }
    
    PID pid = ProcessScheduler::currentPID();
    if (pid == 0) {
        throw std::runtime_error("receive() can only be called from a spawned process");
    }
    
    Message msg(0, Value());
    if (ProcessScheduler::instance().receive(pid, msg, timeout)) {
        return msg.data;
    }
    
    return Value();
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
    nt_sleep_ms(ms);
    return Value();
}

void ProcessVM::registerFunctions(VM& vm, std::shared_ptr<Environment> env) {
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
