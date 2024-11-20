Team Contributions:
Everything is done by 陳安楷

# 1-1. threads/thread.cc
## Thread::Sleep()
* Main purpose:
Relinquish the CPU, because the current thread has either *finished* or is *blocked* waiting on a synchronization

* Note:
1. assumes interrupts are already disabled
2. if no threads on ready queue, CPU should be idle until next I/O interrupt (so thread could be inside the ready queue)

Initializes next thread object:
```
    Thread *nextThread;
```
Do some assertion and debug code:
```
    ASSERT(this == kernel->currentThread);
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Sleeping thread: " << name);
    DEBUG(dbgTraCode, "In Thread::Sleep, Sleeping thread: " << name << ", " << kernel->stats->totalTicks);
```
change the current thread status to blocked
* status could either be `ready` `running` or `blocked`
```
    status = BLOCKED;
    // cout << "debug Thread::Sleep " << name << "wait for Idle\n";
```
when no thread is in the scheduler, CPU idle
```
    while ((nextThread = kernel->scheduler->FindNextToRun()) == NULL) {
        kernel->interrupt->Idle();  // no one to run, wait for an interrupt
    }
```
When there is a thread in schedular
```
    // returns when it's time for us to run
    kernel->scheduler->Run(nextThread, finishing);
```

## Thread::StackAllocate()
* purpose: Allocate and initialize an execution stack. Allowing each thread to have its own memory space for function calls and local variables.

* passed parameters:
`VoidFunctionPtr func`: the procedure to be called
`arg`: arguments passed to into the procedure

allocate a bounded array, if dereference outside the bounded area will cause a segmentation error.
```
void Thread::StackAllocate(VoidFunctionPtr func, void *arg) {
    stack = (int *)AllocBoundedArray(StackSize * sizeof(int));
```

Each ISA has unique characteristics that influence how certain operations are performed, especially those related to low-level memory management and stack handling. Having code for different ISA increases the portability of software

* `stacktop`: Definition: A variable in the code that holds the initial address of the top of the stack memory allocated for a thread.

stackTop andframe marker for different ISAs:
```
#ifdef PARISC
    // HP stack works from low addresses to high addresses
    // everyone else works the other way: from high addresses to low addresses
    stackTop = stack + 16;  // HP requires 64-byte frame marker
    stack[StackSize - 1] = STACK_FENCEPOST;
#endif

#ifdef SPARC
    stackTop = stack + StackSize - 96;  // SPARC stack must contains at
                                        // least 1 activation record
                                        // to start with.
    *stack = STACK_FENCEPOST;
#endif

#ifdef PowerPC                          // RS6000
    stackTop = stack + StackSize - 16;  // RS6000 requires 64-byte frame marker
    *stack = STACK_FENCEPOST;
#endif

#ifdef DECMIPS
    stackTop = stack + StackSize - 4;  // -4 to be on the safe side!
    *stack = STACK_FENCEPOST;
#endif

#ifdef ALPHA
    stackTop = stack + StackSize - 8;  // -8 to be on the safe side!
    *stack = STACK_FENCEPOST;
#endif

#ifdef x86
    // the x86 passes the return address on the stack.  In order for SWITCH()
    // to go to ThreadRoot when we switch to this thread, the return addres
    // used in SWITCH() must be the starting address of ThreadRoot.
    stackTop = stack + StackSize - 4;  // -4 to be on the safe side!
    *(--stackTop) = (int)ThreadRoot;
    *stack = STACK_FENCEPOST;
#endif
```
initializing the machine state of a newly created thread. Which includes 
* program counter (PCstate): points to threadroot
* startup program counter (StartupPCState): do some initialization for the thread using `ThreadBegin`
* Initial program counter (InitialPCState): points to func that's to be executed
* Initial argument state (InitialArgState): points to the argument that passed to func
* When done program counter (WhenDonePCState): points to `ThreadFinish` for cleanup
```
#ifdef PARISC
    machineState[PCState] = PLabelToAddr(ThreadRoot);
    machineState[StartupPCState] = PLabelToAddr(ThreadBegin);
    machineState[InitialPCState] = PLabelToAddr(func);
    machineState[InitialArgState] = arg;
    machineState[WhenDonePCState] = PLabelToAddr(ThreadFinish);
#else
    machineState[PCState] = (void *)ThreadRoot;
    machineState[StartupPCState] = (void *)ThreadBegin;
    machineState[InitialPCState] = (void *)func;
    machineState[InitialArgState] = (void *)arg;
    machineState[WhenDonePCState] = (void *)ThreadFinish;
#endif
}
```
## Thread::Finish()
* purpose: Called by ThreadRoot when a thread is done executing the forked procedure.
* note: tell the schedular to call the destructor once running on a different thread

