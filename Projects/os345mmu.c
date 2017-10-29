// os345mmu.c - LC-3 Memory Management Unit	03/12/2015
//
//		03/12/2015	added PAGE_GET_SIZE to accessPage()
//
// **************************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the CS345 projects.          **
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
#include <assert.h>
#include "os345.h"
#include "os345lc3.h"

// ***********************************************************************
// mmu variables

// LC-3 memory
unsigned short int memory[LC3_MAX_MEMORY];

// statistics
int memAccess;						// memory accesses
int memHits;						// memory hits
int memPageFaults;					// memory faults
int rptClock;						// clock location in root page table (big hand)
int uptClock;						// clock location in user page table (little hand)

int getFrame(int);
int getClockFrame(int);
void swapOutFrame(int);
int getAvailableFrame(void);
extern TCB tcb[];					// task control block
extern int curTask;					// current task #

int getFrame(int notme)
{
	int frame;
	frame = getAvailableFrame();
	if (frame >=0) return frame;

	// run the clock
	frame = getClockFrame(notme);

	return frame;
}

// **************************************************************************
// **************************************************************************
// Clock function
// run the clock in memory and get the next available frame that isn't 
// equal to the notme frame
//
int getClockFrame(int notme){

	int frame;
	// printf("\nRun the clock!");

	// check all entries in the RPT (from 0x2400 to LC3_RTP_END)
	int wrapCount = 0;	// just to make sure we don't loop forever if it fails
	while(1){
		int rpte1;
		int upta, upte1;

		// the clock couldn't find any frames, so break and return -1 (error)
		if(wrapCount > 3) break;

		rpte1 = memory[rptClock];

		if(DEFINED(rpte1) && REFERENCED(rpte1)){
			// clear the reference and increment the clock
			memory[rptClock] = rpte1 = CLEAR_REF(rpte1);
		}else if(DEFINED(rpte1)){
			// the RPT entry hasn't been referenced, so check the UPT
			upta = (FRAME(rpte1)<<6);
			// look through the UPT
			for(; uptClock < 64; uptClock += 2){
				upte1 = memory[upta + uptClock];
				if(FRAME(upte1) == notme){
					// we need to skip over this entry and look for
					// the next available spot
					continue;
				}
				else if(DEFINED(upte1) && REFERENCED(upte1)){
					// clear the reference bit and increment the clock
					memory[upta + uptClock] = upte1 = CLEAR_REF(upte1);
				} else if(DEFINED(upte1)){
					// we can swap out and use the frame
					memory[rptClock] = rpte1 = SET_DIRTY(rpte1);
					frame = FRAME(upte1);				// get the frame address
					swapOutFrame(upta + uptClock);			// swap out the contents
					uptClock += 2;
					if(uptClock >= 64){
						rptClock = (rptClock + 2 >= LC3_RPT_END) ? LC3_RPT : (rptClock + 2);
						uptClock = 0;
					}
					return frame;
				}	
			}
			// we made it though the entire UPT 
			// we might be able to use the UPT frame reference
			bool defFlg = FALSE;
			int i;
			for(i = 0; i < 64; i += 2){
				upte1 = memory[upta + i];
				if(DEFINED(upte1)) defFlg = TRUE;
			}
			if(!defFlg && FRAME(rpte1) != notme){
				frame = FRAME(rpte1);
				swapOutFrame(rptClock);
				rptClock = ((rptClock + 2) >= LC3_RPT_END) ? LC3_RPT : (rptClock + 2);
				uptClock = 0;
				return frame;
			}
		}
		// not defined or not found in the UPT
		// set variables for each iteration (increment the clock)
		rptClock = ((rptClock + 2) >= LC3_RPT_END) ? LC3_RPT : (rptClock + 2);
		uptClock = 0;
		if(rptClock == LC3_RPT){
			wrapCount++;
		}
	}

	// the frame could not be found....
	return -1;
}


