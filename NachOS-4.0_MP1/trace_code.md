Team Contributions:
| work | 陳安楷 | 林夢遠 |
|----------|----------|----------|
| Trace SC_Halt    | *****     |      |
| Trace SC_Create    | *****     |      |
| Trace SC_PrintInt    |      | *****     |
| Trace Makefile    | *****     |      |
| Write exception.cc    |      | *****     |
| Write ksyscall.h    |      | *****     |
|  Write filesys.h   | *****     |    |
|  Write syscall.h   | *****     |     |
|  Debug & testing  |     | *****    |

我們 code tracing 的報告各自打，然後第二部分寫system call報告由陳安楷撰寫


# Tracing SC_Halt system call

### how it passes to kernel?
`Machine->run` 讓 process 開始跑之後，不斷的讓 CPU 去用 `OneInstruction` 去 fetch instructions，在執行 instruction 的時候如果遇到 exception 那就要交給我們的 OS kernel 去處理，像是 Overflow, AddressError, I/O, 檔案讀寫。然後這邊在呼叫 `SysHalt()` 釋放 kernel 物件的記憶體

## machine/mipssim.cc Machine::run():

* goal: swtich to user mode and fetch instructions

In line 54, it allocates memory for Instruction object. 

line 56, it says debug mode for the machine is enabled, it would output system's thread's info and system's total ticks count

Line 60, it ***sets system's CPU to user mode*** so it could execute user-level instructions

Line 61, it starts **infinite loop** for executing instruction. Inside the loop, it executes the instruction inside the `OneInstruction()` function. Then after executing, the system **advances by one tick**

line 73, it debugs using single step mode. Therefore, if the `singleStep` flag is enabled and tick count is larger than runUntilTime, which is probably for controlled execution, the `Debugger` will be called.

## machine/mipssim.cc Machine::OneInstruction(Instruction *instr)

* goal: execute the instruction by fetching instruction using `ReadMem`

line 133~136: fetch instruction, store in `instr->value`, then Decode the instruction

### instr->Decode():
```
void Instruction::Decode() {
    OpInfo *opPtr;

    rs = (value >> 21) & 0x1f;
    rt = (value >> 16) & 0x1f;
    rd = (value >> 11) & 0x1f;
    opPtr = &opTable[(value >> 26) & 0x3f];
    opCode = opPtr->opCode;
    if (opPtr->format == IFMT) {
        extra = value & 0xffff;
        if (extra & 0x8000) {
            extra |= 0xffff0000;
        }
    } else if (opPtr->format == RFMT) {
        extra = (value >> 6) & 0x1f;
    } else {
        extra = value & 0x3ffffff;
    }
    if (opCode == SPECIAL) {
        opCode = specialTable[value & 0x3f];
    } else if (opCode == BCOND) {
        int i = value & 0x1f0000;

        if (i == 0) {
            opCode = OP_BLTZ;
        } else if (i == 0x10000) {
            opCode = OP_BGEZ;
        } else if (i == 0x100000) {
            opCode = OP_BLTZAL;
        } else if (i == 0x110000) {
            opCode = OP_BGEZAL;
        } else {
            opCode = OP_UNIMP;
        }
    }
}
```

138~147: for debug purpose

150: update program counter, +4 because a word is 4 bytes

155~696: Use the switch case structure for executing instructions. Because each instruction have a specific opCode.

## machine/machine.cc Machine::RaiseException

* goal: transfer control to Nachos kernel from user mode

* args: "which": the cause of kernel trap, "badVaddr": virtual address causing the trap

line 98: invoke `Debugger` 

line 99: store the virtual address that cause the trap inside a register

line 103: After finishing anything in progress, change to kernel mode and handle the exception, then return back to user mode.

## userprog/exception.cc ExceptionHandler()

* goal: designed to handle exceptions that occur during the execution of a user program in the Nachos kernel. Exceptions allow user programs to request services from the operating system through system calls. 

we can write more code over here to handle more exception cases 

* args: `which` is the cause of kernel trap

Parameters:
* `val`: Used to store values passed in registers or read from memory.

