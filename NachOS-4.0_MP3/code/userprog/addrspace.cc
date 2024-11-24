// addrspace.cc
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -n -T 0 option
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you are using the "stub" file system, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "addrspace.h"
#include <cstring>    // For memset

#include "copyright.h"
#include "machine.h"
#include "main.h"
#include "noff.h"

Bitmap available_frame(NumPhysPages);

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void
SwapHeader(NoffHeader *noffH) {
    noffH->noffMagic = WordToHost(noffH->noffMagic);
    noffH->code.size = WordToHost(noffH->code.size);
    noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
#ifdef RDATA
    noffH->readonlyData.size = WordToHost(noffH->readonlyData.size);
    noffH->readonlyData.virtualAddr =
        WordToHost(noffH->readonlyData.virtualAddr);
    noffH->readonlyData.inFileAddr =
        WordToHost(noffH->readonlyData.inFileAddr);
#endif
    noffH->initData.size = WordToHost(noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost(noffH->uninitData.size);
    noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);

#ifdef RDATA
    DEBUG(dbgAddr, "code = " << noffH->code.size << " readonly = " << noffH->readonlyData.size << " init = " << noffH->initData.size << " uninit = " << noffH->uninitData.size << "\n");
#endif
}


//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Set up the translation from program memory to physical
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//----------------------------------------------------------------------

