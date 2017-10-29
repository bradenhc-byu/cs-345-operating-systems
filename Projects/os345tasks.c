// os345tasks.c - OS create/kill task	08/08/2013
// ***********************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the BYU CS345 projects.      **
// ** It comes "as is" and "unwarranted."  As such, when you use part   **
// ** or all of the code, it becomes "yours" and you are responsible to **
// ** understand any algorithm or method presented.  Likewise, any      **
// ** errors or problems become your responsibility to fix.             **
// **                                                                   **
// ** NOTES:                                                            **
// ** -Comments beginning with "// ??" may require some implementation. **
// ** -Tab stops are set at every 3 spaces.                             **
// ** -The function API's in "OS345.h" should not be altered.           **
// **                                                                   **
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// ***********************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <time.h>
#include <assert.h>

#include "os345.h"
#include "os345signals.h"
#include "os345lc3.h"
//#include "os345config.h"


extern TCB tcb[];							// task control block
extern int curTask;							// current task #
extern PQueue* rq;							// the ready queue

extern int superMode;						// system mode
extern Semaphore* semaphoreList;			// linked list of active semaphores
extern Semaphore* taskSems[MAX_TASKS];		// task semaphore

int enQ(PQueue*,TID,int);
int deQ(PQueue*,TID);



// **********************************************************************
// **********************************************************************
// create task
int createTask(char* name,						// task name
					int (*task)(int, char**),	// task address
					int priority,				// task priority
					int argc,					// task argument count
					char* argv[])				// task argument pointers
{
	int tid;
	// find an open tcb entry slot
	for (tid = 0; tid < MAX_TASKS; tid++)
	{
		if (tcb[tid].name == 0)
		{
			char buf[8];

			// create task semaphore
			if (taskSems[tid]) deleteSemaphore(&taskSems[tid]);
			sprintf(buf, "task%d", tid);
			taskSems[tid] = createSemaphore(buf, 0, 0);
			taskSems[tid]->taskNum = 0;	// assign to shell

			// copy task name
			tcb[tid].name = (char*)malloc(strlen(name)+1);
			strcpy(tcb[tid].name, name);

			// set task address and other parameters
			tcb[tid].task = task;			// task address
			tcb[tid].state = S_NEW;			// NEW task state
			tcb[tid].priority = priority;	// task priority
			tcb[tid].parent = curTask;		// parent
			tcb[tid].argc = argc;			// argument count

			// ?? malloc new argv parameters  vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
			int i;
			tcb[tid].argv = malloc(sizeof(char*) * argc);
			for(i = 0; i < argc; i++){
				tcb[tid].argv[i] = malloc(sizeof(char) * strlen(argv[i]));
				strcpy(tcb[tid].argv[i], argv[i]); 
			}

			tcb[tid].event = 0;				// suspend semaphore
			// give the task a unique root page table
			tcb[tid].RPT = LC3_RPT + ((tid) ? ((tid-1)<<6) : 0);
			tcb[tid].tickets = 0;			// initialize its ticket count
			tcb[tid].cdir = CDIR;			// inherit parent cDir (project 6)

			// ?? ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

			// define task signals
			createTaskSigHandlers(tid);

			// Each task must have its own stack and stack pointer.
			tcb[tid].stack = malloc(STACK_SIZE * sizeof(int));

			// insert task into "ready" queue
			enQ(rq, tid, tcb[tid].priority);

			if (tid) swapTask();				// do context switch (if not cli)
			return tid;							// return tcb index (curTask)
		}
	}
	// tcb full!
	return -1;
} // end createTask


// *********************************************************************
// *********************************************************************
// Enqueue task into the ready queue
//
int enQ(PQueue* pq, TID tid, int priority){
	
	if(pq->size == MAX_TASKS) return -1;	// if there is not room, don't add the task	
	PQTASK pqt;				// create a new priority queue task
	pqt.tid = tid;
	pqt.priority = priority;
	// push the task onto the queue
	int i;
	for(i = pq->size; i >= 0; i--){
		if(i == 0 || pq->q[i-1].priority < priority){
			pq->q[i] = pqt;
			break;
		}else{
			pq->q[i] = pq->q[i-1];
			pq->q[i-1] = emptyTask;
		}
	}
	pq->size++;
	
	return tid;
}

// *********************************************************************
// *********************************************************************
// Dequeue task from a priority queue
//
int deQ(PQueue* pq, TID tid){
	// tid = -1, get the highest priority task from the ready queue
	if(tid == -1 && pq->size != 0){				// if there is a task in the ready queue
		int rTID = pq->q[pq->size-1].tid;	// save the TID of the highest priority task
		pq->q[pq->size-1] = emptyTask;		// remove the task from the ready queue
		pq->size--;							// decrement the size of the queue
		return rTID;						// return the TID of the highest priority task
	}
	// tid != -1, get the task with the given tid from the ready queue if there are 
	// tasks in the ready queue
	else if (pq->size != 0){
		int i;
		bool found = FALSE;
		for(i = 0; i < pq->size; i++){
			if(pq->q[i].tid == tid){
				found = TRUE;					// set the found flag 
				pq->q[i] = emptyTask;			// clear the task from the queue
			}else if(found){
				pq->q[i-1] = pq->q[i];			// if the task was found, move the  
				pq->q[i] = emptyTask;			// current task to fill the hole in the queue
			}
		}
		if(found){
			pq->size--;						// decrement the size of the queue
			return tid;						// the task was found, return the tid
		} 
	}
	return -1;					// the task was not found
}


// **********************************************************************
// **********************************************************************
// kill task
//
//	taskId == -1 => kill all non-shell tasks
//
static void exitTask(int taskId);
int killTask(int taskId)
{
	if (taskId != 0)			// don't terminate shell
	{
		if (taskId < 0)			// kill all tasks
		{
			int tid;
			for (tid = 1; tid < MAX_TASKS; tid++)
			{
				if (tcb[tid].name) exitTask(tid);
			}
		}
		else
		{
			// terminate individual task
			if (!tcb[taskId].name) return 1;
			exitTask(taskId);	// kill individual task
		}
	}
	if (!superMode) SWAP;
	return 0;
} // end killTask

static void exitTask(int taskId)
{
	assert("exitTaskError" && tcb[taskId].name);

	// 1. find task in system queue
	// 2. if blocked, unblock (handle semaphore)
	// 3. set state to exit

	// ?? add code here
	

	tcb[taskId].state = S_EXIT;			// EXIT task state
	return;
} // end exitTask



// **********************************************************************
// system kill task
//
int sysKillTask(int taskId)
{
	Semaphore* sem = semaphoreList;
	Semaphore** semLink = &semaphoreList;

	// assert that you are not pulling the rug out from under yourself!
	assert("sysKillTask Error" && tcb[taskId].name && superMode);
	printf("\nKill Task %s\n>>", tcb[taskId].name);

	// signal task terminated
	semSignal(taskSems[taskId]);

	// look for any semaphores created by this task
	while(sem = *semLink)
	{
		if(sem->taskNum == taskId)
		{
			// semaphore found, delete from list, release memory
			deleteSemaphore(semLink);
		}
		else
		{
			// move to next semaphore
			semLink = (Semaphore**)&sem->semLink;
		}
	}

	// ?? delete task from system queues
	deQ(rq,taskId);

	// free malloc'd argv variables
	int i;
	for(i = 0; i < tcb[taskId].argc; i++){
		free(tcb[taskId].argv[i]);
	}
	free(tcb[taskId].argv);
	tcb[taskId].argc = 0;

	tcb[taskId].name = 0;			// release tcb slot
	return 0;
} // end sysKillTask