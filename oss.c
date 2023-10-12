#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<time.h>
#include<signal.h>

/*
 * NOTES FOR TMRW
 * Move processTable to global memory. Use calloc to determine its size. Make sure to free it at the end!
 * Can also move sh_key and shm_id to glbal memory. this would allow me to shmdt and shmctl anywhere.
 * I think(?) I can use a switch in sighandler to differntiate alarm from ctrl+c, but they won't be too different anyway.
 */

struct PCB {
	int occupied;
	pid_t pid;
	int startTimeSec;
	int startTimeNano;
};

//For storing each child's PCB. Memory is allocated in main
struct PCB *processTable;

//Shared memory variables
int sh_key;
int shm_id;
int *shm_ptr;

//Needed for killing all child processes
int arraySize;

void help() {
        printf("This program is designed to have a parent process fork off into child processes.\n");
	printf("The child processes use a simulated clock in shared memory to keep track of runtime.\n");
	printf("The runtime is a random number of seconds and nanoseconds between 1 and the time limit prescribed by the user.\n\n");
        printf("The executable takes three flags: [-n proc], [-s simul], and [-t timelimit].\n");
        printf("The value of proc determines the total number of child processes to be produced.\n");
	printf("The value of simul determines the number of children that can run simultaneously.\n");
	printf("The value of timelimit determines the maximum number of seconds that a child process can take.\n");
	printf("\nMADE BY JACOB (JT) FOX\nSeptember 28th, 2023\n");
	exit(1);
}

void incrementClock(int *shm_ptr) {
	shm_ptr[1] += 800;
	if(shm_ptr[1] >= 1000000000) {
		shm_ptr[1] = 0;
		shm_ptr[0] += 1;
	}
}

void terminateProgram(int signum) {
	//detaches from and deletes shared memory
	shmdt(shm_ptr);
	shmctl(shm_id, IPC_RMID, NULL);

	//Kills any remaining active child processes
	int count;
	for(count = 0; count < arraySize; count++) {
		if(processTable[count].occupied)
			kill(processTable[count].pid, signum);
	}

	//Frees memory allocated for processTable
	free(processTable);
	processTable = NULL;

	printf("Program is terminating. Goodbye!\n");
	exit(1);
}

//Can use processTable to get active child process pids and kill them
//How to free shared memory?
//Could use an int runFlag = 1, then sigHandler sets runFlag to 0. If !runFlag, shmdt() && shmctl
//Where to test for this flag?
void sighandler(int signum) {
	printf("\nCaught signal %d\n", signum);
	terminateProgram(signum);
	printf("If you're seeing this, then bad things have happened.\n");
}

/*
void startPCB(int tableEntry, struct PCB *processTable[], int pidNumber, int *time) {
	(*processTable[tableEntry]).occupied = 1;
	(*processTable[tableEntry]).pid = pidNumber;
	(*processTable[tableEntry]).startTimeSec = time[0];
	(*processTable[tableEntry]).startTimeNano = time[1];
}

void endPCB(int pidNumber, int tableSize, struct PCB *processTable[]) {
	int i;
	for(i = 0; i < tableSize; i++) {
		if(processTable[i]->pid == pidNumber) {
			processTable[i]->occupied = 0;
			return;
		}
	}
}*/

void outputTable(int rows, struct PCB processTable[]) {
	printf("Process Table:\nEntry Occupied   PID\tStartS StartN\n");
	int i;
	for(i = 0; i < rows; i++) {
		printf("%d\t%d\t%d\t%d\t%d\t\n\n", i, processTable[i].occupied, processTable[i].pid, processTable[i].startTimeSec, processTable[i].startTimeNano);
	}
}

int randNumGenerator(int max) {
	srand(time(NULL));
	return ((rand() % max) + 1);
}

