// synchconsole.cc
//	Routines providing synchronized access to the keyboard
//	and console display hardware devices.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "synchconsole.h"

#include "copyright.h"

//----------------------------------------------------------------------
// SynchConsoleInput::SynchConsoleInput
//      Initialize synchronized access to the keyboard
//
//      "inputFile" -- if NULL, use stdin as console device
//              otherwise, read from this file
//----------------------------------------------------------------------

SynchConsoleInput::SynchConsoleInput(char *inputFile) {
    consoleInput = new ConsoleInput(inputFile, this);
    lock = new Lock("console in");
    waitFor = new Semaphore("console in", 0);
}

//----------------------------------------------------------------------
// SynchConsoleInput::~SynchConsoleInput
//      Deallocate data structures for synchronized access to the keyboard
//----------------------------------------------------------------------

SynchConsoleInput::~SynchConsoleInput() {
    delete consoleInput;
    delete lock;
    delete waitFor;
}

//----------------------------------------------------------------------
// SynchConsoleInput::GetChar
//      Read a character typed at the keyboard, waiting if necessary.
//----------------------------------------------------------------------

char SynchConsoleInput::GetChar() {
    char ch;

    lock->Acquire();
    waitFor->P();  // wait for EOF or a char to be available.
    ch = consoleInput->GetChar();
    lock->Release();
    return ch;
}

//----------------------------------------------------------------------
// SynchConsoleInput::CallBack
//      Interrupt handler called when keystroke is hit; wake up
//	anyone waiting.
//----------------------------------------------------------------------

void SynchConsoleInput::CallBack() {
    waitFor->V();
}

//----------------------------------------------------------------------
// SynchConsoleOutput::SynchConsoleOutput
//      Initialize synchronized access to the console display
//
//      "outputFile" -- if NULL, use stdout as console device
//              otherwise, read from this file
//----------------------------------------------------------------------

SynchConsoleOutput::SynchConsoleOutput(char *outputFile) {
    consoleOutput = new ConsoleOutput(outputFile, this);
    lock = new Lock("console out");
    waitFor = new Semaphore("console out", 0);
}

//----------------------------------------------------------------------
// SynchConsoleOutput::~SynchConsoleOutput
//      Deallocate data structures for synchronized access to the keyboard
//----------------------------------------------------------------------

SynchConsoleOutput::~SynchConsoleOutput() {
    delete consoleOutput;
    delete lock;
    delete waitFor;
}

//----------------------------------------------------------------------
// SynchConsoleOutput::PutChar
//      Write a character to the console display, waiting if necessary.
//----------------------------------------------------------------------

void SynchConsoleOutput::PutChar(char ch) {
    lock->Acquire();
    consoleOutput->PutChar(ch);
    waitFor->P();
    lock->Release();
}

void SynchConsoleOutput::PutInt(int value) {
    char str[15];    // 定義字元陣列，用來存放轉換後的字串
    int idx = 0;     // 初始化索引變數

    // sprintf將整數轉換成字元串，並將其存入str。最後加上'\n'和字串結束符'\0'
    sprintf(str, "%d\n\0", value);  

    lock->Acquire();  // 獲取鎖，保證同步，只有獲取鎖的執行緒可以進入臨界區
    do {
        DEBUG(dbgTraCode, "In SynchConsoleOutput::PutChar, into consoleOutput->PutChar, " << kernel->stats->totalTicks);
        
        consoleOutput->PutChar(str[idx]);  // 將字元串中的當前字元輸出到控制台

        DEBUG(dbgTraCode, "In SynchConsoleOutput::PutChar, return from consoleOutput->PutChar, " << kernel->stats->totalTicks);
        
        idx++;  // 索引增加，處理下一個字元
        DEBUG(dbgTraCode, "In SynchConsoleOutput::PutChar, into waitFor->P(), " << kernel->stats->totalTicks);
        
        waitFor->P();  // 讓執行緒等待，直到處理完字元輸出

        DEBUG(dbgTraCode, "In SynchConsoleOutput::PutChar, return form waitFor->P(), " << kernel->stats->totalTicks);
    } while (str[idx] != '\0');  // 持續處理字元，直到遇到字串結束符'\0'

    lock->Release();  // 釋放鎖，允許其他執行緒進入臨界區
}


//----------------------------------------------------------------------
// SynchConsoleOutput::CallBack
//      Interrupt handler called when it's safe to send the next
//	character can be sent to the display.
//----------------------------------------------------------------------

void SynchConsoleOutput::CallBack() {
    DEBUG(dbgTraCode, "In SynchConsoleOutput::CallBack(), " << kernel->stats->totalTicks);
    waitFor->V();
}