disable the interrupt first
* interrupt can be disabled `IntOff` or enabled `IntOn` 
```
void Thread::Finish() {
    (void)kernel->interrupt->SetLevel(IntOff);
```
do assertion and debug 
```
    ASSERT(this == kernel->currentThread);
    DEBUG(dbgThread, "Finishing thread: " << name);
```
if all the threads are finished
* decrease the number of running threads
    * if the number of running threads == 0 then shut down Nachos 
```
    if (kernel->execExit && this->getIsExec()) {
        kernel->execRunningNum--;
        if (kernel->execRunningNum == 0) {
            kernel->interrupt->Halt();
        }
    }
```
the current thread has finished so call `thread::Sleep()`
```
    Sleep(TRUE);  // invokes SWITCH
    // not reached
}
```
## Thread::Fork()
* purpose: Invoke (*func)(arg), allowing caller and callee to execute
concurrently.

* passed parameters:
`VoidFunctionPtr func`: the procedure to be called
`arg`: arguments passed to into the procedure

* implementation step:
1. allocate a stack
2. initial the stack so that a call to SWITCH will run the procedure
3. put the thread on ready queue

Initialize schedular and interrupt object
```
void Thread::Fork(VoidFunctionPtr func, void *arg) {
    Interrupt *interrupt = kernel->interrupt;
    Scheduler *scheduler = kernel->scheduler;
    IntStatus oldLevel;
```
debug message and allocate stack using `StackAllocate`
```
    DEBUG(dbgThread, "Forking thread: " << name << " f(a): " << (int)func << " " << arg);
    StackAllocate(func, arg);
```
Turning off the interrupt and schdule the thread to ready queue

WHY turn off??
Disabling Interrupts (IntOff): Turning off interrupts means that the processor
will not respond to these interrupt signals. This is often done to enter a
critical section where shared resources are accessed, preventing race conditions or inconsistent states.

```
    oldLevel = interrupt->SetLevel(IntOff);
    scheduler->ReadyToRun(this);  // ReadyToRun assumes that interrupts
                                  // are disabled!
    (void)interrupt->SetLevel(oldLevel);
}
```
# 1-2. userprog/addrspace.cc
## AddrSpace::AddrSpace()
* what is address space?
An address space is the range of memory addresses that a process can use to access its data and instructions.
Each process operates as if it has its own dedicated memory

* purpose: the constructor to create a new address space

create a new page pageTable
```
AddrSpace::AddrSpace() {
    pageTable = new TranslationEntry[NumPhysPages];
```
initialize page content inside the page pageTable
* different attributes
`virtualPage`: page number in virtual memory
`physicalPage`: frame number
`valid`: whether page is loaded in physical memory
`use`: whether the page has been modified before, for page replacement algorithms
`dirty`: check if the page has been write before since loaded from disk
```
    for (int i = 0; i < NumPhysPages; i++) {
        pageTable[i].virtualPage = i;  // for now, virt page # = phys page #
        pageTable[i].physicalPage = i;
        pageTable[i].valid = TRUE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;
    }
```
initialize the memory to zero for clean space
```
    // zero out the entire address space
    bzero(kernel->machine->mainMemory, MemorySize);
}
```
## AddrSpace::Execute()
* purpose: OS execute the current thread

