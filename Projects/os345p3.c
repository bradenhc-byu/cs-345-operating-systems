// os345p3.c - Jurassic Park
// ***********************************************************************
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
#include <time.h>
#include <assert.h>
#include "os345.h"
#include "os345park.h"

// ***********************************************************************
// project 3 variables

// Jurassic Park
extern JPARK myPark;
extern Semaphore* parkMutex;					// protect park access
extern Semaphore* fillSeat[NUM_CARS];			// (signal) seat ready to fill
extern Semaphore* seatFilled[NUM_CARS];			// (wait) passenger seated
extern Semaphore* rideOver[NUM_CARS];			// (signal) ride over
extern DeltaClock* dc;
extern TCB tcb[];								// task control block
extern int deltaTic;							// the number of tics passed

int timeTaskID;
int globalDriverID;



// ***********************************************************************
// project 3 functions and tasks
void CL3_project3(int, char**);
void CL3_dc(int, char**);
void printDeltaClock(void);
void insertDeltaClock(int,Semaphore*);
void updateDeltaClock();
int carTask(int, char**);
int visitorTask(int, char**);
int driverTask(int, char**);

// semaphore handlers
void randomizeVisitor(int max, Semaphore* sem);
void moveToTicketLine();
void addVisitorToParkLine();
void reduceTicketsAvailable();
void waitForTicket(Semaphore* visitorWait);
void moveToMuseumLine();
void moveToMuseum();
void moveToTourLine();
void moveToTour();
void waitForMuseumExperience(Semaphore* visitorWait);
void waitForTourExperience(Semaphore* visitorWait);
void moveToGiftShopLine();
void waitForGiftShopExperience(Semaphore* visitorWait);
void moveToGiftShop();
void exitPark();
void moveVisitorToCar(int myId);
void moveVisitorOutOfCar(int myId);
void updateCarDriver(int driverID, int position);
Semaphore* getPassengerWaitSem();
Semaphore* getDriverWaitSem(int carID);
void passPassengerWaitSem(Semaphore* visitor);
void passDriverWaitSem(Semaphore* driver, int driverId);

// **********************************************************************
// project 3 semaphores
Semaphore* dcChange;
Semaphore* dcMutex;
Semaphore* tickets;
Semaphore* getPassenger;
Semaphore* seatTaken;
Semaphore* needDriver;
Semaphore* wakeupDriver;
Semaphore* roomInPark;
Semaphore* driverReady;
Semaphore* needTicket;
Semaphore* ticketReady;
Semaphore* buyTicket;
Semaphore* needPassengerWait;
Semaphore* passengerResourceReady;
Semaphore* driverResourceReady;
Semaphore* needDriverSem;
Semaphore* passengerResourceAcquired;
Semaphore* driverResourceAcquired;
Semaphore* resourceMutex;
Semaphore* getTicketMutex;
Semaphore* roomInMuseum;
Semaphore* roomInGiftShop;
Semaphore* getDriverMutex;

// global tracking semaphore
Semaphore* globalSemaphore;