// **************************************************************************
// **************************************************************************
// Swap frame functions
// takes the frame and moves it into swap space
//
void swapOutFrame(int address){
	int entry1, entry2;
	int nextPage;

	entry1 = memory[address];
	entry2 = memory[address + 1];

	if(DIRTY(entry1) && PAGED(entry2)){
		// the dirty bit is not set and the swap exists
		accessPage(SWAPPAGE(entry2), FRAME(entry1), PAGE_OLD_WRITE);
	}else if (!PAGED(entry2)){
		// we need to write to a new swap space
		nextPage = accessPage(0, FRAME(entry1), PAGE_NEW_WRITE);
		entry2 = SET_PAGED(nextPage);
		memory[address + 1] = entry2;
	}
	// clear all bits at the address
	memory[address] = 0;
}



// **************************************************************************
// **************************************************************************
// LC3 Memory Management Unit
// Virtual Memory Process
// **************************************************************************
//           ___________________________________Frame defined
//          / __________________________________Dirty frame
//         / / _________________________________Referenced frame
//        / / / ________________________________Pinned in memory
//       / / / /     ___________________________
//      / / / /     /                 __________frame # (0-1023) (2^10)
//     / / / /     /                 / _________page defined
//    / / / /     /                 / /       __page # (0-4096) (2^12)
//   / / / /     /                 / /       /
//  / / / /     / 	             / /       /
// F D R P - - f f|f f f f f f f f|S - - - p p p p|p p p p p p p p

#define MMU_ENABLE	1

unsigned short int *getMemAdr(int va, int rwFlg)
{
	unsigned short int pa;
	int rpta, rpte1, rpte2;
	int upta, upte1, upte2;
	int rptFrame, uptFrame;

	memAccess += 3;
	// turn off virtual addressing for system RAM
	if (va < 0x3000){
		return &memory[va];
	} 

#if MMU_ENABLE

	rpta = tcb[curTask].RPT + RPTI(va);		// root page table address
	rpte1 = memory[rpta];					// FDRP__ffffffffff
	rpte2 = memory[rpta+1];					// S___pppppppppppp
	if (DEFINED(rpte1))	{ 							// rpte defined
		memHits++;										// increase the number of hits in main memory
	}					
	else{ 											// rpte undefined, fault
		memPageFaults++;								// increase page faults
		rptFrame = getFrame(-1);						// get a new UPT frame from memory 
		rpte1 = SET_DEFINED(rptFrame);	// define the frame
		//rpte1 = SET_FRAME(rpte1,rptFrame);
		if(PAGED(rpte2)){
			// we need to read the UPT in from swap space
			accessPage(SWAPPAGE(rpte2),rptFrame,PAGE_READ);
		}else{
			// we need to initialize the UPT 
			memset(&memory[(rptFrame<<6)], 0, 2 * LC3_FRAME_SIZE);
		}
	}
	memory[rpta] = rpte1 = SET_REF(rpte1);			// set rpt frame access bit
	memory[rpta+1] = rpte2;

	upta = (FRAME(rpte1)<<6) + UPTI(va);	// user page table address
	upte1 = memory[upta]; 					// FDRP__ffffffffff
	upte2 = memory[upta+1]; 				// S___pppppppppppp

	if (DEFINED(upte1)){ 								// upte defined
		memHits += 2;
	}						
	else{												// upte undefined
		memPageFaults++;									// increase the number of page faults
		uptFrame = getFrame(FRAME(rpte1));					// get the data frame from memory
		memory[rpta] = rpte1 = SET_REF(SET_DIRTY(rpte1));			// set the dirty bit on rpt entry
		upte1 = SET_DEFINED(uptFrame);						// define the data frame
		if(PAGED(upte2)){	
			// the data frame is in swap space, we need to read it 
			accessPage(SWAPPAGE(upte2), uptFrame, PAGE_READ);
		}else{
			// set the memory
			memset(&memory[(uptFrame<<6)], 0xf025, 2 * LC3_FRAME_SIZE);
		}
	 }
	 if(rwFlg){
		 upte1 = SET_DIRTY(upte1);
	 }					
	memory[upta] = upte1 = SET_REF(upte1); 			// set upt frame reference bit
	memory[upta+1] = upte2;
	pa = (FRAME(upte1)<<6) + FRAMEOFFSET(va);		// the physical address
	return &memory[pa];
#else
	return &memory[va];
#endif
} // end getMemAdr