* `type`: Stores the **system call number**, which is in register 2 (r2) as part of the calling convention.

line 60~65: (SC_Halt) handles the SC_Halt system call, which shuts down the machine. It logs the event, invokes SysHalt() to perform the shutdown

line 66~78: (SC_PrintInt): Reads an integer value from register 4.Then, calls `SysPrintInt(val)` to print the integer.Finally, Adjusts the program counters (`PrevPCReg`, `PCReg`, `and` `NextPCReg`) to point to the next instruction, avoiding an infinite loop by not re-executing the same system call repeatedly.

line 79~88: (SC_MSG)
Reads a pointer from register 4.
Interprets the pointer as the starting address of a message string in the machine’s memory. Prints the message and halts the system using `SysHalt()` and asserts that the code should not continue.

line 89~102: (SC_Create):
Reads a pointer from register 4, which is the filename for a file to be created. Calls `SysCreate(filename)` to create the file.

Then writes the return status of the file creation to register 2. Finally, adjusts the program counters to move to the next instruction.

line 103~123: (SC_Add):
Reads two integer operands from registers 4 and 5. After calling `SysAdd()` to compute their sum and stores the result in register 2. Then Adjusts the program counters and prints the result.

line 128~132: (SC_Exit)
Reads an exit status from register 4 and prints it. Terminates the current thread by calling `kernel->currentThread->Finish()`.

## userprog/ksyscall.h SysHalt
* goal: signals the OS to stop, shutting down the simulated machine. For example when program finishes execution

line 17: kernel pointer calls the interrupt member, which calls `Halt()` member function that shuts down NachOS

## machine/interrupt.cc Interrupt::Halt() 
* goal: shuts down NachOS cleanly

line 229: after print out performance statistics, release the memory for kernel object

## Tracing SC_Create system call

## 和 Kernel 互動的方式：
執行 instruction exception之後，前往 SysCreate()去handle這個exception，sysCreate()就會去呼叫`fileSystem` 創建檔案並回傳值給`exception.cc`，在Create()裡面，如果`fileDescriptor` 是 -1的話那檔案就沒有成功被開啟`return FALSE`，最後有沒有創建成功會被存到 $v0 裡面，然後處理完這個exception後program counter會向前移。

## userprog/exception.cc ExceptionHandler()
Traced before

## userprog/ksyscall.h SysCreate()
* goal: Creates a file in NachOS file system. Return if the kernel has successfully created a file

See more in `Create()` to now extra details

## filesys/filesys.h FileSystem::Create()
* goal: create a new file. If fileDescriptor returned -1, it means the file couldn't be created and the function returns false.

line 57: If the file was opend, close the file using `Close` after reading and writing are finished

## Tracing SC_PrintInt system call

## SC_PrintInt 和 Kernel 互動的方式：

一開始從` exceptionHandler()` 接到 exception 之後，進到SysPrintInt()，經過ksyscall.h進入kernel `SynchConsoleOutput::PutInt()` 裡面，在PutInt()時將integer轉成char後再PutChar()，同時使用`lock` 來讓output()可以做好，尤其是multithreaded的環境。
為了保持interrupt的時序，context switch 沒有問題，用 `OneTick`來紀錄kernel space的時間，用`Schedule()`去排程什麼時候進行interrupt，因為Nachos虛擬環境沒有硬體來interrupt，那當造成interrupt的東西ex: I/O 結束後，就會呼叫 `callBack()`來繼續process執行

## ExceptionHandler()
- **Comment:** Reads the number `val` to be printed from the register.
- Calls `SysPrintInt(val)` to output the number.
- Advances the PC and returns, ensuring that `assert()` is not triggered for an unsuccessful return.
- **Summary:** In `ExceptionHandler()`, the part handling `SC_PrintInt` reads the number to be printed and calls the underlying `SysPrintInt` function to output the number, then advances the PC.

## ksyscall.h SysPrintInt()
- Outputs the current total ticks.
- Calls `SynchConsoleOutput::PutInt()` from the kernel.
- Outputs the current total ticks again.
- **Summary:** `SysPrintInt()` calls `PutInt()` and records the time spent (in ticks).