// ***********************************************************************
// ***********************************************************************
// project3 command
int P3_project3(int argc, char* argv[])
{
	char buf[32];
	char idBuf[32];
	char* newArgv[2];
	int i;

	// start park
	sprintf(buf, "jurassicPark");
	newArgv[0] = buf;
	createTask( buf,				// task name
		jurassicTask,				// task
		MED_PRIORITY,				// task priority
		1,								// task count
		newArgv);					// task argument

	// initialize COUNTING semaphores
	tickets = createSemaphore("tickets", COUNTING, MAX_TICKETS);
    roomInPark = createSemaphore("roomInPark",COUNTING, MAX_IN_PARK);
    roomInMuseum = createSemaphore("roomInMuseum",COUNTING, MAX_IN_MUSEUM);
    roomInGiftShop = createSemaphore("roomInGiftShop",COUNTING, MAX_IN_GIFTSHOP);
    wakeupDriver = createSemaphore("wakeupDriver",COUNTING,0);

	// initialize mutex semaphores
    dcChange = createSemaphore("dcChange",BINARY,1);
    dcMutex = createSemaphore("dcMutex",BINARY,1);
    resourceMutex = createSemaphore("resourceMutex",BINARY,1);
    getTicketMutex = createSemaphore("getTicketMutex",BINARY,1);
    getDriverMutex = createSemaphore("getDriverMutex",BINARY,1);

	// initialize other BINARY (signal) semaphores
    getPassenger = createSemaphore("getPassenger",BINARY,0);
    seatTaken = createSemaphore("seatTaken",BINARY,0);
    needDriver = createSemaphore("needDriver",BINARY,0);
    driverReady = createSemaphore("driverReady",BINARY,0);
    needTicket = createSemaphore("needTicket",BINARY,0);
    ticketReady = createSemaphore("ticketReady",BINARY,0);
    buyTicket = createSemaphore("buyTicket",BINARY,0);
    needPassengerWait = createSemaphore("needPassengerWait",BINARY,0);
    passengerResourceReady = createSemaphore("passengerResourceReady",BINARY,0);
    driverResourceReady = createSemaphore("driverResourceReady",BINARY,0);
    needDriverSem = createSemaphore("needDriverSem",BINARY,0);
    passengerResourceAcquired = createSemaphore("passengerResourceAcquired",BINARY,0);
	driverResourceAcquired = createSemaphore("driverResourceAcquired",BINARY,0);

	// wait for park to get initialized...
	while (!parkMutex) SWAP;
	printf("\nStart Jurassic Park...");

	//?? create car, driver, and visitor tasks here (SWAP after every line)

	// car task
	for(i = 0; i < NUM_CARS; i++){
		sprintf(buf, "car%d",i);	SWAP;
		sprintf(idBuf, "%d", i);	SWAP;
		newArgv[0] = buf;			SWAP;
		newArgv[1] = idBuf;			SWAP;
		createTask(buf,	
				   carTask,
				   MED_PRIORITY,
				   2,
				   newArgv);		SWAP;
	}

	// driver task
	for(i = 0; i < NUM_DRIVERS; i++){
		sprintf(buf,"driver%d",i);		SWAP;
		sprintf(idBuf,"%d",i);			SWAP;
		newArgv[0] = buf;				SWAP;
		newArgv[1] = idBuf;				SWAP;
		createTask(buf,
				   driverTask,
				   MED_PRIORITY,
				   2,
				   newArgv);			SWAP;
	}

	// visitor task
	for(i = 0; i < NUM_VISITORS; i++){
		sprintf(buf,"visitor%d",i);		SWAP;
		sprintf(idBuf,"%d",i);			SWAP;
		newArgv[0] = buf;				SWAP;
		newArgv[1] = idBuf;				SWAP;
		createTask(buf,
				   visitorTask,
				   MED_PRIORITY,
				   2,
				   newArgv);			SWAP;
	}


	return 0;
} // end project3


// ***********************************************************************
// ***********************************************************************
// car task
//
int carTask(int argc, char* argv[]){

	int i;
	int carID = atoi(argv[1]);	SWAP;
	char endedMessage[32];	SWAP;
	sprintf(endedMessage,"%s Tour Over",argv[0]);	SWAP;
	Semaphore* passengerWait[NUM_SEATS];	SWAP;
	Semaphore* driverDone;	SWAP;
	printf("Starting %s Task",argv[0]);	SWAP;

	while(1){
		for(i = 0; i < NUM_SEATS; i++){

			SEM_WAIT(fillSeat[carID]); 			SWAP;
			SEM_SIGNAL(getPassenger);			SWAP;	// get the passenger
			SEM_WAIT(seatTaken);				SWAP;	// save the passenger rideDone sem
			
			passengerWait[i] = getPassengerWaitSem();	SWAP;

			SEM_SIGNAL(seatFilled[carID]); SWAP;	// signal the next seat ready
		}

		// the car is full, get a driver
		SEM_WAIT(getDriverMutex);			SWAP;
		{
			// put up our hand for the driver and wait
			SEM_SIGNAL(needDriver);			SWAP;
			// wakeup a driver if he is asleep
			SEM_SIGNAL(wakeupDriver);		SWAP;
			// save the driver sem
			driverDone = getDriverWaitSem(carID); SWAP;
		}
		SEM_SIGNAL(getDriverMutex);			SWAP;

		// wait for the ride to finish
		SEM_WAIT(rideOver[carID]);		SWAP;

		// release the driver 
		SEM_SIGNAL(driverDone);			SWAP;
	
		// release the passengers
		for(i = 0; i < NUM_SEATS; i++){
			SEM_SIGNAL(passengerWait[i]);	SWAP;
		}

		SEM_WAIT(parkMutex);	SWAP;
		{
			if(myPark.numExitedPark == NUM_VISITORS) break;
		}
		SEM_SIGNAL(parkMutex);	SWAP;
	}

	return 0;

}

