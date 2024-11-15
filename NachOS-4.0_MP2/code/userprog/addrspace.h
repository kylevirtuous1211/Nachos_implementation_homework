// addrspace.h
//	Data structures to keep track of executing user programs
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"
#include "machine.h"
#include "bitmap.h"
#include <mutex>  // Include the mutex header

extern Bitmap available_frame;    // Extern declaration of the frame allocation bitmap
extern std::mutex frame_mutex;    // Extern declaration of the mutex for synchronizing frame access

#define UserStackSize 1024  // Increase this as necessary!

class AddrSpace {
   public:
    AddrSpace();   // Create an address space.
    ~AddrSpace();  // De-allocate an address space

    bool Load(char *fileName);  // Load a program into addr space from a file
                                // Return false if not found

    void Execute(char *fileName);  // Run a program
                                   // Assumes the program has already been loaded

    void SaveState();     // Save/restore address space-specific info on a context switch
    void RestoreState();

    bool LoadSegments(OpenFile *executable, NoffHeader &noffH);
    bool LoadSegment(OpenFile *executable, Segment &segment, unsigned int startVPN, bool readOnly);

    // Translate virtual address _vaddr_ to physical address _paddr_. _mode_ is 0 for Read, 1 for Write.
    ExceptionType Translate(unsigned int vaddr, unsigned int *paddr, int mode);

   private:
    TranslationEntry *pageTable;  // Linear page table translation for now
    unsigned int numPages;        // Number of pages in the virtual address space

    void InitRegisters();  // Initialize user-level CPU registers before jumping to user code
};

#endif  // ADDRSPACE_H