// **************************************************************************
// **************************************************************************
// set frames available from sf to ef
//    flg = 0 -> clear all others
//        = 1 -> just add bits
//
void setFrameTableBits(int flg, int sf, int ef)
{	int i, data;
	int adr = LC3_FBT-1;             // index to frame bit table
	int fmask = 0x0001;              // bit mask

	// 1024 frames in LC-3 memory
	for (i=0; i<LC3_FRAMES; i++)
	{	if (fmask & 0x0001)
		{  fmask = 0x8000;
			adr++;
			data = (flg)?MEMWORD(adr):0;
		}
		else fmask = fmask >> 1;
		// allocate frame if in range
		if ( (i >= sf) && (i < ef)) data = data | fmask;
		MEMWORD(adr) = data;
	}
	return;
} // end setFrameTableBits


// **************************************************************************
// get frame from frame bit table (else return -1)
int getAvailableFrame()
{
	int i, data;
	int adr = LC3_FBT - 1;				// index to frame bit table
	int fmask = 0x0001;					// bit mask

	for (i=0; i<LC3_FRAMES; i++)		// look thru all frames
	{	if (fmask & 0x0001)
		{  fmask = 0x8000;				// move to next work
			adr++;
			data = MEMWORD(adr);
		}
		else fmask = fmask >> 1;		// next frame
		// deallocate frame and return frame #
		if (data & fmask)
		{  MEMWORD(adr) = data & ~fmask;
			return i;
		}
	}
	return -1;
} // end getAvailableFrame



// **************************************************************************
// read/write to swap space
int accessPage(int pnum, int frame, int rwnFlg)
{
	static int nextPage;						// swap page size
	static int pageReads;						// page reads
	static int pageWrites;						// page writes
	static unsigned short int swapMemory[LC3_MAX_SWAP_MEMORY];

	if ((nextPage >= LC3_MAX_PAGE) || (pnum >= LC3_MAX_PAGE))
	{
		printf("\nVirtual Memory Space Exceeded!  (%d)", LC3_MAX_PAGE);
		exit(-4);
	}
	switch(rwnFlg)
	{
		case PAGE_INIT:                    		// init paging
			memAccess = 0;						// memory accesses
			memHits = 0;						// memory hits
			memPageFaults = 0;					// memory faults
			nextPage = 0;						// disk swap space size
			pageReads = 0;						// disk page reads
			pageWrites = 0;						// disk page writes
			rptClock = LC3_RPT;					// initialize RPT clock (big hand)
			uptClock = 0;						// initialize UPT clock (little hand)
			return 0;

		case PAGE_GET_SIZE:                    	// return swap size
			return nextPage;

		case PAGE_GET_READS:                   	// return swap reads
			return pageReads;

		case PAGE_GET_WRITES:                    // return swap writes
			return pageWrites;

		case PAGE_GET_ADR:                    	// return page address
			return (int)(&swapMemory[pnum<<6]);

		case PAGE_NEW_WRITE:                   // new write (Drops thru to write old)
			pnum = nextPage++;

		case PAGE_OLD_WRITE:                   // write
			//printf("\n    (%d) Write frame %d (memory[%04x]) to page %d", p.PID, frame, frame<<6, pnum);
			memcpy(&swapMemory[pnum<<6], &memory[frame<<6], 1<<7);
			pageWrites++;
			return pnum;

		case PAGE_READ:                    	// read
			//printf("\n    (%d) Read page %d into frame %d (memory[%04x])", p.PID, pnum, frame, frame<<6);
			memcpy(&memory[frame<<6], &swapMemory[pnum<<6], 1<<7);
			pageReads++;
			return pnum;

		case PAGE_FREE:                   // free page
			// 0xf025 is the LC-3 halt instruction
			memset(&memory[(frame<<6)], 0xf025, 128);
			break;
   }
   return pnum;
} // end accessPage