// ***********************************************************************
// ***********************************************************************
// Visitor task
// 
int visitorTask(int argc, char* argv[]){

	char semName[32];		SWAP;
	int visitorID = atoi(argv[1]);	SWAP;
	sprintf(semName,"%sWait",argv[0]);	SWAP;
	Semaphore* visitor = createSemaphore(semName,BINARY,0);	SWAP;

	// insert a visitor at random into the park line (max 10 sec wait)
	randomizeVisitor(100, visitor);	SWAP;

	// once it is the visitor arrives at the park, add them to the park line
	addVisitorToParkLine();	SWAP;
	SEM_WAIT(roomInPark);	SWAP;
	{
		// once the visitor gets to the park entrance, move them to the ticket line
		moveToTicketLine();	SWAP;
		waitForTicket(visitor);	SWAP;

		// once the visitor has a ticket, move them to the museum line
		// once the visitor is at the front of the museum line and
		// a spot opens in the museum, move them to the museum 
		moveToMuseumLine();	SWAP;
		waitForMuseumExperience(visitor);	SWAP;

		// once the visitor has experienced the museum, move them to the tour line
		// then wait until they have finished their experience on the tour
		moveToTourLine();	SWAP;
		waitForTourExperience(visitor);	SWAP;

		// once the visitor has finished the tour, have them get in line for
		// the gift shop and wait until they have gone through
		moveToGiftShopLine();	SWAP;
		waitForGiftShopExperience(visitor);		SWAP;

		// once the visitor has left the gift shop, they exit the park
		exitPark();	SWAP;
	}
	// allow another visitor to enter the park
	SEM_SIGNAL(roomInPark);	SWAP;

	return 0;
}

// **********************************************************************
// **********************************************************************
// driver task
//
int driverTask(int argc, char* argv[]){

	char semName[32];		SWAP;
	int driverID = atoi(argv[1]);	SWAP;
	sprintf(semName,"driverDone%d",driverID);	SWAP;
	Semaphore* driverDone = createSemaphore(semName,BINARY,0);	SWAP;

	while(1){
		SEM_WAIT(wakeupDriver);	SWAP;

		if(semTryLock(needDriver)){

			// the driver needs to get in to the car
			passDriverWaitSem(driverDone,driverID);	SWAP;
			// let the car task know the driver is ready
			SEM_SIGNAL(driverReady);	SWAP;
			// wait for the driver to start the tour
			SEM_WAIT(driverDone);	SWAP;
			// the driver starts the tour
			updateCarDriver(driverID, 0);	SWAP;

		}else if(semTryLock(needTicket)){

			// signal that someone needs a ticket 
			updateCarDriver(driverID,-1);	SWAP;
			// wait for a ticket to become available
			SEM_WAIT(tickets);	SWAP;
			SEM_SIGNAL(ticketReady);	SWAP;
			// buy the ticket once it is available
			SEM_WAIT(buyTicket);	SWAP;
			// the driver goes back to sleep
			updateCarDriver(driverID,0);	SWAP;

		}
		SEM_WAIT(parkMutex); SWAP;
		{
			if(myPark.numExitedPark == NUM_VISITORS){
				break;
			}
		}
		SEM_SIGNAL(parkMutex); SWAP;
			
	}

	return 0;
}


// ***********************************************************************
// ***********************************************************************
// delta clock command
int P3_dc(int argc, char* argv[])
{
	printf("\nDelta Clock");
	// ?? Implement a routine to display the current delta clock contents

	printDeltaClock();

	
	return 0;
} // end CL3_dc

