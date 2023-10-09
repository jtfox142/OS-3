#include<unistd.h>
#include<sys/types.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/ipc.h>
#include<sys/shm.h>

void output(int pid, int ppid, int sysClockS, int sysClockNano, int termTimeS, int termTimeNano) {
	printf("WORKER PID:%d PPID:%d SysclockS:%d SysClockNano:%d TermTimeS:%d TermTimeNano:%d\n", pid, ppid, sysClockS, sysClockNano, termTimeS, termTimeNano);	
}

int checkTime(int sysClockS, int sysClockNano, int termTimeS, int termTimeNano) {
	if(sysClockS < termTimeS)
		return 1;
	if(sysClockNano < termTimeNano)
		return 1;
	return 0;
}

int main(int argc, char** argv) {
	int seconds;
	seconds = atoi(argv[0]);
	int nanoseconds;
	nanoseconds = atoi(argv[1]);	
       	
	pid_t ppid = getppid();
	pid_t pid = getpid();

	//get access to shared memory
	const int sh_key = ftok("./oss.c", 0);
	int shm_id = shmget(sh_key, sizeof(int) * 2, IPC_CREAT | 0666);
	int *shm_ptr = shmat(shm_id, 0, 0);

	int termTimeS;
        termTimeS = shm_ptr[0] + seconds;
        int termTimeNano;
        termTimeNano = shm_ptr[1] + nanoseconds;

	int outputTimer;
      	outputTimer = shm_ptr[0];
	int outputCounter;
	outputCounter = 1;
	
	output(pid, ppid, shm_ptr[0], shm_ptr[1], termTimeS, termTimeNano);	
	printf("--Just starting\n");

  	while (checkTime(shm_ptr[0], shm_ptr[1], termTimeS, termTimeNano)) {
		if(shm_ptr[0] > outputTimer) {
			outputTimer = shm_ptr[0];
			output(pid, ppid, shm_ptr[0], shm_ptr[1], termTimeS, termTimeNano);
			printf("--%d seconds have passed since starting\n", outputCounter++); 
	
		}
	}
	output(pid, ppid, shm_ptr[0], shm_ptr[1], termTimeS, termTimeNano);
	printf("--Terminating\n");

	//detach from shared memory
	shmdt(shm_ptr);
	return EXIT_SUCCESS;
}
