// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.
#define MAX_OPEN_FILES 20


#include "copyright.h"
#include "ksyscall.h"
#include "main.h"
#include "syscall.h"
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// If you are handling a system call, don't forget to increment the pc
// before returning. (Or else you'll loop making the same system call forever!)
//
//	"which" is the kind of exception.  The list of possible exceptions
//	is in machine.h.
//----------------------------------------------------------------------
void ExceptionHandler(ExceptionType which) {
    char ch;
    int val;
    int type = kernel->machine->ReadRegister(2);// 讀取系統調用類型
    int status, exit, threadID, programID, fileID, numChar;
    DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");
    DEBUG(dbgTraCode, "In ExceptionHandler(), Received Exception " << which << " type: " << type << ", " << kernel->stats->totalTicks);
    switch (which) {
        case MemoryLimitException:
            cerr << "Memory limit exceeded. Unable to allocate enough memory for the program.\n";
            kernel->interrupt->Halt();
            break;
        case SyscallException:
            switch (type) {
                case SC_Halt:
                    DEBUG(dbgSys, "Shutdown, initiated by user program.\n");
                    SysHalt();
                    cout << "in exception\n";
                    ASSERTNOTREACHED();
                    break;

                case SC_Open:// 開檔案的系統調用，讀取檔名並嘗試開啟檔案
                {
                    DEBUG(dbgSys, "Open system call.\n");
                    int filenameAddr = kernel->machine->ReadRegister(4);
                    
                    char filename[256]; // Assumes file names won't be longer than 128 characters
                    bool success = true;
                    // Copy the file name from user memory to kernel space
                    for (int i = 0; i < 255; i++) {
                        int temp;
                        if (!kernel->machine->ReadMem(filenameAddr + i, 1, &temp)) {
                            success = false;
                            break;
                        }
                        filename[i] = (char)temp;
                        if (filename[i] == '\0') break; // EOF
                    }
                    
                    filename[255] = '\0';
                    int size = 5;  // Size of the array

                    DEBUG(dbgSys, "Opening file: " << filename << "\n");

                    if (!success) {
                        kernel->machine->WriteRegister(2, -1);  // Return -1 for failure
                        break;
                    }

                    fileID = SysOpen(filename);
                    DEBUG(dbgSys, "Opening fileID: " << fileID << "\n");
                    kernel->machine->WriteRegister(2, (int)fileID);

                    kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                    kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                    kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);

                    return;
                    
                }
                
                case SC_Close:
                {
                    fileID = kernel->machine->ReadRegister(4);

                    if (fileID >= 0 && fileID < MAX_OPEN_FILES && kernel->fileSystem->OpenFileTable[fileID] != NULL) {
                        delete kernel->fileSystem->OpenFileTable[fileID]; // Close the file
                        kernel->fileSystem->OpenFileTable[fileID] = NULL; // Free the slot
                        kernel->machine->WriteRegister(2, 1); // Success
                    } else {
                        kernel->machine->WriteRegister(2, -1); // Failure
                    }

                    // Increment the Program Counter
                    kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                    kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                    kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);

                    return;
                }
                case SC_Write:
                {
                    int bufferAddr = kernel->machine->ReadRegister(4);
                    int size = kernel->machine->ReadRegister(5);
                    fileID = kernel->machine->ReadRegister(6);

                    if (fileID >= 0 && fileID < MAX_OPEN_FILES && kernel->fileSystem->OpenFileTable[fileID] != NULL) {
                        char *buffer = new char[size];
                        
                        // Copy data from user space to kernel space
                        for (int i = 0; i < size; i++) {
                            int temp;
                            kernel->machine->ReadMem(bufferAddr + i, 1, &temp);
                            buffer[i] = (char)temp;
                        }

                        int bytesWritten = kernel->fileSystem->OpenFileTable[fileID]->Write(buffer, size);
                        kernel->machine->WriteRegister(2, bytesWritten);  // Return the number of bytes written

                        delete[] buffer;
                    } else {
                        kernel->machine->WriteRegister(2, -1); // Error
                    }


                    kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                    kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                    kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);

                    return;
                }
                case SC_Read: {
                    DEBUG(dbgSys, "Read system call.\n");

                    // Read arguments from registers
                    int bufferAddr = kernel->machine->ReadRegister(4);  // Buffer address
                    int size = kernel->machine->ReadRegister(5);        // Number of bytes to read
                    fileID = kernel->machine->ReadRegister(6);      // OpenFileId

                    if (fileID >= 0 && fileID < MAX_OPEN_FILES && kernel->fileSystem->OpenFileTable[fileID] != NULL) {
                        OpenFile *file = kernel->fileSystem->OpenFileTable[fileID];  // Get the open file

                        // Allocate a temporary buffer to store the data
                        char *buffer = new char[size];

                        // Perform the read operation from the file
                        int bytesRead = file->Read(buffer, size);
                        // Copy the data from kernel space to user space
                        for (int i = 0; i < bytesRead; i++) {
                            kernel->machine->WriteMem(bufferAddr + i, 1, buffer[i]);
                        }

                        // Return the number of bytes read
                        kernel->machine->WriteRegister(2, bytesRead);

                        // Free the temporary buffer
                        delete[] buffer;
                    } else {
                        // Return -1 if the file descriptor is invalid
                        kernel->machine->WriteRegister(2, -1);
                    }

                    // Increment the Program Counter to avoid re-executing the same system call
                    kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                    kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                    kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);

                    return;
                }
                
                case SC_PrintInt:
                    DEBUG(dbgSys, "Print Int\n");
                    val = kernel->machine->ReadRegister(4);
                    DEBUG(dbgTraCode, "In ExceptionHandler(), into SysPrintInt, " << kernel->stats->totalTicks);
                    SysPrintInt(val);
                    DEBUG(dbgTraCode, "In ExceptionHandler(), return from SysPrintInt, " << kernel->stats->totalTicks);
                    // Set Program Counter
                    kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));  // 將當前的程式計數器 (PC) 儲存為前一個 PC
                    kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);  // 將 PC 往前移動 4 位元組 (對應一條 MIPS 指令的長度)
                    kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);  // 更新 NextPC，設定為下一條指令的位置
                    return;
                    ASSERTNOTREACHED();  // 不應該執行到這裡，如果到達這行表示程式邏輯有錯誤
                    break;
                case SC_MSG:
                    DEBUG(dbgSys, "Message received.\n");
                    val = kernel->machine->ReadRegister(4);
                    {
                        char *msg = &(kernel->machine->mainMemory[val]);
                        cout << msg << endl;
                    }   
                    SysHalt();
                    ASSERTNOTREACHED();
                    break;
                case SC_Create:
                    val = kernel->machine->ReadRegister(4);
                    {
                        char *filename = &(kernel->machine->mainMemory[val]);
                        // cout << filename << endl;
                        status = SysCreate(filename);
                        kernel->machine->WriteRegister(2, (int)status);
                    }
                     kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg)); // 記錄當前 PC
                     kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4); // 將 PC 更新到下一條指令
                     kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4); // 更新下一條指令的地址
                     return; // 結束該系統呼叫處理
                     ASSERTNOTREACHED(); // 確保不會到達這行代碼
                    break;
                case SC_Add:
                    DEBUG(dbgSys, "Add " << kernel->machine->ReadRegister(4) << " + " << kernel->machine->ReadRegister(5) << "\n");
                    /* Process SysAdd Systemcall*/
                    int result;
                    result = SysAdd(/* int op1 */ (int)kernel->machine->ReadRegister(4),
                                    /* int op2 */ (int)kernel->machine->ReadRegister(5));
                    DEBUG(dbgSys, "Add returning with " << result << "\n");
                    /* Prepare Result */
                    kernel->machine->WriteRegister(2, (int)result);
                    /* Modify return point */
                    {
                        /* set previous programm counter (debugging only)*/
                        kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

                        /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
                        kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);

                        /* set next programm counter for brach execution */
                        kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
                    }
                    cout << "result is " << result << "\n";
                    return;
                    ASSERTNOTREACHED();
                    break;
                case SC_Exit:
                    DEBUG(dbgAddr, "Program exit\n");
                    val = kernel->machine->ReadRegister(4);
                    cout << "return value:" << val << endl;
                    kernel->currentThread->Finish();
                    break;
                default:
                    cerr << "Unexpected system call " << type << "\n";
                    break;
            }
            break;
        default:
            cerr << "Unexpected user mode exception " << (int)which << "\n";
            break;
    }
    ASSERTNOTREACHED();
}