// **********************************************************************
// **********************************************************************
// insert delta clock function and update delta clock function
//
void insertDeltaClock(int time,Semaphore* sem){

	if(time < 0 || sem == NULL) return;
	if(dc->size >= MAX_TASKS) return;
	SEM_WAIT(dcMutex);	SWAP;
	{
		int tics = time;	SWAP;
		int i = dc->size;	SWAP;
		// find where the new entry should go
		if(dc->size > 0){
			while(tics > 0 && i > 0){ 
				time = tics;	SWAP;
				tics -= dc->clock[i-1].time; SWAP;
				if(tics > 0){
					dc->clock[i] = dc->clock[--i]; SWAP;
				}
			}
		}
		// create a new delta clock entry
		DCEntry dce;				
		dce.sem = sem;	SWAP;			
		dce.time = (i == 0) ? tics : time;	SWAP
		dc->clock[i] = dce; SWAP;
		// adjust the time of the entry after if it is not at the bottom
		if(i != 0){ dc->clock[i-1].time = abs(tics);	SWAP;}
		// adjust the size of the delta clock
		dc->size++;		SWAP;
	}							
	SEM_SIGNAL(dcMutex); SWAP;
}

void updateDeltaClock(int numTics){
	if(dc->size == 0){
		deltaTic = 0;
		return;
	}
	// if the mutex indicates we can access the delta clock
	if(dcMutex->state == 1){
		// determine if more than one task should be run
		int i = dc->size-1;	
		//int ticsLeft = numTics - dc->clock[i].time;				
		dc->clock[i].time -= numTics;			// decrement the time of the top entry
		while(dc->size > 0 && dc->clock[i].time <= 0){
			SEM_SIGNAL(dc->clock[i].sem);	
			dc->clock[i] = emptyDCEntry;
			dc->size--;						// decrement the clock size	
			i--;							// move to the next element
		//	if(ticsLeft > 0 && dc->size > 0){
		//		// still tics left, need to decrement again
		//		int nextTicsLeft = ticsLeft - dc->clock[i].time;
		//		dc->clock[i].time -= ticsLeft;
		//		ticsLeft = nextTicsLeft;
		//	}
			SEM_SIGNAL(dcChange);
		}
		deltaTic = 0;
	}	
}


// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// display all pending events in the delta clock list
void printDeltaClock(void)
{
	int i;
	for(i = dc->size - 1; i >= 0; i--){
		printf("\n%4d%4d  %-20s", i, dc->clock[i].time, dc->clock[i].sem->name);
	}
	return;
}


// ***********************************************************************
// test delta clock
int P3_tdc(int argc, char* argv[])
{
	dcChange = createSemaphore("dcChange",BINARY,1);
	dcMutex = createSemaphore("dcMutex",BINARY,1);

	createTask( "DC Test",			// task name
		dcMonitorTask,		// task
		10,					// task priority
		argc,					// task arguments
		argv);

	timeTaskID = createTask( "Time",		// task name
		timeTask,	// task
		10,			// task priority
		argc,			// task arguments
		argv);
	return 0;
} // end P3_tdc



// ***********************************************************************
// monitor the delta clock task
int dcMonitorTask(int argc, char* argv[])
{
	int i, flg;
	char buf[32];
	Semaphore* event[10];
	// create some test times for event[0-9]
	int ttime[10] = {
		90, 300, 50, 170, 340, 300, 50, 300, 40, 110	};

	for (i=0; i<10; i++)
	{
		sprintf(buf, "event[%d]", i);
		event[i] = createSemaphore(buf, BINARY, 0);
		insertDeltaClock(ttime[i], event[i]);
	}
	printDeltaClock();

	while (dc->size > 0)
	{
		SEM_WAIT(dcChange)
		flg = 0;
		for (i=0; i<10; i++)
		{
			if (event[i]->state ==1)			{
					printf("\n  event[%d] signaled", i);
					event[i]->state = 0;
					flg = 1;
				}
		}
		if (flg) printDeltaClock();
	}
	printf("\nNo more events in Delta Clock");

	// kill dcMonitorTask
	tcb[timeTaskID].state = S_EXIT;
	return 0;
} // end dcMonitorTask


extern Semaphore* tics1sec;

// ********************************************************************************************
// display time every tics1sec
int timeTask(int argc, char* argv[])
{
	char svtime[64];						// ascii current time
	while (1)
	{
		SEM_WAIT(tics1sec)
		printf("\nTime = %s", myTime(svtime));
	}
	return 0;
} // end timeTask



// ***********************************************************************
// ***********************************************************************
// semaphore handlers

