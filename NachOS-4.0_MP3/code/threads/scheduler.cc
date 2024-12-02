// scheduler.cc
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would
//	end up calling FindNextToRun(), and that would put us in an
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "scheduler.h"

#include "copyright.h"
#include "debug.h"
#include "main.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler() {
    readyList = new SystemQueue();
    toBeDestroyed = NULL;
}

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler() {
    delete readyList;
}

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void Scheduler::ReadyToRun(Thread *thread) {
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
    // cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    readyList->Append(thread);
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun() {
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (readyList->IsEmpty()) {
        return NULL;
    } else {
        return readyList->RemoveFront();
    }
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void Scheduler::Run(Thread *nextThread, bool finishing) {
    Thread *oldThread = kernel->currentThread;

    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {  // mark that we need to delete current thread
        ASSERT(toBeDestroyed == NULL);
        toBeDestroyed = oldThread;
    }

    if (oldThread->space != NULL) {  // if this thread is a user program,
        oldThread->SaveUserState();  // save the user's CPU registers
        oldThread->space->SaveState();
    }

    oldThread->CheckOverflow();  // check if the old thread
                                 // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running

    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());

    // This is a machine-dependent assembly language routine defined
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread

    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();  // check if thread we were running
                           // before this one has finished
                           // and needs to be cleaned up

    if (oldThread->space != NULL) {     // if there is an address space
        oldThread->RestoreUserState();  // to restore, do it.
        oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void Scheduler::CheckToBeDestroyed() {
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
        toBeDestroyed = NULL;
    }
}

//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void Scheduler::Print() {
    cout << "Ready list contents:\n";
    readyList->Apply(ThreadPrint);
}

void Scheduler::aging() {
    readyList->Aging();
}

bool Scheduler::shouldPreempt() {
    return readyList->ShouldPreempt();
}

////////////////////system queue//////////////

Scheduler::SystemQueue::SystemQueue() :
    L1 (new SortedList<Thread*>(Thread::compareTime)),
    L2 (new SortedList<Thread*>(Thread::comparePriority)),
    L3 ( new List<Thread*>()) {}

Scheduler::SystemQueue::~SystemQueue() {
    delete L1;
    delete L2;
    delete L3;
}

void Scheduler::SystemQueue::Append(Thread * thread) {
    int level = getQueueLevel(thread);
    switch(level) {
        case Lv1:
            L1->Insert(thread);
            break;
        case Lv2:  
            L2->Insert(thread);
            break;
        case Lv3:
            L3->Append(thread);
            break;
    }
    DEBUG(dbgQueue, "[A] Tick [" 
        << kernel->stats->totalTicks << "]: Thread [" 
        << thread->getID() << "] is inserted into queue L[" 
        << level+1 << "]"); // print debug message
}

void Scheduler::SystemQueue::Apply(void (* callback)(Thread *)) {
    L1->Apply(callback);
    L2->Apply(callback);
    L3->Apply(callback);
}

void Scheduler::SystemQueue::Aging() {
    Apply(Thread::Aging);
    PromoteThreads(L3, Lv3);
    PromoteThreads(L2, Lv2);
}

bool Scheduler::SystemQueue::IsEmpty() {
    return L1->IsEmpty() && L2->IsEmpty() && L3->IsEmpty();
}

Thread * Scheduler::SystemQueue::RemoveFront() {
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

Scheduler::SystemQueue::Level Scheduler::SystemQueue::getQueueLevel(Thread * t) {
    int priority = t->getPriority();
    if (priority >= 100) {
        return Lv1;
    } else if (priority >= 50) {
        return Lv2;
    } else {
        return Lv3;
    }
}

void Scheduler::SystemQueue::PromoteThreads(List<Thread*> * fromQueue, Level level) {
    ListIterator<Thread*> it(fromQueue);

    while (!it.IsDone()) {
        Thread * t = it.Item();
        Level QL = getQueueLevel(t);
        if (QL < level) {
            fromQueue->Remove(t);
            DEBUG(dbgQueue, "[Aging] Tick [" 
              << kernel->stats->totalTicks << "]: Thread [" 
              << t->getID() << "] promoted from L[" 
              << level+1 << "] to L[" << QL+1 << "]");
            Append(t);
        } 
        it.Next();
    }
}

bool Scheduler::SystemQueue::ShouldPreempt() {
    Thread * t = kernel->currentThread;
    bool preempt = false;
    // round robin
    if (getQueueLevel(t) == Lv3)
        preempt = true;
    // if L1 is not empty
    // 
    else if (!L1->IsEmpty() && 
    (getQueueLevel(t) == Lv2 || Thread::compareTime(t, L1->Front()) > 0)) {
        preempt = true;
    }
    return preempt;
}
