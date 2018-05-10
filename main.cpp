#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "semun.h" 
#include "binary_sems.h"  
#include <signal.h>
#include <sys/wait.h>
#include <string>
#define BUF_SIZE 1024
#define PRODUCER 0
#define CONSUMER 1
#define SEM_KEY 778
#define SHM_KEY 999
#define WRITE_SEM 0
#define READ_SEM 1
using namespace std;

int shmid, semid;
int fd;
char *letter;
pid_t parent;
struct shmseg *shmp; 
Boolean bsUseSemUndo = FALSE;
Boolean bsRetryOnEintr = TRUE;
union semun dummy;
string table = "";

struct shmseg{
	int cnt;
	char buf[BUF_SIZE];
};

void producer();
void consumer();
int initSemAvailable(int semId, int semNum);
int initSemInUse(int semId, int semNum);
void sigHandler(int sig);

//Main
int main(){	
	//Main process
	parent = getpid();

	//Fork producer and consumer child processes
	if(fork() == 0){
		//Child process (producer)	
		producer();
		kill(getpid(), SIGKILL);
	}else{
		//Parent process
		if(fork() == 0){
			//Child process (consumer)	
			consumer();
			kill(parent, SIGCHLD);
			kill(getpid(), SIGKILL);
		}
		
	}
	
	//Initialize Signal for printing table when childs have terminals
	if(signal(SIGUSR1, sigHandler) == SIG_ERR){
		cout << "Error" << endl;
		exit(1);
	}
	if(signal(SIGUSR2, sigHandler) == SIG_ERR){
		cout << "Error" << endl;
		exit(1);
	}
	if(signal(SIGCHLD, sigHandler) == SIG_ERR){
		cout << "Error" << endl;
		exit(1);
	}
	while(1);
}

//Signal handler
void sigHandler(int sig){
	if(sig == SIGUSR1){
		table += "P";
	}

	if(sig == SIGUSR2){
		table +="C";
	}

	if(sig == SIGCHLD){
		cout << table << endl;
		//exit(1);
	}
}

//Producer process (child 1)
void producer(){
	//Create two semaphores
	semid = semget(SEM_KEY, 2, IPC_CREAT | 0777);
	if(semid == -1){
		cout << "Producer failed to create keys" << endl;
		exit(1);
	}
	if(initSemAvailable(semid, WRITE_SEM) == -1){
		cout << "initSemAvailable" << endl;
		exit(1);
	}
	if(initSemInUse(semid, READ_SEM) == -1){
		cout << "initSemInUse" << endl;
		exit(1);
	}
	
	//Shared memory 
	shmid = shmget(SHM_KEY, sizeof(struct shmseg), IPC_CREAT | 0777);
	if(shmid == -1){
		cout << "shmget" << endl;
		exit(1);
	}
	
	//Attach memory to producer
	shmp = (shmseg*) shmat(shmid, NULL, 0);
	if(shmp == (void *) -1){
		cout << "shmat" << endl;
		exit(1);
	}
	
	//Move bytes from file1 to shared buffer (write)
	if ((fd = open("file1", O_RDONLY)) >= 0)	{
		while(1){
			//Wait for writer's turn. Increments semaphore
			if(reserveSem(semid, WRITE_SEM) == -1){
				cout << "reserveSem1" << endl;		
				exit(1);
			}
		
			//Read file 1 byte at a time into shmp's buffer		
			shmp->cnt = read(fd, shmp->buf, 1);
			
			//Send signal to handler that a byte has moved to buffer
			kill(parent, SIGUSR1);		
		
			//Give reader a turn. Decrement semaphore
			if(releaseSem(semid, READ_SEM) == -1){
				cout << "releaseSem" << endl;
			}
		
			//If EOF is reached, break out of loop
			if(shmp->cnt == 0){
				break;
			}

			//Sleep
			usleep(10000);
	    	}
	    	
		//Writer reserves semaphore once more so the reader completes final access to shared memory
		if(reserveSem(semid, WRITE_SEM) == -1){
			cout << "reserveSem2" << endl; 
			exit(1);			
		}
		
		//Writer removes shared memory segment and semaphore set
		if(semctl(semid, 0, IPC_RMID, dummy) == -1){
			cout << "semctl" << endl;
			exit(1);
		}			
		
		//Detach shared memory segment		
		if(shmdt(shmp) == -1){	
			cout << "shmdt" << endl;	
			exit(1);
		}
		if(shmctl(shmid, IPC_RMID, 0) == -1){
			cout << "shmctl" << endl;			
			exit(1);
		}
	}	
	
}


//Consumer process (child 2)
void consumer(){
	//Get semaphores created by writer 
	semid = semget(SEM_KEY, 0, 0);
	if(semid == -1){
		cout << "semget_cons" << endl;
		exit(1);
	}
	
	//Get shared memory created by reader
	shmid = shmget(SHM_KEY, 0, 0);
	if(shmid == -1){
		cout << "shmget" << endl;
		exit(1);
	}	
	
	//Attach shared memory to producer 
	shmp = (shmseg*) shmat(shmid, NULL, SHM_RDONLY);
	if(shmp == (void *) -1){
		cout << "shmat" << endl;
		exit(1);
	}

	//Open file2
	int fd2 = open("file2", O_WRONLY | O_CREAT, S_IRWXU);		
	
	//Move bytes from shared buffer to file2 (read)
	while(1){
		//Wait for readers's turn. Increments semaphore
		if(reserveSem(semid, READ_SEM) == -1){
			cout << "reserveSem3" << endl;		
			exit(1);
		}
		
		//If EOF is reached, break out of loop
		if(shmp->cnt == 0){
			break;
		}
				
		//Write
		if(write(fd2, shmp->buf, 1) == 1){ //STDOUT_FILENO
						
		}else{
			cout << "partial/failed write" << endl;
			exit(1);
		}

		//Send signal to handler that a byte has moved to buffer
		kill(parent, SIGUSR2);
			
		//Give writer a turn. Decrement semaphore
		if(releaseSem(semid, WRITE_SEM) == -1){
			cout << "releaseSem" << endl;
		}
		
		//Sleep
		usleep(10000);
	    }

	//Detach memory
	if(shmdt(shmp) == -1){
		cout << "shmdt" << endl;
		exit(1);
	}

	//Give writer one more turn. 
	if(releaseSem(semid, WRITE_SEM) == -1){
		cout << "releaseSem" << endl;
		exit(1);
	}
	
}

//Initalize semaphore to 1
int initSemAvailable(int semId, int semNum){
	union semun arg;
	arg.val = 1;
	return semctl(semId, semNum, SETVAL, arg);
}


//Initialize semaphore to 0
int initSemInUse(int semId, int semNum){
	union semun arg;
	arg.val = 0;
	return semctl(semId, semNum, SETVAL, arg);
}

//Reserve semaphore - decrement it by 1 
int reserveSem(int semId, int semNum){
    struct sembuf sops;

    sops.sem_num = semNum;
    sops.sem_op = -1;
    sops.sem_flg = bsUseSemUndo ? SEM_UNDO : 0;

    while (semop(semId, &sops, 1) == -1)
        if (errno != EINTR || !bsRetryOnEintr)
            return -1;

    return 0;
}

//Release semaphore - increment it by 1
int releaseSem(int semId, int semNum)
{
    struct sembuf sops;

    sops.sem_num = semNum;
    sops.sem_op = 1;
    sops.sem_flg = bsUseSemUndo ? SEM_UNDO : 0;

    return semop(semId, &sops, 1);
}