Semaphore* getPassengerWaitSem(){
	Semaphore* waitSem;
	// put up our hand for a passenger and wait
	SEM_SIGNAL(needPassengerWait);			SWAP;
	SEM_WAIT(passengerResourceReady);		SWAP;
	// assign the sem to the current passenger's sem
	waitSem = globalSemaphore;				SWAP;
	// pur our hand down
	SEM_SIGNAL(passengerResourceAcquired);	SWAP;
	return waitSem;
}

Semaphore* getDriverWaitSem(int carID){
	Semaphore* waitSem;
	// put up our hand for a driver and wait
	SEM_SIGNAL(needDriverSem);					SWAP;
	SEM_WAIT(driverResourceReady);				SWAP;
	// assign the sem to the current driver's sem
	waitSem = globalSemaphore;					SWAP;
	// update the park variables about the driver
	updateCarDriver(globalDriverID, carID + 1);	SWAP;
	// put our hand down
	SEM_SIGNAL(driverResourceAcquired);			SWAP;
	return waitSem;
}

void updateCarDriver(int driverID, int carPos){
	// wait to access shared memory
	SEM_WAIT(parkMutex);	SWAP;
	{
		myPark.drivers[driverID] = carPos;	SWAP;
	}
	SEM_SIGNAL(parkMutex);	SWAP;
	// open shared memory to others
}

void randomizeVisitor(int maxTime, Semaphore* visitor){
	// put the visitor semaphore into the delta clock
	insertDeltaClock(rand() % maxTime, visitor);	SWAP;
	// wait until the visitor is at the front of the line
	SEM_WAIT(visitor);	SWAP;
}

void addVisitorToParkLine(){
	// wait to access shared memory
	SEM_WAIT(parkMutex);	SWAP;
	{
		myPark.numOutsidePark++;	SWAP;
	}
	SEM_SIGNAL(parkMutex);	SWAP;
	// open shared memory to others
}

void moveToTicketLine(){
	// wait to access shared memory
	SEM_WAIT(parkMutex);	SWAP;
	{
		myPark.numOutsidePark--;	SWAP;
		myPark.numInPark++;			SWAP;
		myPark.numInTicketLine++;	SWAP;
	}
	SEM_SIGNAL(parkMutex);	SWAP;
	// open shared memory to others
}

void waitForTicket(Semaphore* visitor){
	// add the visitor to the delta clock (max wait of 3 sec)
	randomizeVisitor(30, visitor);	SWAP;

	// once the visitor is at the front of the line, wait for a ticket
	SEM_WAIT(getTicketMutex);	SWAP;
	{
		// put our hand up for a ticketReady
		SEM_SIGNAL(needTicket);	SWAP;

		// wakeup a driver to sell the ticket
		SEM_SIGNAL(wakeupDriver);	SWAP;

		// wait until a ticket is available
		SEM_WAIT(ticketReady);	SWAP;

		// buy a ticket once it is available
		SEM_SIGNAL(buyTicket); SWAP;

		reduceTicketsAvailable();	SWAP;
	}
	SEM_SIGNAL(getTicketMutex);	SWAP;
	// allow other visitors to get a ticket
}

void reduceTicketsAvailable(){
	// wait to access shared memory
	SEM_WAIT(parkMutex);	SWAP;
	{
		myPark.numTicketsAvailable--;	SWAP;
	}
	SEM_SIGNAL(parkMutex);	SWAP;
	// open shared memory to others
}

void moveToMuseumLine(){
	// wait to access shared memory
	SEM_WAIT(parkMutex);	SWAP;
	{
		myPark.numInTicketLine--;	SWAP;
		myPark.numInMuseumLine++;	SWAP;
	}
	SEM_SIGNAL(parkMutex);	SWAP;
	// open shared memory to others 
}

void waitForMuseumExperience(Semaphore* visitor){
	// add the visitor to the delta clock (max wait 3 sec)
	randomizeVisitor(30, visitor);

	// wait for a spot to open up in the museum
	SEM_WAIT(roomInMuseum);
	{
		// move the visitor into the museum and
		// set how long they will stay (max 3 sec)
		moveToMuseum();
		randomizeVisitor(30, visitor);
	}
	// allow the next visitor to enter museum
	SEM_SIGNAL(roomInMuseum);
}

void moveToMuseum(){
	// wait to access shared memory
	SEM_WAIT(parkMutex);	SWAP;
	{
		myPark.numInMuseumLine--; 	SWAP;
		myPark.numInMuseum++;		SWAP;
	}
	SEM_SIGNAL(parkMutex);	SWAP;
	// open shared memory to others 
}

