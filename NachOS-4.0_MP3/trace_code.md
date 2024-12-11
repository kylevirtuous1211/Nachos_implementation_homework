<div style="page-size: A4; margin: 0; text-align: center;">

# **Operating systems MP3**





---

## Team member: 陳安楷、林庭宇
### Date: 12/2/2024

---

#### **Abstract: Implemented triple level queue that simulates CPU scheduling**  

</div>

## Job distribution:
* Report: 
    * 陳安楷 implementation + trace code 大部分
    * 林庭宇負責 trace Ready→Running
* Implementation: 陳安楷 (寫到剩沒過 testcase4)
* Debug: 林庭宇 (debug 成功！)

## Process State New -> Ready

1. `Kernel::ExecAll()` calls `Exec(execfile[i])` on all the execfile
2. `Exec(char *name)` initializes a new thread, `SetIsExec` to true,
 allocate virtual memory space for the thread and calls `Fork(VoidFunctionPtr func, void *arg)`
3. `Fork(VoidFunctionPtr func, void *arg)` calls `Execute(char *fileName)` in the thread's virtual space, 
it calls `InitRegisters()` and `RestoreState()` to the virtual address space
* `InitRegisters()`: initializes `PCReg` and `NextPCReg` and `StackReg`
* `RestoreState()`: restores pageTable and PageTableSize back to machine(simulated CPU)
In `Fork(VoidFunctionPtr func, void *arg)` function, firstly, we create a Interrupt pointer object and Scheduler pointer object that points to `kernel->interrupt` and `kernel->schedular`
Then calls `Thread::StackAllocate(VoidFunctionPtr func, void *arg)` 
4. `Thread::StackAllocate(VoidFunctionPtr func, void *arg)` allocates an bounded array.
```
void Thread::StackAllocate(VoidFunctionPtr func, void *arg) {
    stack = (int *)AllocBoundedArray(StackSize * sizeof(int));

```

<a id="readyToRun"></a>

5. After allocating stack, Fork calls `Schedular::ReadyToRun(Thread: *thread)`. Then append thread on `List<Thread *> *Scheduler::readyList` after setting thread's status to READY

## Process state Running -> Ready

1. Inside `Machine::Run()`. Firstly, we create a new `Instruction` class object pointer. Then we switch back to usermode, start running `OneInstruction(Instruction *instr)` function. After executing an instruction, we advance simulated time by calling `Interrupt::OneTick()` 
* `OneInstruction(Instruction *instr)` executes instruction based on `instr->opCode`
* `Interrupt::OneTick()` advances 10 each time interrupts are enabled and advances 1 each time user-level instruction ran on the `stats->totalTicks` timer .
2. Inside `Interrupt::OneTick()`,  after advancing the timer it calls `Interrupt::CheckIfDue(bool advanceClock)` to processes all pending interrupts that are due. It continues to run as long as there are pending interrupts and the next interrupt is due (i.e., its scheduled time is less than or equal to the current time). 
3. If we can context switch, then we yield the current thread. First in `Thread::Yield()` we create another `Thread *` pointer object which will point to the next scheduled thread object using `kernel->schedular->FindNextToRun()`

<a id="FindNextToRun"></a>

4. In `Schedular::FindNextToRun()` the function returns the front of `readyList` (FIFO).
5. In `Schedular::ReadyToRun(Thread *thread)`
append a thread to readyList
<a id=Run></a>

6. When the thread is in `readyList`, it has the possibility to be called by the schedular to run in `Scheduler::Run()`. Firstly, we have to save the old thread's state, then switch to the selected thread using `SWITCH(oldThread, nextThread);`. Release old thread using `CheckToBeDestroyed` and restore `oldThread` address space

## Process state Running -> Waiting
who calls synchConsoleOutput->PutChar(char ch)? 
好像如果是有需要同步化問題的輸出就要用`synchConsoleOutput()` namespace 裡面的東西
1. inside ` SynchConsoleOutput::PutChar(char ch)` we use `Lock->Acquire()` to do the following: consumes semaphore's value and set the locker holder to the current thread.
```
void Lock::Acquire() {
    semaphore->P();
    lockHolder = kernel->currentThread;
}
```
then print character asynchronously using `consoleOutput->PutChar(ch)`. The next line, `waitFor` will block the thread until `SynchConsoleInput::CallBack()` signals the write operation is complete. At the end we can release the lock `Lock->Release()` allowing other thread to print characters.
2. `Semaphore::P()` what is mainly does is consume semaphore value. If value is 0, means there is no resources; therefore, append thread to queue and set to calls `Thread::Sleep()` which sets `currentThread` status to `BLOCKED` and run `nextThread`
3. `List<T>::Append(T item)` appends item to the end of a linked list.
<a id="sleep"></a>