## SynchConsoleOutput::PutInt()
- Converts the input `value` into a string `str`.
- Locks before output to prevent race conditions during multithreading.
- Enters a while loop to output each character of `str` one by one, calling `consoleOutput->PutChar()` each time.
- After `PutChar`, calls `waitFor->P()` to ensure the output sequence is correct.
- Releases the lock after completing the output.
- **Summary:** `SynchConsoleOutput::PutInt()` outputs an integer to the console by converting it to a string and outputting each character one by one, including extensive debug messages to record operation times.

## SynchConsoleOutput::PutChar()
- Locks, then calls `consoleOutput->PutChar(ch)` to output `ch`.
- Releases the lock after the output completes.
- **Summary:** `SynchConsoleOutput::PutChar()` outputs a single character.

## ConsoleOutput::PutChar()
- Checks if `putBusy` is true (another `put` is in progress).
- Writes `ch` into `writeFileNo` (the file to which the console outputs).
- Sets `putBusy` to true to prevent other threads from causing race conditions before the output.
- Delays for a short time (`ConsoleTime`) before calling `interrupt` to perform `ConsoleWriteInt` for output.
- **Summary:** `consoleOutput->PutChar()` writes the character to `writeFileNo` for console output and triggers `ConsoleWriteInt` after a brief delay.

## Interrupt::Schedule()
- Calculates the time for the interrupt to trigger and initializes `toOccur` with `toCall`, trigger time, and `IntType type`.
- Excludes cases where `fromNow <= 0` (cases that are due immediately).
- Adds `toOccur` to the `pending` `SortedList` for future interrupt execution.
- **Summary:** `Interrupt::Schedule()` creates a schedule and adds it to the list for future execution.

## Machine::Run()
- Creates an `instr` pointer to hold the instruction and switches to User Mode.
- Enters a loop, continuously executing the next instruction (`OneInstruction()`), recording execution time, and calling `OneTick()` to simulate time advancement and check for interrupts.
- Calls `debugger()` if single-step debugging is enabled and the current time reaches the debug interval.
- **Summary:** `Machine::Run()` simulates the system's continuous process of fetching and executing instructions.

## Machine::OneTick()
- Advances time based on the current mode.
- Disables interrupts and calls `CheckIfDue(FALSE)` to check and execute pending interrupts. Passing `FALSE` means it doesn't jump directly to the next interrupt time but gradually advances.
- Re-enables interrupts afterward.
- Performs a context switch if `yieldOnReturn` is set and restores the mode to the original state (kernel or user mode).
- **Summary:** `Machine::OneTick()` advances time, checks for pending interrupts, gradually progresses without skipping directly to the next interrupt, and performs context switches as needed.

## Interrupt::CheckIfDue()
- Asserts that other interrupts are disabled.
- Runs debug checks and returns `FALSE` if the pending interrupt list is empty.
- Checks the earliest interrupt in the list. If the time hasn't reached, it determines whether to advance the clock. If not, it returns `FALSE`; if so, it jumps directly to the time.
- Executes `DelayedLoad()` to update delayed values before entering the kernel trap.
- Enters a handler loop to execute all due interrupts in the pending list, then exits and returns `TRUE` (as at least one interrupt will execute).
  
## ConsoleOutput::CallBack()
- Debugs, sets `putBusy` to `FALSE` (indicating that the current output operation is complete and new outputs can be accepted), and updates the output character count.
- Calls `CallBack()` for subsequent operations.
- **Summary:** `ConsoleOutput::CallBack()` handles the actions to be performed after output is complete and calls `callWhenDone->CallBack()` for further actions.

## SynchConsoleOutput::CallBack()
- Outputs debug messages.
- Executes `waitFor->V()` to unblock any process waiting on `waitFor->P()`.
- **Summary:** `SynchConsoleOutput::CallBack()` handles post-output operations by releasing synchronization locks.

## Tracing Makefile in code/test/MakeFile
* goal: to understand how testfiles are compiled

line 115: This variable lists the programs that will be built when you run make. Each program corresponds to a C source file (e.g.add.c for add, halt.c for halt, etc.).

