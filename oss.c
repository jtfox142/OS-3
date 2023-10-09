#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<time.h>

struct PCB {
	int occupied;
	pid_t pid;
	int startTimeSec;
	int startTimeNano;
};

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
	shm_ptr[1] += 10000;
	if(shm_ptr[1] >= 1000000000) {
		shm_ptr[1] = 0;
		shm_ptr[0] += 1;
	}
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
	int option;
	int proc;
	int simul;
	int timelimit;

	//allocate shared memory
	const int sh_key = ftok("./oss.c", 0);
	const int shm_id = shmget(sh_key, sizeof(int) * 2, IPC_CREAT | 0666);
	if(shm_id <= 0) {
		printf("Shared memory allocation failed\n");
		exit(1);
	}

	//attach to shared memory
	int *shm_ptr;
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

	struct PCB processTable[proc];

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
			processTable[totalChildren].occupied = 0;
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
	//detach from and delete memory
	shmdt(shm_ptr);
	shmctl(shm_id, IPC_RMID, NULL);
	return EXIT_SUCCESS;
}