int main(int argc, char** argv) {
	alarm(60);
	signal(SIGALRM, sighandler);
	signal(SIGINT, sighandler);
	int option;
	int proc;
	int simul;
	int timelimit;

	//allocate shared memory
	sh_key = ftok("./oss.c", 0);
	shm_id = shmget(sh_key, sizeof(int) * 2, IPC_CREAT | 0666);
	if(shm_id <= 0) {
		printf("Shared memory allocation failed\n");
		exit(1);
	}

	//attach to shared memory
	shm_ptr = shmat(shm_id, 0 ,0);
	if(shm_ptr <= 0) {
		printf("Attaching to shared memory failed\n");
		exit(1);
	}
	

	while ((option = getopt(argc, argv, "hn:s:t:")) != -1) {
  		switch(option) {
   			case 'h':
    				help();
    				break;
   			case 'n':
    				proc = atoi(optarg);
    				break;
   			case 's':
				simul = atoi(optarg);
				break;
			case 't':
				timelimit = atoi(optarg);
				break;
		}
	}
	
	arraySize = proc;
	//Allocates memory for the processTable stored in global memory
	processTable = calloc(arraySize, sizeof(struct PCB));

	int totalChildren;
	int runningChildren;
	totalChildren = 0;
	runningChildren = 0;

	//set clock to zero
        shm_ptr[0] = 0;
        shm_ptr[1] = 0;


	//vars for fetching worker termTime values
	const int maxNano = 1000000000;
	int randNumS, randNumNano;

	//char str for sending randNum values to the worker
	char secStr[sizeof(int)];
	char nanoStr[sizeof(int)];

	//initialize child processes
	while(runningChildren < simul) { 
  		pid_t childPid = fork();                

      		if(childPid == 0) {
			randNumS = randNumGenerator(timelimit);
			randNumNano = randNumGenerator(maxNano);
			snprintf(secStr, sizeof(int), "%d", randNumS);
			snprintf(nanoStr, sizeof(int), "%d", randNumNano);
			execlp("./worker", secStr, nanoStr,  NULL);
       			exit(1);
       		}
		else {
			processTable[runningChildren].occupied = 1;
			processTable[runningChildren].pid = childPid;
			processTable[runningChildren].startTimeSec = shm_ptr[0];
			processTable[runningChildren].startTimeNano = shm_ptr[1];
			runningChildren++;
			totalChildren++;
		}
       	}
      
	int outputTimer;
	outputTimer = 0;
	int halfSecond = 500000000;
	do {
		incrementClock(shm_ptr);

		//Use an alarm for this
		/*if(sixtySecondsHasPassed)
			terminateProgram();*/
		
		if(abs(shm_ptr[1] - outputTimer) >= halfSecond){
			outputTimer = shm_ptr[1];
			printf("\nOSS PID:%d SysClockS:%d SysClockNano:%d\n", getpid(), shm_ptr[0], shm_ptr[1]); 
			outputTable(proc, processTable);
		}
		
		int status;
		int pid = waitpid(-1, &status, WNOHANG); //Will return 0 if no processes have terminated
		if(pid) {
			int i;
			for(i = 0; i < arraySize; i++) {
				if(processTable[i].pid == pid)
					processTable[i].occupied = 0;
			}
			runningChildren--;
			if(totalChildren < proc) {
				pid_t childPid = fork(); //Launches child
				if(childPid == 0) {
					randNumS = randNumGenerator(timelimit);
					randNumNano = randNumGenerator(maxNano);
					snprintf(secStr, sizeof(int), "%d", randNumS);
					snprintf(nanoStr, sizeof(int), "%d", randNumNano);
					execlp("./worker", secStr, nanoStr, NULL);
					exit(1);
				}
				else {
					processTable[totalChildren].occupied = 1;
					processTable[totalChildren].pid = childPid;
					processTable[totalChildren].startTimeSec = shm_ptr[0];
					processTable[totalChildren].startTimeNano = shm_ptr[1];
					runningChildren++;
					totalChildren++;
				}
			}
		}
	} while(runningChildren);	

	pid_t wpid;
	int status = 0;
	while((wpid = wait(&status)) > 0);
	terminateProgram(SIGTERM);
	return EXIT_SUCCESS;
}


