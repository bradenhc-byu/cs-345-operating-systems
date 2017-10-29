// os345interrupts.c - pollInterrupts	08/08/2013
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
#include "os345config.h"
#include "os345signals.h"

// **********************************************************************
//	local prototypes
//
void pollInterrupts(void);
static void keyboard_isr(void);
static void timer_isr(void);


extern void updateDeltaClock(int);

// **********************************************************************
// **********************************************************************
// global semaphores

extern Semaphore* keyboard;				// keyboard semaphore
extern Semaphore* charReady;				// character has been entered
extern Semaphore* inBufferReady;			// input buffer ready semaphore

extern Semaphore* tics1sec;				// 1 second semaphore
extern Semaphore* tics10thsec;				// 1/10 second semaphore
extern Semaphore* tics10sec;

extern char inChar;				// last entered character
extern int charFlag;				// 0 => buffered input
extern int inBufIndx;				// input pointer into input buffer
extern char inBuffer[INBUF_SIZE+1];	// character input buffer

extern time_t oldTime1;					// old 1sec time
extern time_t oldTime10;				// old 10sec time
extern clock_t myClkTime;
extern clock_t myOldClkTime;

extern int pollClock;				// current clock()
extern int lastPollClock;			// last pollClock

extern int superMode;						// system mode

extern int deltaTic;				// the number of tics passed (delta clock)


extern char* previousCommands[INBUF_SIZE+1];		// keeps track of previous commands
extern int previousCommandIndex;					// index into the previous commands array
extern int commandListSize;							// the size of the command list


// **********************************************************************
// **********************************************************************
// simulate asynchronous interrupts by polling events during idle loop
//
void pollInterrupts(void)
{
	// check for task monopoly
	pollClock = clock();
	assert("Timeout" && ((pollClock - lastPollClock) < MAX_CYCLES));
	lastPollClock = pollClock;

	// check for keyboard interrupt
	if ((inChar = GET_CHAR) > 0)
	{
	  keyboard_isr();
	}

	// timer interrupt
	timer_isr();

	return;
} // end pollInterrupts


// **********************************************************************
// keyboard interrupt service routine
//
static void keyboard_isr()
{
	// assert system mode
	assert("keyboard_isr Error" && superMode);

	semSignal(charReady);					// SIGNAL(charReady) (No Swap)
	if (charFlag == 0)
	{
		switch (inChar)
		{

			case '\r':
			case '\n':
			{
				inBufIndx = 0;				// EOL, signal line ready
				/*if(commandListSize == NUM_COMMANDS){
					previousCommands[0] = '\0';
					for(int i = 0; i < NUM_COMMANDS - 1; i++){
						previousCommands[i] = previousCommands[i + 1];
					}
					strcpy(previousCommands[commandListSize - 1], inBuffer);
				}else{
					
					strcpy(previousCommands[commandListSize], inBuffer);
					if(commandListSize != 0) previousCommandIndex++;
					commandListSize++;
				}*/
				semSignal(inBufferReady);	// SIGNAL(inBufferReady)
				break;
			}

			case 127:						// backspace
			{
				
				
				if(inBufIndx > 0){
					inBufIndx--;
					printf("\b \b");				
				}
				inBuffer[inBufIndx] = 0;
				break;
			}

			/*case 38:					// up arrow
			{
				strcpy(inBuffer, previousCommands[previousCommandIndex]);
				if(previousCommandIndex - 1 >= 0) previousCommandIndex--;
			}

			case 40:					// down arrow
			{
				if(previousCommandIndex + 1 < commandListSize) previousCommandIndex++;
				strcpy(inBuffer, previousCommands[previousCommandIndex]);
				
			}*/

			case 0x12:						// ^R
			{
				sigSignal(-1, mySIGCONT);				// interrupt all tasks with mySIGCONT
				int tid;
				extern TCB tcb[];
				for(tid = 0; tid < MAX_TASKS; tid++){	//Clear mySIGSTOP and mySIGTSTP from signal
					tcb[tid].signal &= ~mySIGSTOP;
					tcb[tid].signal &= ~mySIGTSTP;
				}
				break;
			}

			case 0x17:						// ^W
			{
				sigSignal(-1, mySIGTSTP);	// interrupt all tasks with mySIGTSTP
				break;
			}

			case 0x18:						// ^x
			{
				sigSignal(0, mySIGINT);		// interrupt task 0
				semSignal(inBufferReady);	// SEM_SIGNAL(inBufferReady)
				break;
			}

			default:
			{
				if(inBufIndx + 1 != MAX_STRING_SIZE){		//prevent buffer overflow
					inBuffer[inBufIndx++] = inChar;
					inBuffer[inBufIndx] = 0;
					printf("%c", inChar);		// echo character
				}
			}
		}
	}
	else
	{
		// single character mode
		inBufIndx = 0;
		inBuffer[inBufIndx] = 0;
	}
	return;
} // end keyboard_isr


// **********************************************************************
// timer interrupt service routine
//
static void timer_isr()
{
	time_t currentTime;						// current time

	// assert system mode
	assert("timer_isr Error" && superMode);

	// capture current time
  	time(&currentTime);

	// check the ten second time
	if((currentTime - oldTime10) >= 10){
		// signal 10 second
		semSignal(tics10sec);
		oldTime10 += 10;
	}

  	// one second timer
  	if ((currentTime - oldTime1) >= 1)
  	{
		// signal 1 second
  	   semSignal(tics1sec);
		oldTime1 += 1;
  	}

	// sample fine clock
	myClkTime = clock();
	if ((myClkTime - myOldClkTime) >= ONE_TENTH_SEC)
	{
		myOldClkTime = myOldClkTime + ONE_TENTH_SEC;   // update old
		semSignal(tics10thsec);
		deltaTic++;							// increment the number of tics passed
		updateDeltaClock(deltaTic);					// update the delta clock
	}


	return;
} // end timer_isr