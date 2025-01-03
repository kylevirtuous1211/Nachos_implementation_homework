// scheduler.h
//	Data structures for the thread dispatcher and scheduler.
//	Primarily, the list of threads that are ready to run.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "copyright.h"
#include "list.h"
#include "thread.h"

// The following class defines the scheduler/dispatcher abstraction --
// the data structures and operations needed to keep track of which
// thread is running, and which threads are ready but not running.

class Scheduler {
   public:
    Scheduler();   // Initialize list of ready threads
    ~Scheduler();  // De-allocate ready list

    void ReadyToRun(Thread* thread);
    // Thread can be dispatched.
    Thread* FindNextToRun();  // Dequeue first thread on the ready
                              // list, if any, and return thread.
    void Run(Thread* nextThread, bool finishing);
    // Cause nextThread to start running
    void CheckToBeDestroyed();  // Check if thread that had been
                                // running needs to be deleted
    void Print();               // Print contents of ready list

    void aging();

    bool shouldPreempt();

    // SelfTest for scheduler is implemented in class Thread
    class MultilevelThreadQueue {
      public:
         enum Level {
            Lv1, Lv2, Lv3
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
      Thread* toBeDestroyed;     // finishing thread to be destroyed
                               // by the next thread that runs
};

#endif  // SCHEDULER_H
