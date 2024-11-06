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

## 1-1. threads/thread.cc

## Thread::Sleep()
## Thread::StackAllocate()
## Thread::Finish()
## Thread::Fork()
## 1-2. userprog/addrspace.cc
## AddrSpace::AddrSpace()
## AddrSpace::Execute()
## AddrSpace::Load()
## 1-3. threads/kernel.cc
## Kernel::Kernel()
## Kernel::ExecAll()
## Kernel::Exec()
## Kernel::ForkExecute()
## 1-4. threads/scheduler.cc
## Scheduler::ReadyToRun()
## Scheduler::Run()