```
else
# change this if you create a new test program!
PROGRAMS = add halt createFile LotOfAdd
endif
```

## how does the makefile work
* goal: for building user level programs that can run on MIPS simulator

* Procedure:
    * compile in .coff, then convert into .noff for nachos MIPS simulator 
    * uses GCC to compile into object files and linking them into final output. Start.o ensures correct startup routine

* Variables explanation
    * CC, AS, LD: compiler, assembler, and linker commands
    * CFLAGS: including include directories and options for compiling
    * PROGRAMS: specifies the names of the programs to be built

### How files are built
For example, this is the code block for halt
```
halt.o: halt.c
	$(CC) $(CFLAGS) -c halt.c
halt: halt.o start.o
	$(LD) $(LDFLAGS) start.o halt.o -o halt.coff
	$(COFF2NOFF) halt.coff halt
```
1. halt.o is created by compiling halt.c.
2. halt.coff is created by linking halt.o and tart.o.
3. halt (the final executable) is created by converting halt.coff to NOFF format.


## Part II: Implementation for Syscall

From code tracing we followed how other system code work and modified these files:

* userprog
  * exception.cc
  * ksyscall.h
  * syscall.h
* filesys
  * filesys.h 

## implementation in exception.cc
Finish the code for SC_Open, SC_Write, SC_Read, SC_Close

For example, the code for `SC_Write`:
```
case SC_Write :
    val = kernel->machine->ReadRegister(4);
    numChar = kernel->machine->ReadRegister(5);
    fileID = kernel->machine->ReadRegister(6);
    {
        char *buffer = &(kernel->machine->mainMemory[val]);
        status = SysWrite(buffer, numChar, fileID);
        DEBUG(dbgSys, "SC_Write returning status " << status << "\n");
        kernel->machine->WriteRegister(2, (int)status);
    }
    kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
    kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
    kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
    return;
    ASSERTNOTREACHED();
    break;
```
We get the start of memory from register 4, how many bytes or char we need to write in register 5, and fileID from register 6. After having these values, we call the kernel using `Syswrite` for operation. Here, the `buffer` is the pointer to `mainMemory`. At the end, we advance Program Counter for one word.

The other example:
```
case SC_Read :
    // starting address for user program's memory
    val = kernel->machine->ReadRegister(4);
    // how many bytes should OS reads
    numChar = kernel->machine->ReadRegister(5);
    fileID = kernel->machine->ReadRegister(6);

    if (val < 0 || numChar < 0 || (val+numChar) > MemorySize) {
        kernel->machine->WriteRegister(2, -1); // return -1 for error
    } else {
        // buffer is address to mainmemory
        char *buffer = &(kernel->machine->mainMemory[val]);
        status = SysRead(buffer, numChar, fileID);
        DEBUG(dbgSys, "SC_Read returning status " << status << "\n");
        kernel->machine->WriteRegister(2, (int)status);
    }

    kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
    kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
    kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);

    return;
    ASSERTNOTREACHED();
    break;
```
Same, we get the needed values from register 4, 5, 6. Afterwards, we check some conditions such as is memory reading in the valid range. After that we invoke `SysRead` to let kernel do stuff. Then at the end we forward our PC.

* Other system call is implemented in similar ways.

## Implementation in ksyscall.h
this is the implementation
```

OpenFileId SysOpen(char *name)
{
    DEBUG(dbgSys, "In ksyscall.h:OpenFileId, into fileSystem->OpenAFile, " << kernel->stats->totalTicks);
    return kernel->fileSystem->OpenAFile(name);
    DEBUG(dbgTraCode, "return from ksyscall.h:OpenFileId, into fileSystem->OpenAFile, " << kernel->stats->totalTicks);
}

int SysWrite(char *buffer, int size, OpenFileId id)
{
	return kernel->fileSystem->WriteFile(buffer, size, id);
}

int SysRead(char *buffer, int size, OpenFileId id)
{
    return kernel->fileSystem->ReadFile(buffer, size, id);
}

int SysClose(OpenFileId id)
{
	return kernel->fileSystem->CloseFile(id);
}

```
We invoke kernel level functions to deal with system call. These function are all inside `fileSystem` class.