void moveToTourLine(){
	// wait to access shared memory
	SEM_WAIT(parkMutex);	SWAP;
	{
		myPark.numInMuseum--;	SWAP;
		myPark.numInCarLine++;	SWAP;
	}
	SEM_SIGNAL(parkMutex);	SWAP;
	// open shared memory to others
}

void waitForTourExperience(Semaphore* visitor){
	// wait in line (max 3 sec)
	randomizeVisitor(30, visitor);	SWAP;

	// at front of line, wait for request for passenger 
	SEM_WAIT(getPassenger);		SWAP;
	// get in the car
	moveToTour();	SWAP;
	// release the visitor to the custody of the driver/car
	passPassengerWaitSem(visitor);	SWAP;
	// wait for the visitor to finish the tour
	SEM_WAIT(visitor);	SWAP;
}

void moveToTour(){
	// wait to access shared memory
	SEM_WAIT(parkMutex);	SWAP;
	{
		myPark.numInCarLine--;			SWAP;
		myPark.numInCars++;				SWAP;
		myPark.numTicketsAvailable++;	SWAP;
		SEM_SIGNAL(tickets);			SWAP;
	}
	SEM_SIGNAL(parkMutex);	SWAP;
	// open shared memory to others
}

void passPassengerWaitSem(Semaphore* visitor){
	// wait for global resources to be available
	SEM_WAIT(resourceMutex);	SWAP;
	{
		// notify the car task that the visitor got in
		SEM_SIGNAL(seatTaken);	SWAP;
		// wait for the car to ask for the visitor sem
		SEM_WAIT(needPassengerWait);	SWAP;
		// put the visitor sem in the mailbox
		globalSemaphore = visitor;	SWAP;
		// tell the car task the sem is ready
		SEM_SIGNAL(passengerResourceReady);	SWAP;
		// wait for the car task to say they got the sem
		SEM_WAIT(passengerResourceAcquired);	SWAP;
	}
	SEM_SIGNAL(resourceMutex);	SWAP;
	// open up the global resources
}

void moveToGiftShopLine(){
	// wait to access shared memory
	SEM_WAIT(parkMutex);	SWAP;
	{
		myPark.numInCars--;		SWAP;
		myPark.numInGiftLine++;	SWAP;
	}
	SEM_SIGNAL(parkMutex);	SWAP;
	// open shared memory to others
}

void waitForGiftShopExperience(Semaphore* visitor){
	// wait in line (max 3 sec)
	randomizeVisitor(30, visitor);	SWAP;
	// enter the gift shop once there is room
	SEM_WAIT(roomInGiftShop);	SWAP;
	{
		moveToGiftShop();
		// spend no more than 3 sec in gift shop
		randomizeVisitor(30, visitor);	SWAP;
	}
	SEM_SIGNAL(roomInGiftShop);	SWAP;
}

void moveToGiftShop(){
	// wait to access shared memory
	SEM_WAIT(parkMutex);	SWAP;
	{
		myPark.numInGiftLine--;	SWAP;
		myPark.numInGiftShop++;	SWAP;
	}
	SEM_SIGNAL(parkMutex);	SWAP;
	// open shared memory to others
}

void exitPark(){
	// wait to access shared memory
	SEM_WAIT(parkMutex);	SWAP;
	{
		myPark.numInGiftShop--;	SWAP;
		myPark.numInPark--;		SWAP;
		myPark.numExitedPark++;	SWAP;
	}
	SEM_SIGNAL(parkMutex);	SWAP;
	// open shared memory to others
}

void passDriverWaitSem(Semaphore* driver, int driverID){
	// wait for the global resource to become available
	SEM_WAIT(resourceMutex);	SWAP;
	{
		// put our hand up to signal we are ready
		SEM_WAIT(needDriverSem);	SWAP;
		// put the driver sem into the mailbox
		globalSemaphore = driver;	SWAP;
		globalDriverID = driverID;	SWAP;
		// send the mailbox to the car task
		SEM_SIGNAL(driverResourceReady); SWAP;
		// wait for the car task to confirm it got the sem
		SEM_WAIT(driverResourceAcquired);	SWAP;
	}
	// free the global resource for others to use
	SEM_SIGNAL(resourceMutex);	SWAP;
}