* passed parameters:
`char *fileName`: the name for thread executed 
change the CPU's task to the current thread
```
void AddrSpace::Execute(char *fileName) {
    kernel->currentThread->space = this;
```
initial registers, load page table, then run the user program
```
    this->InitRegisters();  // set the initial register values
    this->RestoreState();   // load page table register

    kernel->machine->Run();  // jump to the user progam

    ASSERTNOTREACHED();  // machine->Run never returns;
                         // the address space exits
                         // by doing the syscall "exit"
}
```

## AddrSpace::Load()
* purpose: Load a user program into memory from a file

* passed parameters:
`char *fileName`: the file containing the object code to load into memory

open the executable file using fileSystem, initializes NoffHeader `noffH`
```
bool AddrSpace::Load(char *fileName) {
    OpenFile *executable = kernel->fileSystem->Open(fileName);
    NoffHeader noffH;
    unsigned int size;
```
case when the executable cannot be open
```
    if (executable == NULL) {
        cerr << "Unable to open file " << fileName << "\n";
        return FALSE;
    }
```
verify that the file is of the expected format before processing it.
if it's not `NOFFMAGIC` you should swap it
SwapHeader so `noffH.noffMagic` == `NOFFMAGIC`
```
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) &&
        (WordToHost(noffH.noffMagic) == NOFFMAGIC))
        SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);
```
RDATA is for read only file. Here we calculate the address space needed
```
#ifdef RDATA
    // how big is address space?
    size = noffH.code.size + noffH.readonlyData.size + noffH.initData.size +
           noffH.uninitData.size + UserStackSize;
    // we need to increase the size
    // to leave room for the stack
#else
    // how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size + UserStackSize;  // we need to increase the size
                                                                                           // to leave room for the stack
#endif
```
calculate how many pages needed using `numPages`
```
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    ASSERT(numPages <= NumPhysPages);  // check we're not trying
                                       // to run anything too big --
                                       // at least until we have
                                       // virtual memory

    DEBUG(dbgAddr, "Initializing address space: " << numPages << ", " << size);
```
Copy the code `noffH.code` and data segments `noffH.initData` into main memory.
Using `OpenFile::ReadAt(char *into, int numBytes, int position)`
```
    // Note: this code assumes that virtual address = physical address
    if (noffH.code.size > 0) {
        DEBUG(dbgAddr, "Initializing code segment.");
        DEBUG(dbgAddr, noffH.code.virtualAddr << ", " << noffH.code.size);
        executable->ReadAt(
            &(kernel->machine->mainMemory[noffH.code.virtualAddr]),
            noffH.code.size, noffH.code.inFileAddr);
    }
    if (noffH.initData.size > 0) {
        DEBUG(dbgAddr, "Initializing data segment.");
        DEBUG(dbgAddr, noffH.initData.virtualAddr << ", " << noffH.initData.size);
        executable->ReadAt(
            &(kernel->machine->mainMemory[noffH.initData.virtualAddr]),
            noffH.initData.size, noffH.initData.inFileAddr);
    }
```
Then, we write readonlyData into main memory
```
#ifdef RDATA
    if (noffH.readonlyData.size > 0) {
        DEBUG(dbgAddr, "Initializing read only data segment.");
        DEBUG(dbgAddr, noffH.readonlyData.virtualAddr << ", " << noffH.readonlyData.size);
        executable->ReadAt(
            &(kernel->machine->mainMemory[noffH.readonlyData.virtualAddr]),
            noffH.readonlyData.size, noffH.readonlyData.inFileAddr);
    }
#endif
```
close file at the end of load file
```
    delete executable;  // close file
    return TRUE;        // success
}

```
# 1-3. threads/kernel.cc
## Kernel::Kernel()
* purpose: Constructor for `Kernel` object to interpret command line arguments in order to determine flags
* initialize:
`argc`: argument count
`argv`: argument vector that points to the arguments