AddrSpace::AddrSpace() : pageTable(NULL), numPages(0) {
    NULL;
    // pageTable = new TranslationEntry[NumPhysPages];
    // for (int i = 0; i < NumPhysPages; i++) {
    //     pageTable[i].virtualPage = i;  // for now, virt page # = phys page #
    //     pageTable[i].physicalPage = i;
    //     pageTable[i].valid = TRUE;
    //     pageTable[i].use = FALSE;
    //     pageTable[i].dirty = FALSE;
    //     pageTable[i].readOnly = FALSE;
    // }
    // // zero out the entire address space
    // bzero(kernel->machine->mainMemory, MemorySize);
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.
//----------------------------------------------------------------------

AddrSpace::~AddrSpace() {
    for (unsigned int i = 0; i < numPages; i++) {
        if (pageTable[i].valid) {
            available_frame.Clear(pageTable[i].physicalPage);
        }
    }
    delete[] pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::Load
// 	Load a user program into memory from a file.
//
//	Assumes that the page table has been initialized, and that
//	the object code file is in NOFF format.
//
//	"fileName" is the file containing the object code to load into memory
//----------------------------------------------------------------------

bool AddrSpace::Load(char *fileName) {
    OpenFile *executable = kernel->fileSystem->Open(fileName);
    NoffHeader noffH;
    unsigned int size;

    if (executable == NULL) {
        cerr << "Unable to open file " << fileName << "\n";
        return FALSE;
    }

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) &&
        (WordToHost(noffH.noffMagic) == NOFFMAGIC))
        SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

#ifdef RDATA
    size = noffH.code.size + noffH.readonlyData.size + noffH.initData.size +
           noffH.uninitData.size + UserStackSize;
#else
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size + UserStackSize;
#endif
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    // Remove the assertion to allow total virtual memory to exceed physical memory
    // ASSERT(numPages <= NumPhysPages);

    pageTable = new TranslationEntry[numPages];
    if (pageTable == NULL) {
        delete executable;
        return FALSE;
    }

    DEBUG(dbgAddr, "Initializing address space: "<< *fileName << " " << numPages << " pages, " << size << " bytes.");

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

bool AddrSpace::LoadSegments(OpenFile *executable, NoffHeader &noffH) {
    unsigned int currVPN = 0;

    // Load code segment
    if (noffH.code.size > 0) {
        if (!LoadSegment(executable, noffH.code, currVPN, FALSE)) {
            // **Handle failure by propagating FALSE**
            return FALSE;
        }
        currVPN += divRoundUp(noffH.code.size, PageSize);
    }

    // Load initialized data segment
    if (noffH.initData.size > 0) {
        if (!LoadSegment(executable, noffH.initData, currVPN, FALSE)) {
            // **Handle failure by propagating FALSE**
            return FALSE;
        }
        currVPN += divRoundUp(noffH.initData.size, PageSize);
    }

#ifdef RDATA
    // Load read-only data segment
    if (noffH.readonlyData.size > 0) {
        if (!LoadSegment(executable, noffH.readonlyData, currVPN, TRUE)) {
            // **Handle failure by propagating FALSE**
            return FALSE;
        }
        currVPN += divRoundUp(noffH.readonlyData.size, PageSize);
    }
#endif

    // **Load uninitialized data segment**
    if (noffH.uninitData.size > 0) {
        if (!LoadSegment(executable, noffH.uninitData, currVPN, FALSE)) {
            // **Handle failure by propagating FALSE**
            return FALSE;
        }
        currVPN += divRoundUp(noffH.uninitData.size, PageSize);
    }

    // **Allocate stack pages**
    unsigned int stackPages = divRoundUp(UserStackSize, PageSize);
    for (unsigned int i = 0; i < stackPages; i++) {
        unsigned int vpn = currVPN + i;
        int pageid = available_frame.FindAndSet();
        if (pageid == -1) {
            kernel->interrupt->setStatus(SystemMode);
            ExceptionHandler(MemoryLimitException);
            kernel->interrupt->setStatus(UserMode);
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
            
            // Switch to System Mode before handling the exception
            kernel->interrupt->setStatus(SystemMode);
            
            // Trigger MemoryLimitException (assuming MemoryLimitException is defined as exception code 8)
            ExceptionHandler(MemoryLimitException);
            
            // Switch back to User Mode after handling the exception
            kernel->interrupt->setStatus(UserMode);
            
            // **Important:** Return FALSE to propagate the failure up the call stack
            return FALSE;
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

//----------------------------------------------------------------------
// AddrSpace::Execute
// 	Run a user program using the current thread
//
//      The program is assumed to have already been loaded into
//      the address space
//
//----------------------------------------------------------------------

void AddrSpace::Execute(char *fileName) {
    kernel->currentThread->space = this;

    this->InitRegisters();  // set the initial register values
    this->RestoreState();   // load page table register

    kernel->machine->Run();  // jump to the user progam

    ASSERTNOTREACHED();  // machine->Run never returns;
                         // the address space exits
                         // by doing the syscall "exit"
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void AddrSpace::InitRegisters() {
    Machine *machine = kernel->machine;
    int i;

    for (i = 0; i < NumTotalRegs; i++)
        machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start", which
    //  is assumed to be virtual address zero
    machine->WriteRegister(PCReg, 0);

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    // Since instructions occupy four bytes each, the next instruction
    // after start will be at virtual address four.
    machine->WriteRegister(NextPCReg, 4);

    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we don't
    // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG(dbgAddr, "Initializing stack pointer: " << numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, don't need to save anything!
//----------------------------------------------------------------------

void AddrSpace::SaveState() {}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() {
    kernel->machine->pageTable = pageTable;
    kernel->machine->pageTableSize = numPages;
}

//----------------------------------------------------------------------
// AddrSpace::Translate
//  Translate the virtual address in _vaddr_ to a physical address
//  and store the physical address in _paddr_.
//  The flag _isReadWrite_ is false (0) for read-only access; true (1)
//  for read-write access.
//  Return any exceptions caused by the address translation.
//----------------------------------------------------------------------
ExceptionType
AddrSpace::Translate(unsigned int vaddr, unsigned int *paddr, int isReadWrite) {
    TranslationEntry *pte;
    int pfn;
    unsigned int vpn = vaddr / PageSize;
    unsigned int offset = vaddr % PageSize;

    if (vpn >= numPages) {
        return AddressErrorException;
    }

    pte = &pageTable[vpn];

    if (isReadWrite && pte->readOnly) {
        return ReadOnlyException;
    }

    pfn = pte->physicalPage;

    // if the pageFrame is too big, there is something really wrong!
    // An invalid translation was loaded into the page table or TLB.
    if (pfn >= NumPhysPages) {
        DEBUG(dbgAddr, "Illegal physical page " << pfn);
        return BusErrorException;
    }

    pte->use = TRUE;  // set the use, dirty bits

    if (isReadWrite)
        pte->dirty = TRUE;

    *paddr = pfn * PageSize + offset;

    ASSERT((*paddr < MemorySize));

    // cerr << " -- AddrSpace::Translate(): vaddr: " << vaddr <<
    //   ", paddr: " << *paddr << "\n";

    return NoException;
}
