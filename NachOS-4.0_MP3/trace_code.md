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
3. 