## Implementation of filesys.h

* Explanation of variables

`OpenFileTable` is where the `OpenFileId` are stored
```
FileSystem() {
    for (int i = 0; i < 20; i++) OpenFileTable[i] = NULL;
}
```

* Implementation for `OpenAFile`
```
 OpenFileId OpenAFile(char *name) {
        // find available slot in openFileTable
        for (OpenFileId id = 0; id < MAX_OPEN_FILES; id++) {
            if (OpenFileTable[id] == NULL) {
                int fileDescriptor = OpenForReadWrite(name, FALSE);
                if (fileDescriptor == -1) {
                    // can't open a file
                    return -1;
                }
                OpenFileTable[id] = new OpenFile(fileDescriptor);
                return id;
            }
        }
        // no availble slot
        return -1;
    }
```
Firstly, iterate through `openFileTable` and find a spot, if there's no spot we return -1. Else, we create a new File object and store inside `OpenFileTable`.

The `OpenForReadWrite` will return the state of file opening after opening a file. If we can't open a file then also return -1.

* Implementation for `WriteFile`
```
    int WriteFile(char *buffer, int size, OpenFileId id){
        if (id < 0 || id >= MAX_OPEN_FILES || OpenFileTable[id] == NULL) {
            // is not valid ID
            return -1;
        }
        int bytesWritten = OpenFileTable[id]->Write(buffer, size);

        if (bytesWritten < 0) {
            return -1;
        }
	    return bytesWritten;
    }
```
for writing file, we point to our OpenFileTable[id] and use `Write` to write data into the mainMemory. It will how many bytes have be written. If it's the value smaller than 0 it means write fail, so return -1.


* Implementation for `ReadFile`
```
    int ReadFile(char *buffer, int size, OpenFileId id){
        if (id < 0 || id >= MAX_OPEN_FILES || OpenFileTable[id] == NULL) {
            // is not valid ID
            return -1;
        }
        int bytesRead = OpenFileTable[id]->Read(buffer, size);
        if (bytesRead < 0) {
            return -1;
        }
        return bytesRead;
    }
```
Similar to `WriteFile` but instead using `Write` we `Read` from the main memory.

* Implementation for `CloseFile`
```
int CloseFile(OpenFileId id){
    if (id < 0 || id >= MAX_OPEN_FILES || OpenFileTable[id] == NULL) {
        // is not valid ID
        return -1;
    }
    delete OpenFileTable[id];
    OpenFileTable[id] = NULL;
    return 1; // delete successful
}
```
We simply release the File object and return 1 when delete is successful.

## Implementation of syscall.h
```
#define SC_Open	6
#define SC_Read	7
#define SC_Write	8
#define SC_Seek 9
#define SC_Close	10
```
Here, we uncomment the system call number so exception could be caught when system call is being raise when running MIPS instructions.

## Implementation of start.S
```

		.globl Open
		.ent Open
Open:
        addiu $2, $0, SC_Open
        syscall
        j       $31
        .end Open

        .globl Write
        .ent    Write
Write:
        addiu $2, $0, SC_Write
        syscall
        j       $31
        .end Write

        .globl Read
        .ent    Read
Read:
        addiu $2, $0, SC_Read
        syscall
        j       $31
        .end Read

        .globl Close
        .ent    Close
Close:
        addiu $2, $0, SC_Close
        syscall
        j       $31
        .end Close
```
In start.S we write these functions in assembly allows precise manipulation of CPU registers, ensuring that system call numbers and arguments are correctly positioned for the kernel to interpret.

##  What difficulties did you encounter when implementing this assignment?
1. Didn't know which files to modify. Especially I haven't done anything related to system before, so it took me a lot of time to understand each components.

2. Code tracing is not easy to understand and we didn't learn most of the stuff file system in class yet. Therefore, I think that's why I have trouble understanding system call mechanisms.


## Any feedback you would like to let us know.
1. Thank you for preparing this homework TA and Prof. Chou.
2. More hints next time please.









