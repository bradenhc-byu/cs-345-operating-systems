// os345semaphores.c - OS Semaphores
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


extern TCB tcb[];							// task control block
extern int curTask;							// current task #
extern PQueue* rq;							// the ready queue

extern int superMode;						// system mode
extern Semaphore* semaphoreList;			// linked list of active semaphores


extern int enQ(PQueue*,TID,int);
extern int deQ(PQueue*,TID);


// **********************************************************************
// **********************************************************************
// signal semaphore
//
//	if task blocked by semaphore, then clear semaphore and wakeup task
//	else signal semaphore
//
void semSignal(Semaphore* s)
{
	// assert there is a semaphore and it is a legal type
	assert("semSignal Error" && s && ((s->type == 0) || (s->type == 1)));

	int tid;
	// check semaphore type
	if (s->type == 0)
	{		
		// binary semaphore
		s->state = 1;							// signal the sem 
		if((tid = deQ(s->bq,-1)) < 0) return;	// if there is a task in the blocked
		s->state = 0;							//   queue, signal and run
		tcb[tid].event = 0;						// clear the event semaphore
		tcb[tid].state = S_READY;				// set the state to ready
		enQ(rq, tid, tcb[tid].priority);		// move to the ready queue
		//if(!superMode) swapTask();
		return;
	}
	else
	{
		// counting semaphore
		s->state++;							// increment the state
		if(s->state > 0) return;			// return if there is nothing in the queue
		
		tid = deQ(s->bq,-1);				// get the tid of the next task in the blocked queue
		tcb[tid].event = 0;					// clear the event semaphore
		tcb[tid].state = S_READY;			// set the task to ready
		enQ(rq, tid, tcb[tid].priority);	// queue the task in the ready queue
		//if(!superMode) swapTask();
		return;

	}
} // end semSignal



// **********************************************************************
// **********************************************************************
// wait on semaphore
//
//	if semaphore is signaled, return immediately
//	else block task
//
int semWait(Semaphore* s)
{
	assert("semWait Error" && s);												// assert semaphore
	assert("semWait Error" && ((s->type == 0) || (s->type == 1)));	// assert legal type
	assert("semWait Error" && !superMode);								// assert user mode

	// check semaphore type
	if (s->type == 0)
	{
		// binary semaphore
		// if state is zero, then block task

		if (s->state == 1){
			s->state = 0;				// sem was signaled, consume it 
			return 0;					// return without a block
		}else{
			tcb[curTask].event = s;								// block task
			tcb[curTask].state = S_BLOCKED;						// set blocked state
			enQ(s->bq,deQ(rq,curTask),tcb[curTask].priority);	// move task to blocked queue
			
			swapTask();						// reschedule the tasks
			return 1;
		}
	}
	else
	{
		// counting semaphore
		s->state--;					// consume the sem
		if(s->state >= 0) return 0;	// if resources are available, return

		int tid = deQ(rq,curTask);			// block the task if resources are not available
		tcb[tid].event = s;
		tcb[tid].state = S_BLOCKED;
		enQ(s->bq, tid, tcb[tid].priority);

		swapTask();
		return 1;
	}
} // end semWait



// **********************************************************************
// **********************************************************************
// try to wait on semaphore
//
//	if semaphore is signaled, return 1
//	else return 0
//
int semTryLock(Semaphore* s)
{
	assert("semTryLock Error" && s);										// assert semaphore
	assert("semTryLock Error" && ((s->type == 0) || (s->type == 1)));		// assert legal type
	assert("semTryLock Error" && !superMode);								// assert user mode

	// check semaphore type
	if (s->type == 0)
	{
		// binary semaphore
		// if state is zero, then block task

		if (s->state == 0)
		{
			return 0;
		}
		// state is non-zero (semaphore already signaled)
		s->state = 0;						// reset state, and don't block
		return 1;
	}
	else
	{
		// counting semaphore
		s->state--;						// decrement the state
		if(s->state >= 0) return 0;		// sem is available
		return 1;						// sem is blocked
	}
} // end semTryLock


// **********************************************************************
// **********************************************************************
// Create a new semaphore.
// Use heap memory (malloc) and link into semaphore list (Semaphores)
// 	name = semaphore name
//		type = binary (0), counting (1)
//		state = initial semaphore state
// Note: memory must be released when the OS exits.
//
Semaphore* createSemaphore(char* name, int type, int state)
{
	Semaphore* sem = semaphoreList;
	Semaphore** semLink = &semaphoreList;

	// assert semaphore is binary or counting
	assert("createSemaphore Error" && ((type == 0) || (type == 1)));	// assert type is validate

	// look for duplicate name
	while (sem)
	{
		if (!strcmp(sem->name, name))
		{
			printf("\nSemaphore %s already defined", sem->name);

			// ?? What should be done about duplicate semaphores ??
			// semaphore found - change to new state
			sem->type = type;					// 0=binary, 1=counting
			sem->state = state;				// initial semaphore state
			sem->taskNum = curTask;			// set parent task #
			return sem;
		}
		// move to next semaphore
		semLink = (Semaphore**)&sem->semLink;
		sem = (Semaphore*)sem->semLink;
	}

	// allocate memory for new semaphore
	sem = (Semaphore*)malloc(sizeof(Semaphore));

	// set semaphore values
	sem->name = (char*)malloc(strlen(name)+1);
	strcpy(sem->name, name);				// semaphore name
	sem->type = type;							// 0=binary, 1=counting
	sem->state = state;						// initial semaphore state
	sem->taskNum = curTask;					// set parent task #
	sem->bq = (PQueue*)malloc(sizeof(PQueue));
	sem->bq->q = (PQTASK*)malloc(MAX_TASKS * sizeof(PQTASK));
	sem->bq->q[0] = emptyTask;
	sem->bq->size = 0;

	// prepend to semaphore list
	sem->semLink = (struct semaphore*)semaphoreList;
	semaphoreList = sem;						// link into semaphore list
	return sem;									// return semaphore pointer
} // end createSemaphore



// **********************************************************************
// **********************************************************************
// Delete semaphore and free its resources
//
bool deleteSemaphore(Semaphore** semaphore)
{
	Semaphore* sem = semaphoreList;
	Semaphore** semLink = &semaphoreList;

	// assert there is a semaphore
	assert("deleteSemaphore Error" && *semaphore);

	// look for semaphore
	while(sem)
	{
		if (sem == *semaphore)
		{
			// semaphore found, delete from list, release memory
			*semLink = (Semaphore*)sem->semLink;

			// free the name array before freeing semaphore
			printf("\ndeleteSemaphore(%s)", sem->name);

			// move blocked queue tasks into ready queue in an exit state
			int i;
			for(i = 0; i < sem->bq->size; i++){
				if(tcb[sem->bq->q[i].tid].name) {
					tcb[sem->bq->q[i].tid].state = S_EXIT;
					enQ(rq,sem->bq->q[i].tid,sem->bq->q[i].priority);
				}	
			}

			// free all semaphore memory
			free(sem->name);
			free(sem->bq->q);
			free(sem->bq);
			free(sem);

			return TRUE;
		}
		// move to next semaphore
		semLink = (Semaphore**)&sem->semLink;
		sem = (Semaphore*)sem->semLink;
	}

	// could not delete
	return FALSE;
} // end deleteSemaphore