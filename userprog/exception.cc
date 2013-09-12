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
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "console.h"
#include "synch.h"

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
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------
static Semaphore *readAvail;
static Semaphore *writeDone;
static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
    int memval, vaddr, printval, tempval, exp;

    // New changes
    if (!initializedConsoleSemaphores) {
        readAvail = new Semaphore("read avail", 0);
        writeDone = new Semaphore("write done", 1);
        initializedConsoleSemaphores = true;
    }

    Console *console = new Console(NULL, NULL, ReadAvail, WriteDone, 0);;

    if ((which == SyscallException) && (type == SC_Halt)) {
	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();
    }
    else if ((which == SyscallException) && (type == SC_PrintInt)) {
       printval = machine->ReadRegister(4);
       if (printval == 0) {
          writeDone->P() ;
          console->PutChar('0');
       }
       else {
          if (printval < 0) {
              writeDone->P() ;
             console->PutChar('-');
             printval = -printval;
          }
          tempval = printval;
          exp=1;
          while (tempval != 0) {
             tempval = tempval/10;
             exp = exp*10;
          }
          exp = exp/10;
          while (exp > 0) {
             writeDone->P() ;
             console->PutChar('0'+(printval/exp));
             printval = printval % exp;
             exp = exp/10;
          }
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SC_PrintChar)) {
         writeDone->P() ;
        console->PutChar(machine->ReadRegister(4));   // echo it!
        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SC_PrintString)) {
       vaddr = machine->ReadRegister(4);
       machine->ReadMem(vaddr, 1, &memval);
       while ((*(char*)&memval) != '\0') {
            writeDone->P() ;        // wait for write to finish
          console->PutChar(*(char*)&memval);
          vaddr++;
          machine->ReadMem(vaddr, 1, &memval);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    } 
    else if ((which == SyscallException) && (type == SC_GetReg)) {
       int reg = (int) machine->ReadRegister(4);
       reg = (int) machine->ReadRegister(reg);
       machine->WriteRegister(2, reg);

       // Advance program counters
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SC_GetPA)) {
       int virtAddress = machine->ReadRegister(4);
       unsigned vpn = (unsigned) virtAddress/PageSize;

       // Checking conditions
       if(vpn > machine->pageTableSize) {
           virtAddress = -1;
       } else if (!machine->pageTable[vpn].valid) {
           virtAddress = -1;
       } else if (machine->pageTable[vpn].physicalPage > NumPhysPages ){
           virtAddress = -1;
       } 
       machine->WriteRegister(2, virtAddress);

       // Advance program counters
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    } 
    else if ((which == SyscallException) && (type == SC_GetPID)) {
       machine->WriteRegister(2, currentThread->getPid());

       // Advance program counters
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    } 
    else if ((which == SyscallException) && (type == SC_GetPPID)) {
       machine->WriteRegister(2, currentThread->getPpid());

       // Advance program counters
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    } 
    else if ((which == SyscallException) && (type == SC_Time)) {
        // this is a simple system call, just acces the global variable
        // totalTicks
        machine->WriteRegister(2, stats->totalTicks);

        // Advance program counters
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    } 
    else if ((which == SyscallException) && (type == SC_Yield)) {
        // Increase the program counter before yielding
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);

        // Call yield method on the current thread
        currentThread->Yield();
    } 
    else if ((which == SyscallException) && (type == SC_Sleep)) {
        // Increase the program counter before sleeping 
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
        
        // The time in number of ticks
        int time = machine->ReadRegister(4);

        // Yield if time is zero, or else insert the element in a sorted fashion
        // into the timerQueue
        if (time == 0) {
            IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
            currentThread->Yield();
            (void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
        } else {
            ListElement *waitingThread = new ListElement(currentThread,  stats->totalTicks + time);
            timerQueue->SortedInsert((void *)waitingThread, stats->totalTicks + time);  
            // Sleep the current Process
            IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
            currentThread->Sleep();
            (void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
        }
    } 
    else if ((which == SyscallException) && (type == SC_Fork)) {
        // Increase the program counter before sleeping 
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
        
        // create a new kernel thread
        Thread *child = new Thread("forked thread");
        
        // Copy the address space of the currentThread into the child thread
        // child->space = currentThread->space;
        child->space = new AddrSpace(currentThread->space->getNumPages(), currentThread->space->getStartPhysPage()); 

        // Change the return address register to zero and save state
        machine->WriteRegister(2, 0);
        child->SaveUserState();
        
        // Setting the return value of the parent thread
        machine->WriteRegister(2, child->getPid());

        // Allocate the stack 
        child->StackAllocate(&forkStart, 0);

        // The child is now ready to run
        IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
        scheduler->ReadyToRun(child);
        (void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
    }
    else if ((which == SyscallException) && (type == SC_Exec)) {
        // We are in the kernel space, we have to copy the name of the file 
        // by translating using ReadMem
        char filename[100];
        int i=0;

        vaddr = machine->ReadRegister(4);
        machine->ReadMem(vaddr, 1, &memval);
        while ((*(char*)&memval) != '\0') {
            filename[i]  = (char)memval;
            ++i;
            vaddr++;
            machine->ReadMem(vaddr, 1, &memval);
        }
        filename[i]  = (char)memval;

        // The above is a direct copy of StartProcess, I didn't want to change
        // its scope so it has been included here
        OpenFile *executable = fileSystem->Open(filename);
        AddrSpace *space;

        if (executable == NULL) {
            printf("Unable to open file %s\n", filename);
            return;
        }
        space = new AddrSpace(executable);    
        currentThread->space = space;

        delete executable;			// close file

        space->InitRegisters();		// set the initial register values
        space->RestoreState();		// load page table register

        machine->Run();			// jump to the user progam
        ASSERT(FALSE);			// machine->Run never returns;
        // the address space exits
        // by doing the syscall "exit"
    }
    else {
        printf("Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }
}
