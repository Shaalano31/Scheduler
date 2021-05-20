#include "priority_queue.h"

/* Modify this file as needed*/
int remainingtime;

int main(int agrc, char * argv[])
{
    initClk();
    int remainingtime = atoi(argv[1]);
    int runningtime = atoi(argv[1]);
    int starttime = atoi(argv[2]);
    int id = atoi(argv[3]);
    int arrived = atoi(argv[4]);
    //printf("Started with PID %d at time %d with running time %d\n", getpid(), getClk(), runningtime);

    key_t key_id;
    int shmid;
    key_id = ftok("keyfile.txt", 2);
    shmid = shmget(key_id, 12, IPC_CREAT | 0644);
    if (shmid == -1)
    {
        perror("Error in create");
        exit(-1);
    }
    void *sharedWaitedTime = shmat(shmid, (void *)0, 0);
    if (sharedWaitedTime == (void *)-1)
    {
        perror("Error in attach");
        exit(-1);
    }

    int waitingtime = atoi((char *)sharedWaitedTime);

    while(remainingtime > 0) 
    {
        remainingtime = runningtime - (getClk() - starttime - atoi((char *)sharedWaitedTime));
    }

    int time = getClk();
    destroyClk(false);
    kill(getppid(), SIGUSR1);
    exit(time);
}