initialize flags variables for kernel object
```
Kernel::Kernel(int argc, char **argv) {
    randomSlice = FALSE;
    debugUserProg = FALSE;
    execExit = FALSE;
    consoleIn = NULL;   // default is stdin
    consoleOut = NULL;  // default is stdout
#ifndef FILESYS_STUB
    formatFlag = FALSE;
#endif
    reliability = 1;  // network reliability, default is 1.0
    hostName = 0;     // machine id, also UNIX socket name
                      // 0 is the default machine id
```
what each flag does:
* -rs: enable pseudo random time slicing
```
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-rs") == 0) {
            ASSERT(i + 1 < argc);
            RandomInit(atoi(argv[i + 1]));  // initialize pseudo-random
                                            // number generator
            randomSlice = TRUE;
            i++;
```
* -s: enable single step debugging
```
        } else if (strcmp(argv[i], "-s") == 0) {
            debugUserProg = TRUE;
```
* -e: add another executable file
```
        } else if (strcmp(argv[i], "-e") == 0) {
            execfile[++execfileNum] = argv[++i];
            cout << execfile[execfileNum] << "\n";
```
* -ee: exit when all the threads finished running
```
        } else if (strcmp(argv[i], "-ee") == 0) {
            // Added by @dasbd72
            // To end the program after all the threads are done
            execExit = TRUE;
```
* -ci: specifies the file to read input from
```
        } else if (strcmp(argv[i], "-ci") == 0) {
            ASSERT(i + 1 < argc);
            consoleIn = argv[i + 1];
            i++;
```
* -co: specifies the file to output to
```
        } else if (strcmp(argv[i], "-co") == 0) {
            ASSERT(i + 1 < argc);
            consoleOut = argv[i + 1];
            i++;

#ifndef FILESYS_STUB
        } else if (strcmp(argv[i], "-f") == 0) {
            formatFlag = TRUE;
#endif
```
* -n: likelyhood messages are dropped
```
        } else if (strcmp(argv[i], "-n") == 0) {
            ASSERT(i + 1 < argc);  // next argument is float
            reliability = atof(argv[i + 1]);
            i++;
```
* -m: set machine id
```
        } else if (strcmp(argv[i], "-m") == 0) {
            ASSERT(i + 1 < argc);  // next argument is int
            hostName = atoi(argv[i + 1]);
            i++;
```
* -u: print out Partial usage information
```
        } else if (strcmp(argv[i], "-u") == 0) {
            cout << "Partial usage: nachos [-rs randomSeed]\n";
            cout << "Partial usage: nachos [-s]\n";
            cout << "Partial usage: nachos [-ci consoleIn] [-co consoleOut]\n";
#ifndef FILESYS_STUB
            cout << "Partial usage: nachos [-nf]\n";
#endif
            cout << "Partial usage: nachos [-n #] [-m #]\n";
        }
    }
}
```

## Kernel::ExecAll()
* purpose: execute all the executable file inside the main memory
* recall: use -e to add executable file