4. As I explained in 2. To add more detail, when there is no thread in scheduler's `readyList` set the interrupt status to idle.
5. explained in [2-4](#FindNextToRun)
6. explained in [2-6](#Run)

## Waiting -> Ready
1. Remove a waiting thread from `Semaphore::queue` and set the thread's status as READY. Then we increase the value of semaphore so waiting thread in the critical section can be run by machine.
2. explained in [1-5](#readyToRun)

## Running -> Terminated
1. In the case `SC_Exit` of the ExceptionHandler, calls `kernel->currentThread->Finish()`
2. In `kernel->currentThread->Finish()` we call this function to finish the thread. Firstly, we check if execution exit condition is met; if so, we decrease number of running threads. Else, we call `Thread::Sleep`.
3. explained in [3-4](#sleep)
4. explained in [2-4](#FindNextToRun)
5. explained in [2-6](#Run)

## Ready -> Running
1. explained in [2-4](#FindNextToRun)
2. explained in [2-6](#Run)
3. SWITCH

when we create a new thread. 
We call Thread::StackAllocate(VoidFunctionPtr func, void *arg).

```c++
void Thread::StackAllocate (VoidFunctionPtr func, void *arg)
{
    stack = (int *) AllocBoundedArray(StackSize * sizeof(int));
    stackTop = stack + StackSize - 4;   // -4 to be on the safe side!
    *(--stackTop) = (int) ThreadRoot;
    *stack = STACK_FENCEPOST;
    machineState[PCState] = (void*)ThreadRoot;
    machineState[StartupPCState] = (void*)ThreadBegin;
    machineState[InitialPCState] = (void*)func;
    machineState[InitialArgState] = (void*)arg;
    machineState[WhenDonePCState] = (void*)ThreadFinish;
}
```

So,
machineState[7] stores (void*)ThreadRoot.

machineState[2] stores (void*)ThreadBegin.

machineState[5] stores (void*)func.

machineState[3] stores (void*)arg.

machineState[6] stores (void*)ThreadFinish.

based on the following code, when calling SWITCH(oldthread,newthread), it will first 
```
1.push nextThread 
2.then push newThread then  
3.call SWITCH.
```
```assembly
/* void SWITCH( thread *t1, thread *t2 )
**
** on entry, stack looks like this:
**      8(esp)  ->              thread *t2
**      4(esp)  ->              thread *t1
**       (esp)  ->              return address
**
** we push the current eax on the stack so that we can use it as
** a pointer to t1, this decrements esp by 4, so when we use it
** to reference stuff on the stack, we add 4 to the offset.
*/
```

then in the implementation of SWITCH, it 
```
1.saved the register into machinestate of old thread
2.saved the return address to the machinestate of old thread
3.let %eax point to new thread
4.save the data in new thread into register
5.save the return address from new thread to register
```
the code of the steps can follow following code
```
.globl SWITCH
SWITCH:
#Step1
    # Save the value of eax
    movl    %eax,_eax_save

    # Move pointer to oldThread into eax
    movl    4(%esp),%eax

    # Save registers into oldThread's machineState
    movl    %ebx,_EBX(%eax)
    movl    %ecx,_ECX(%eax)
    movl    %edx,_EDX(%eax)
    movl    %esi,_ESI(%eax)
    movl    %edi,_EDI(%eax)
    movl    %ebp,_EBP(%eax)
    movl    %esp,_ESP(%eax)  # Save stack pointer
#Step2
    # Get the saved value of eax
    movl    _eax_save,%ebx
    movl    %ebx,_EAX(%eax)  # Store it in oldThread's machineState

    # Get return address from stack into ebx
    movl    0(%esp),%ebx
    movl    %ebx,_PC(%eax)   # Save it into the pc storage
#Step3
    # Move pointer to nextThread into eax
    movl    8(%esp),%eax

    # Get new value for eax into ebx
    movl    _EAX(%eax),%ebx
    movl    %ebx,_eax_save   # Save it
#Step4
    # Restore registers from nextThread's machineState
    movl    _EBX(%eax),%ebx
    movl    _ECX(%eax),%ecx
    movl    _EDX(%eax),%edx
    movl    _ESI(%eax),%esi
    movl    _EDI(%eax),%edi
    movl    _EBP(%eax),%ebp
    movl    _ESP(%eax),%esp  # Restore stack pointer
#Step5
    # Restore return address into eax
    movl    _PC(%eax),%eax
    movl    %eax,4(%esp)     # Copy over the ret address on the stack
    movl    _eax_save,%eax

    # Return to the restored address
    ret
```

ThreadRoot

The function ThreadRoot expects the following registers to be initialized:
```
eax: Points to the startup function (interrupt enable).
edx: Contains the initial argument to the thread function.
esi: Points to the thread function.
edi: Points to Thread::Finish().
```
```asssembly
pushl   %ebp
movl    %esp, %ebp
pushl   InitialArg      # Passes the argument (arg)
call    *StartupPC      # Executes (void*)ThreadBegin
call    *InitialPC      # Executes (void*)func
call    *WhenDonePC     # Executes (void*)ThreadFinish

# NOT REACHED
movl    %ebp, %esp
popl    %ebp
ret
```
To sum up, the steps of this scenario is:
```
1.find pending thread by FindNextToRun(), then Scheduler::Run()
2.In scheduler::run(), the thread's state will be Ready->Running then execute SWITCH();
3.if the thread had been SWITCH before:
    execute form the place of last executes
  if not:
    call Thread::begin
    callfunc(forkexecute())
    call ThreadFinish()
```


## Homework Implementation
### In `thread.h`
We created a `OrderManager` class for each thread object to record burst time, priority. Therefore, we can have easier implementation of our three level queue
```

    class OrderManager { // for changing thread states
    public:
        static const int AGING_TICK = 1500;
        OrderManager(Thread *t, int initPriority);
        void newToReady();
        // collect information when the thread start runniing
        void readyToRun(Thread *lastThread); 
        
        void runToReady();
        void runToWait();
        void waitToReady();
        void runToTerminated();
        // getter to protect private member variables
        double getRemainTime(); // remained execution time needed (approximation)
        int getPriority(); // get priority of current thread
        void aging(); // aging for priority
    private:
        Thread *thread;
        // for predicting remain time
        double remainBurstTime;
        double currentBurstTime;
        double lastBurstTime;
        int priority;
        const int init_priority;
        // for record the time that enters running state
        int tick_cache;
        void setPriority(int priority);
        void toReady();
        void leaveRun();
    };
```
Also, created two comparator `compareTime(Thread * t1, Thread * t2)`, `comparePriority(Thread * t1, Thread * t2)` 
for SortedList. And a Aging mechanism for current thread to prevent starvation.
```
    static int compareTime (Thread* t1, Thread* t2);
    static int comparePriority (Thread* t1, Thread* t2);
    static void Aging(Thread* t);
```
Create `getPriority` thread public method to invoke orderManager's `getPriority()`
```
int getPriority() { return orderManager->getPriority(); }
```
`setStatus` to set the ThreadStatus (process states)
```
void setStatus(ThreadStatus st, Thread * last = NULL);
```
These are the thread's status which corresponds to the process states.
```
enum ThreadStatus { NEW,
                    READY,
                    RUNNING,
                    WAITING,
                    TERMINATED,
                    ZOMBIE };

```
Also, we add a `OrderManager* ` object as a member of class Thread
```
class Thread{
    public:
    ...
    private:
        OrderManager * orderManager
}
```
### thread.cc
Dynamic allocate a OrderManager in the constructor of Thread
```
Thread::Thread(char *threadName, int threadID, int initPriority) {
....
    orderManager = new OrderManager(this, initPriority);
....
}
```
Also, release the memory content at the destructor
```
Thread::~Thread() {
...
    delete orderManager;
...
}
```
Implementation of `Thread::setStatus(ThreadStatus st, Thread * last)`. We want to achieve is a systematic way to record last burst time, current burst time and update `tick_cache` to current statistics
```
void Thread::setStatus(ThreadStatus st, Thread * last) {
    if (st == READY) {
        switch(status) {
            case NEW:
                orderManager->newToReady();
                break;
            case RUNNING:
                orderManager->runToReady();
                break;
            case WAITING:
                orderManager->waitToReady();
                break;
            default:
                ASSERT(FALSE);
        }
    } else if (st == RUNNING) {
        ASSERT(status == READY);
        orderManager->readyToRun(last);
    } else if (st == WAITING) {
        ASSERT(status == RUNNING);
        orderManager->runToWait();
    } else if (st == TERMINATED) {
        ASSERT(status == RUNNING);
        orderManager->runToTerminated();
    }
    status = st;
}
```
`Thread::compareTime(Thread* t1, Thread* t2)` 
The compare function to compare the remain time of the thread. Returns true if `t1` has 
smaller remain time than `t2`
```
int Thread::compareTime(Thread* t1, Thread* t2) {
    ASSERT(t1->status == READY && t2->status == READY || 
    t1->status == READY && t2->status == RUNNING ||
    t1->status == RUNNING && t2->status == READY);

    return static_cast<int> (t1->orderManager->getRemainTime() - t2->orderManager->getRemainTime());
}
```
The compare function  to compare the priority. Returns true if 't2' is larger than 't1'
```
int Thread::comparePriority(Thread* t1, Thread* t2) {
    ASSERT(t1->status == READY && t2->status == READY || 
    t1->status == READY && t2->status == RUNNING ||
    t1->status == RUNNING && t2->status == READY);

    return static_cast<int> (t2->orderManager->getPriority() - t1->orderManager->getPriority());
}
```
Thread's aging method calls the `orderManager->aging()`.
```
void Thread::Aging(Thread* t) {
    t->orderManager->aging();
}
```
Implementation of `Thread::OrderManager::getPriority()`
```
int Thread::OrderManager::getPriority() {return priority;}
```
Implementation of `Thread::OrderManager::getRemainTime()`
There are 2 cases:
1. If the thread is still running, then we need to further minus the current bursting time.
2. Else, we just minus our exponential average result (SJF) with accumulated burst time.
```
double Thread::OrderManager::getRemainTime() {
    if (thread->status == RUNNING) {
        return remainBurstTime - (currentBurstTime + kernel->stats->totalTicks - tick_cache);
    } else {
        return remainBurstTime - currentBurstTime;
    }
}
```

### Thread::OrderManager
#### helper functions 
to help record correct burst time duration and priority value

`Thread::OrderManager::leaveRun()` 
This method is responsible for updating the CPU burst time metrics of a thread when it
transitions from the running state to another state (e.g., waiting or ready). Accurate tracking of burst times is crucial for implementing the Preemptive Shortest Job First (SJF) scheduling algorithm.
```
void Thread::OrderManager::leaveRun() {
    int currTick = kernel->stats->totalTicks;
    currentBurstTime += currTick - tick_cache;
    tick_cache = currTick;
    lastBurstTime = currentBurstTime;
}
```
`void Thread::OrderManager::toReady()`
Correcly updating `tick_cache` and resets priority
```
void Thread::OrderManager::toReady() {
    tick_cache = kernel->stats->totalTicks;
    priority = init_priority; // reset the priority (if aged)
}

#### process cycle
```
`Thread::OrderManager::runToWait()`
1. Firstly, calls `LeaveRun` because we left running state.
2. Secondly, we use exponential average formula to get the estimated `remainBurstTime` and logs the debug message.
3. Thirdly, reset `currentBurstTime` when run enters wait (spec).
```
void Thread::OrderManager::runToWait() {
    leaveRun();
    double oldRemainBurstTime = remainBurstTime;
    if (currentBurstTime) {
        remainBurstTime = 0.5 * currentBurstTime + 0.5 * remainBurstTime;
    }
    DEBUG(dbgQueue, "[D] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] update approximate burst time, from: ["<< oldRemainBurstTime << "], add [" << currentBurstTime << "], to [" << remainBurstTime << "]");

    currentBurstTime = 0; // reset current burst time T
}
```
`Thread::OrderManager::readyToRun(Thread * last)`
1. Set the `tick_cache` to current ticks
2. logs the information [E]
```
void Thread::OrderManager::readyToRun(Thread * last) {
    tick_cache = kernel->stats->totalTicks;
    if (last) {
        DEBUG(dbgQueue, "[E] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is now selected for execution,\
        thread [" << last->getID() << "] is replaced, and it has executed [" << last->orderManager->lastBurstTime << "] ticks");
    } else {
        DEBUG(dbgQueue, "[E] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] (main thread) starts its execution");
    }
}
```
`void Thread::OrderManager::runToReady()`
1. just leave run and set to ready.
```
void Thread::OrderManager::runToReady() {
    leaveRun();
    toReady();
}
```
Other methods for the process life cycle.
```
void Thread::OrderManager::waitToReady() {toReady();}
void Thread::OrderManager::newToReady() {toReady();}
void Thread::OrderManager::runToTerminated() {leaveRun();}
```
#### Aging 
Calculate the age by the time waiting inn ready queue.
* advance `tick_cache` so the thread won't age multiple times.
```
void Thread::OrderManager::aging() {
    ASSERT(thread->status == READY);

    int age = (kernel->stats->totalTicks - tick_cache)/AGING_TICK;
    if (age) {
        tick_cache += age * AGING_TICK; // advance tick_cache
        setPriority(min(priority+10*age, 149));
    }
}
```
`Thread::OrderManager::setPriority(int priority)` 
Set to the new priority and log the debug message [C] over here.
```
void Thread::OrderManager::setPriority(int priority) {
    if (this->priority != priority) {
        DEBUG(dbgQueue, "[C] Tick [" << kernel->stats->totalTicks 
            << "]: Thread [" << thread->getID()
            << "] changes its priority from [" 
            << this->priority << "] to [" << priority << "]");
        this->priority = priority;
    }
}
```
### In scheduler.h
1. Added two public functions `aging()` and `shouldPreempt`.
2. Also, create a public `MultilevelThreadQueue` class for triple queue implementation
3. Create enum `Level` member to simplify queue operation 
4. change the readyList type to MultilevelThreadQueue*
```
class Scheduler {
   public:
    void aging();

    bool shouldPreempt();

    // SelfTest for scheduler is implemented in class Thread
    class MultilevelThreadQueue {
      public:
         enum Level {
            Lv1, Lv2, Lv3  // simplify implementation
         };
         MultilevelThreadQueue();
         ~MultilevelThreadQueue();
         void Append(Thread* thread);
         bool IsEmpty();
         Thread * RemoveFront();
         Thread * Front();
         void Apply(void (* callback)(Thread *));
         void Aging();
         bool ShouldPreempt();
         void PromoteThreads(List<Thread*> * fromQueue, Level level);

     private:
         // preemptive SJF (priority 100-149)
         SortedList<Thread*> *L1;
         // non-preemptive (priority 50-99)
         SortedList<Thread*> *L2;
         // round robin per 100 ticks (priority 0-49)
         List<Thread*> *L3;
         Level getQueueLevel(Thread * t);
    };
   private:
      MultilevelThreadQueue * readyList;
};
```
### MultilevelThreadQueue Implementation
Constructor and destructor, initializes L1, L2, L3 queue
```
Scheduler::MultilevelThreadQueue::MultilevelThreadQueue() {
    L1 = new SortedList<Thread*>(Thread::compareTime);
    L2 = new SortedList<Thread*>(Thread::comparePriority);
    L3 = new List<Thread*>();
}

Scheduler::MultilevelThreadQueue::~MultilevelThreadQueue() {
    delete L1;
    delete L2;
    delete L3;
}
```
`Scheduler::MultilevelThreadQueue::Level Scheduler::MultilevelThreadQueue::getQueueLevel(Thread * t) `
Returns the thread's `Level` 
```
Scheduler::MultilevelThreadQueue::Level Scheduler::MultilevelThreadQueue::getQueueLevel(Thread * t) {
    int priority = t->getPriority();
    if (priority >= 100) {
        return Lv1;
    } else if (priority >= 50) {
        return Lv2;
    } else {
        return Lv3;
    }
}

```
void Scheduler::MultilevelThreadQueue::Append(Thread * thread)
Append based on which level the thread is on.
```
void Scheduler::MultilevelThreadQueue::Append(Thread *thread) {
    int level = getQueueLevel(thread);
    if (level == Lv1) {
        L1->Insert(thread);
    } else if (level == Lv2) {
        L2->Insert(thread);
    } else {
        L3->Append(thread);
    }
    DEBUG(dbgQueue, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" 
        << thread->getID() << "] is inserted into queue L[" << level + 1 << "]");
}
```
#### Aging
The `Timer::Interrupt` will fire about every 100 ticks. Which calls the `Alarm::Callback()` method we check **aging** and **preemption** at every alarm. 
```
void Alarm::CallBack() {
    Interrupt *interrupt = kernel->interrupt;
    MachineStatus status = interrupt->getStatus();

    kernel->scheduler->aging();

    if(status == IdleMode) {//only preempt when the machine is idle
        return;
    }
    else if( kernel->scheduler->shouldPreempt()) {
        interrupt->YieldOnReturn();
    }
}
```

`void Scheduler::MultilevelThreadQueue::Apply(void (*callback))`
 applies callback function on all three queues. So when we call `Scheduler::MultilevelThreadQueue::Aging()` we can check every queue at the same time. Also, we check if threads on L3 and L2 queue can promote to higher priority queue using `Scheduler::MultilevelThreadQueue::PromoteThreads(List<Thread*> *fromQueue, Level level)`.
```
void Scheduler::MultilevelThreadQueue::Apply(void (*callback)(Thread *)) {
    L1->Apply(callback);
    L2->Apply(callback);
    L3->Apply(callback);
}

void Scheduler::MultilevelThreadQueue::Aging() {
    Apply(Thread::Aging);
    PromoteThreads(L3, Lv3);
    PromoteThreads(L2, Lv2);
}
```
`Scheduler::MultilevelThreadQueue::PromoteThreads(List<Thread*> *fromQueue, Level level)`
Basically, we iterate through every thread in the queue and check if it belongs to higher level using `getQueueLevel(Thread * t)`. If so, then we remove from current queue and calls `Scheduler::MultilevelThreadQueue::Append(Thread *thread)` 
```
void Scheduler::MultilevelThreadQueue::PromoteThreads(List<Thread*> *fromQueue, Level level) {
    ListIterator<Thread*> it(fromQueue);
    while (!it.IsDone()) {
        Thread *t = it.Item();
        Level fromQueueLevel = getQueueLevel(t);
        if (fromQueueLevel < level) {
            fromQueue->Remove(t);
            DEBUG(dbgQueue, "[Aging] Tick [" << kernel->stats->totalTicks << "]: Thread [" 
                << t->getID() << "] promoted from L[" << level + 1 << "] to L[" << fromQueueLevel + 1 << "]");
            Append(t);
        }
        it.Next();
    }
}
```
`Scheduler::aging()` Simply call `readyList->Aging`
```
void Scheduler::aging() {
    readyList->Aging();
}
```

#### preemption
`bool Scheduler::MultilevelThreadQueue::ShouldPreempt()`
##### Logic:
* L3 queue always preempt (round robin) when alarm fires
* L2 nonpreemptive so doesn't need to preempt
* Therefore, if L1 has thread inside. L2 threads and remainTime that is longer will be preempt!
```
bool Scheduler::MultilevelThreadQueue::ShouldPreempt() {
    Thread *currentThread = kernel->currentThread;
    Level currentLevel = getQueueLevel(currentThread);

    if (currentLevel == Lv3) {
        return true;
    }

    if (!L1->IsEmpty() && (currentLevel == Lv2 || Thread::compareTime(currentThread, L1->Front()) > 0)) {
        return true;
    }

    return false;
}

```

`Scheduler::shouldPreempt()` Simply call `readyList->ShouldPreempt()`
```
bool Scheduler::shouldPreempt() {
    return readyList->ShouldPreempt();
}
```

#### other helper functions
During context switch, `Thread * Scheduler::FindNextToRun()` 
would call `Thread * Scheduler::MultilevelThreadQueue::RemoveFront()` to determine which thread is going to run next. So we use `Thread *List<Thread *>::RemoveFront()` member function to achieve this. At the end, we print the debug message as the spec described.
```
Thread * Scheduler::MultilevelThreadQueue::RemoveFront() {
    Thread * t;
    Level level;

    if (!L1->IsEmpty()) {
        t = L1->RemoveFront();
        level = Lv1;
    } else if (!L2->IsEmpty()) {
        t = L2->RemoveFront();
        level = Lv2;
    } else {
        t = L3->RemoveFront();
        level = Lv3;
    }
    DEBUG(dbgQueue, "[B] Tick [" << kernel->stats->totalTicks 
    << "]: Thread [" << t->getID() 
    << "] is removed from queue L[" << level+1 << "]");
    
    return t;
}
```
`Scheduler::MultilevelThreadQueue::IsEmpty()` Simply check if all three queue are all empty.
```
bool Scheduler::MultilevelThreadQueue::IsEmpty() {
    return L1->IsEmpty() && L2->IsEmpty() && L3->IsEmpty();
}
```

## Reflection
陳安楷：This is really hard and complicated but fun at the same time. After implementing MP3, I have a deeper understanding on process cycle and scheduling mechanisms such as aging and exponential average SJF. Thank you TA!!