iterate through all the executable file, execute them in kernel using `Exec` (trace next!)
```
void Kernel::ExecAll() {
    for (int i = 1; i <= execfileNum; i++) {
        int a = Exec(execfile[i]);
    }
```
Call Finish to release thread memory used in `Exec(char *name)`
[Finish](##Thread::Finish())
```
    currentThread->Finish();
    // Kernel::Exec();
}
```

## Kernel::Exec()
* purpose: Execute the executable file by creating a new thread and allocate address space for the thread
* passed parameters: `char *name`: the debug name for the created thread

Create a new thread object within `Kernel` class 
* t: an array of 10 pointers to `Thread` objects within `Kernel` class
```
int Kernel::Exec(char *name) {
    t[threadNum] = new Thread(name, threadNum);
```
1. set the state of thread object to is executing
2. allocate address space for the thread
3. call `Fork()` on the current thread to start running `Thread *t`
```
    t[threadNum]->setIsExec();
    t[threadNum]->space = new AddrSpace();
    t[threadNum]->Fork((VoidFunctionPtr)&ForkExecute, (void *)t[threadNum]);
    threadNum++;
```
return the `threadNum` that was created
```
    return threadNum - 1;
}
```
## Kernel::ForkExecute()
* purpose: to execute the forked thread

* passed parameters:
`Thread *t`: the pointer to the thread object
```
void ForkExecute(Thread *t) {
    if (!t->space->Load(t->getName())) {
        return;  // executable not found
    }
```
execute the thread using [Execute](#addrspaceexecute)
```
    t->space->Execute(t->getName());
}
```

# 1-4. threads/scheduler.cc
## Scheduler::ReadyToRun()
* purpose: Mark a thread as ready, but not running. Put it on the ready list, for later scheduling onto the CPU.
* passed parameters:
`Thread *thread`: the thread pointer that points to the current thread object

some debug/assertion check 
```
void Scheduler::ReadyToRun(Thread *thread) {
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
    // cout << "Putting thread on ready list: " << thread->getName() << endl ;
```
set the status of current thread to `READY` and append to ready list
* `List<Thread *> *Scheduler::readyList`: queued ready thread implemented with linked-list
```
    thread->setStatus(READY);
    readyList->Append(thread);
}
```
## Scheduler::Run()
* purpose: Dispatch the CPU to nextThread. Use context switch routine to switch to next thread
* Note: assume current running thread has already changed from running to blocked or ready
* passed parameters:
`Thread *nextThread`: thread object to be executed
`bool finishing`: is set if the current thread is to be deleted once we're no longer running on its stack

create a `Thread` pointer that points to old thread
```
void Scheduler::Run(Thread *nextThread, bool finishing) {
    Thread *oldThread = kernel->currentThread;
```
delete old thread if `finishing` is set
```
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {  // mark that we need to delete current thread
        ASSERT(toBeDestroyed == NULL);
        toBeDestroyed = oldThread;
    }
```
save the state of user program thread
```
    if (oldThread->space != NULL) {  // if this thread is a user program,
        oldThread->SaveUserState();  // save the user's CPU registers
        oldThread->space->SaveState(); // save address space data
    }
```
if `oldThread` doesn't overflow, do context swtich with `nextThread`
```
    oldThread->CheckOverflow();  // check if the old thread
                                 // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running

    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());

    // This is a machine-dependent assembly language routine defined
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread); // Stop running oldThread, start run nextThread
```
debug messages, interrupt needs to be disabled so saving states of 2 process can be completed
```
    // we're back, running oldThread

    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());
```
destroy the old thread if it has been set `finishing` is true.
Otherwise, restore register state and address space state
```
    CheckToBeDestroyed();  // check if thread we were running
                           // before this one has finished
                           // and needs to be cleaned up

    if (oldThread->space != NULL) {     // if there is an address space
        oldThread->RestoreUserState();  // to restore, do it.
        oldThread->space->RestoreState();
    }
}
```
# Answering questions:
1. How does Nachos allocate the memory space for a new thread(process)?

Ans: In main function calls `Kernel::ExecAll()` function that runs all the executable file. In `Kernel::Exec`, new Thread object is being created
```
    t[threadNum] = new Thread(name, threadNum);
    t[threadNum]->setIsExec();
    t[threadNum]->space = new AddrSpace();
    t[threadNum]->Fork((VoidFunctionPtr)&ForkExecute, (void *)t[threadNum]);
    threadNum++;

    return threadNum - 1;
```
So the virtual space and physical space mapping is done by dynamic allocating a new `AddrSpace()` While in `Thread::Fork()`, `Thread::StackAllocate()` the system allocates and initializes an execution stack. Allowing each thread to have its own memory space for function calls and local variables. 

2. How does Nachos initialize the memory content of a thread(process), including loading the user binary code in the memory?

Inside Kernel::Exec(char *name)
```
int Kernel::Exec(char *name) {
    t[threadNum] = new Thread(name, threadNum);
    t[threadNum]->setIsExec();
    t[threadNum]->space = new AddrSpace();
    t[threadNum]->Fork((VoidFunctionPtr)&ForkExecute, (void *)t[threadNum]);
    threadNum++;

    return threadNum - 1;
```

1. Nachos uses `AddrSpace()` to initialize a new space for the thread. 

2. Then `t[threadNum]->Fork((VoidFunctionPtr)&` loads binary inside the virtual table and physical space. While calls `StackAllocate` which allocates space for `this->InitRegisters()` and `this->RestoreState()`
```
void Thread::Fork(VoidFunctionPtr func, void *arg) {
    Interrupt *interrupt = kernel->interrupt;
    Scheduler *scheduler = kernel->scheduler;
    IntStatus oldLevel;

    DEBUG(dbgThread, "Forking thread: " << name << " f(a): " << (int)func << " " << arg);
    StackAllocate(func, arg);

    oldLevel = interrupt->SetLevel(IntOff);
    scheduler->ReadyToRun(this);  // ReadyToRun assumes that interrupts
                                  // are disabled!
    (void)interrupt->SetLevel(oldLevel);
}
```
```
void AddrSpace::Execute(char *fileName) {
    kernel->currentThread->space = this;

    this->InitRegisters();  // set the initial register values
    this->RestoreState();   // load page table register

    kernel->machine->Run();  // jump to the user progam

    ASSERTNOTREACHED();  // machine->Run never returns;
                         // the address space exits
                         // by doing the syscall "exit"
}
```

3. When `Thread::Fork()` is called, thread will be added in `readyList` it will be scheduled to run when ``Thread::Yield()` is called at `Interrupt::OneTick()` Then when `Schedular::FindNextToRun` is called at either `Thread::Sleep()` or `Thread::Yield()` we will get the next thread to run and thus achieving multiprogramming.

3. How does Nachos create and manage the page table?

create the page table by dynamically allocate an array with size of the executable file. Then, we load data page by page while setting the below attributes. 

* different attributes
`virtualPage`: page number in virtual memory
`physicalPage`: frame number
`valid`: whether page is loaded in physical memory
`use`: whether the page has been modified before, for page replacement algorithms
`dirty`: check if the page has been write before since loaded from disk

```
    for (int i = 0; i < NumPhysPages; i++) {
        pageTable[i].virtualPage = i;  // for now, virt page # = phys page #
        pageTable[i].physicalPage = i;
        pageTable[i].valid = TRUE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;
    }
```
initialize the physical memory to zero for clean space to put page data.
```
// zero out the entire address space
    bzero(kernel->machine->mainMemory + pageid * PageSize, PageSize);
}
```
4. How does Nachos translate addresses?

In `AddrSpace::Translate` we can find the following formulas, thus knowing how Nachos translate addreesses

1. virtual page number: `unsigned int vpn = vaddr / PageSize;`
2. offset: `unsigned int offset = vaddr % PageSize`
3. page table entry: ` pte = &pageTable[vpn];`
4. physical frame number: `pfn = pte->physicalPage;`

Physical Addreess: `*paddr = pfn * PageSize + offset;`

5. How Nachos initializes the machine status (registers, etc) before running a thread(process)?

At `Thread::Fork()` it calls `StackAllocate(func, arg)`

`func` includes calling `InitRegisters()` and `RestoreState()` which also sets up registers.

In `StackAllocate()` it sets up these information

* program counter (PCstate): points to threadroot
* startup program counter (StartupPCState): do some initialization for the thread using ThreadBegin
* Initial program counter (InitialPCState): points to func that's to be executed
* Initial argument state (InitialArgState): points to the argument that passed to func
* When done program counter (WhenDonePCState): points to ThreadFinish for cleanup

After setting up the machine status, CPU(mips simulator) can fetch instructions and run.


6. Which object in Nachos acts the role of process control block

In thread.h we can find these:

class Thread` act as process control block. Which has  thread states like 
```
// Thread state
enum ThreadStatus { JUST_CREATED,
                    RUNNING,
                    READY,
                    BLOCKED,
                    ZOMBIE };
```
`. Which corresponds to create, run, wait, end like the process control block.

7. When and how does a thread get added into the ReadyToRun queue of Nachos CPU scheduler?

When `Thread::Fork()` is called, thread will be added in `readyList` it will be scheduled to run when ``Thread::Yield()` is called at `Interrupt::OneTick()` Then when `Schedular::FindNextToRun` is called at either `Thread::Sleep()` or `Thread::Yield()` we will get the next thread to run and thus achieving multiprogramming.

```
void Scheduler::ReadyToRun(Thread *thread) {
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
    // cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    readyList->Append(thread);
}
```
When we call `Kernel::ExecAll()`, we will call Kernel::Exec() based on how many user program we and create thread using `fork()` and allocate stack. Then call `schedular->ReadyToRun(this)` add to `ReadyList`

# Code implementation

### machine.h
1. added a exception type called MemoryLimException to catch insufficient memory 

```
enum ExceptionType { NoException,            // Everything ok!
                     SyscallException,       // A program executed a system call.
                     PageFaultException,     // No valid translation found
                     ReadOnlyException,      // Write attempted to page marked
                                             // "read-only"
                     BusErrorException,      // Translation resulted in an
                                             // invalid physical address
                     AddressErrorException,  // Unaligned reference or one that
                                             // was beyond the end of the
                                             // address space
                     OverflowException,      // Integer overflow in add or sub.
                     IllegalInstrException,  // Unimplemented or reserved instr.
                    
                     MemoryLimException,     // added: Insufficient memory
                     NumExceptionTypes
};
```
2. created a bitmap to check the unallocated space in `mainMemory`

In `addrspace.h`

Create a bitmap for knowing which frame has been used
```
extern Bitmap available_frame;    // Frame allocation bitmap
```
helper functions to help loading segments into `mainMemory` 
```
    bool LoadSegment(OpenFile *executable, Segment &segment, unsigned int startVPN, bool readOnly);
    bool LoadSegments(OpenFile *executable, NoffHeader &noffH);

```
In `addrspace.cc`

deallocating the frames one by one on `pageTable` 
```
AddrSpace::~AddrSpace() {
    for (unsigned int i = 0; i < numPages; i++) {
        if (pageTable[i].valid) {
            available_frame.Clear(pageTable[i].physicalPage);
        }
    }
    delete[] pageTable;
}
```
In `addrspace::Load()`
initialize the page table and call `LoadSegments(executable, noffH)` 
```
// checking total size stuff

    // Initialize page table entries with virtual page numbers
    for (unsigned int i = 0; i < numPages; i++) {
        pageTable[i].virtualPage = i;
        pageTable[i].physicalPage = -1;  // Physical page not assigned yet
        pageTable[i].valid = FALSE;      // Not loaded into memory yet
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;
    }

    // Load segments into memory
    if (!LoadSegments(executable, noffH)) {
        delete[] pageTable;
        delete executable;
        return FALSE;
    }

    delete executable;  // Close the executable file
    return TRUE;        // Loading successful
}
```
In `LoadSegments` we load `noffH.code` `noffH.initData` `noffH.readonlyData` `noffH.uninitData` and `UserStack`
```
bool AddrSpace::LoadSegments(OpenFile *executable, NoffHeader &noffH) {
    unsigned int currVPN = 0;

    // Load code segment
    if (noffH.code.size > 0) {
        if (!LoadSegment(executable, noffH.code, currVPN, FALSE)) {
            return FALSE;
        }
        currVPN += divRoundUp(noffH.code.size, PageSize);
    }

    // Load initialized data segment
    if (noffH.initData.size > 0) {
        if (!LoadSegment(executable, noffH.initData, currVPN, FALSE)) {
            return FALSE;
        }
        currVPN += divRoundUp(noffH.initData.size, PageSize);
    }

#ifdef RDATA
    // Load read-only data segment
    if (noffH.readonlyData.size > 0) {
        if (!LoadSegment(executable, noffH.readonlyData, currVPN, TRUE)) {
            return FALSE;
        }
        currVPN += divRoundUp(noffH.readonlyData.size, PageSize);
    }
#endif

    // **Load uninitialized data segment**
    if (noffH.uninitData.size > 0) {
        if (!LoadSegment(executable, noffH.uninitData, currVPN, FALSE)) {
            return FALSE;
        }
        currVPN += divRoundUp(noffH.uninitData.size, PageSize);
    }
```
We have to initialize the pageTable because when we were counting the `size` of executable file the stack is not included
```

    // **Allocate stack pages**
    unsigned int stackPages = divRoundUp(UserStackSize, PageSize);
    for (unsigned int i = 0; i < stackPages; i++) {
        unsigned int vpn = currVPN + i;
        int pageid = available_frame.FindAndSet();
        if (pageid == -1) {
            cerr << "No free frames available for stack allocation.\n";
            return FALSE;
        }

        pageTable[vpn].virtualPage = vpn;
        pageTable[vpn].physicalPage = pageid;
        pageTable[vpn].valid = TRUE;
        pageTable[vpn].readOnly = FALSE;
        pageTable[vpn].use = FALSE;
        pageTable[vpn].dirty = FALSE;

        // Zero out the stack page
        bzero(kernel->machine->mainMemory + pageid * PageSize, PageSize);
    }
    currVPN += stackPages;

    return TRUE;
}
```
Then we just load the segment to mainMemory. Firstly, use `Bitmap::FindAndSet()` to check if there is availabe frame. Then, zero out physical page and read segment data into memory
```
bool AddrSpace::LoadSegment(OpenFile *executable, Segment &segment, unsigned int startVPN, bool readOnly) {
    unsigned int numPages = divRoundUp(segment.size, PageSize);

    for (unsigned int i = 0; i < numPages; i++) {
        unsigned int vpn = startVPN + i;

        // Allocate a physical frame
        int pageid;
        {
            pageid = available_frame.FindAndSet();
            DEBUG(98, "Allocated frame " << pageid);
        }
        if (pageid == -1) {
            cerr << "No free frames available for allocation.\n";
            kernel->interrupt->setStatus(SystemMode);
            ExceptionHandler(MemoryLimitException);
            kernel->interrupt->setStatus(UserMode);
        }

        // Update page table entry
        pageTable[vpn].physicalPage = pageid;
        pageTable[vpn].valid = TRUE;
        pageTable[vpn].readOnly = readOnly;

        // Zero out the physical page
        bzero(kernel->machine->mainMemory + pageid * PageSize, PageSize);

        // Calculate the physical address
        unsigned int paddr = pageid * PageSize;

        // Read the segment data into memory
        unsigned int readSize = PageSize;
        unsigned int remainingSize = segment.size - i * PageSize;
        if (remainingSize < PageSize) {
            readSize = remainingSize;
        }

        executable->ReadAt(
            kernel->machine->mainMemory + paddr,
            readSize,
            segment.inFileAddr + i * PageSize);
    }

    return TRUE;
}
```
End of code explanation.