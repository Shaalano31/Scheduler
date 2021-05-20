#include <stdio.h>      //if you don't use scanf/printf change this include
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/queue.h>
#include <string.h>

typedef short bool;
#define true 1
#define false 0

#define SHKEY 300

struct processData
{
    int id;
    int memsize;
    int pid;
    int arrivaltime;
    int priority;
    int runningtime;
    int waitingtime;
    int finishtime;
    int starttime;
    int turnaroundtime;
    int responsetime;
    float weightedturnaroundtime;
    int sleeptime;
    struct memUnit* memAssigned;
};

struct memUnit
{
    bool isAllocated;
    int processId;
    int size;
    int smallestUnit;
    int beginning;
    struct memUnit* childOne;
    struct memUnit* childTwo;
};

struct memUnit* mainMemory = NULL;

struct memUnit* memoryAllocation(int processId, int processSize, struct memUnit* mem)
{
    if(mem->isAllocated){
        return NULL;
    }

    if(mem->childOne == NULL){
        if((mem->size / 2) < processSize && processSize <= (mem->size)){
            mem->isAllocated = true;
            mem->processId = processId;
            return mem;
        }
        else if ((mem->size / 2) >= processSize){
            struct memUnit* new_childOne;
            new_childOne = (struct memUnit*)malloc(sizeof(struct memUnit));
            new_childOne->isAllocated = false;
            new_childOne->size = mem->size / 2;
            new_childOne->smallestUnit = mem->size / 2;
            new_childOne->beginning = mem->beginning;
            mem->childOne = new_childOne;

            struct memUnit* new_childTwo;
            new_childTwo = (struct memUnit*)malloc(sizeof(struct memUnit));
            new_childTwo->isAllocated = false;
            new_childTwo->size = mem->size / 2;
            new_childTwo->smallestUnit = mem->size / 2;
            new_childTwo->beginning = mem->beginning + (mem->size / 2);
            mem->childTwo = new_childTwo;

            struct memUnit* val = memoryAllocation(processId, processSize, mem->childOne);
            mem->smallestUnit = val->size;
            return val;
        }
        else{
            return NULL;
        }
    }
    else if(mem->childTwo->smallestUnit < mem->childOne->smallestUnit){

        struct memUnit* val = memoryAllocation(processId, processSize, mem->childTwo);
        if(val == NULL){
            val = memoryAllocation(processId, processSize, mem->childOne);
        }
        if((val != NULL) && (val->size < mem->smallestUnit)){
            mem->smallestUnit = val->size;
        }
        return val;
    }
    else{
        struct memUnit* val = memoryAllocation(processId, processSize, mem->childOne);
        if(val == NULL){
            val = memoryAllocation(processId, processSize, mem->childTwo);
        }
        if((val != NULL) && (val->size < mem->smallestUnit)){
            mem->smallestUnit = val->size;
        }
        return val;
    }
};

int memoryDeallocation(int processId, int processSize, struct memUnit* mem)
{
    if(mem->isAllocated){
        if(mem->processId == processId){
            mem->isAllocated = false;
            //printf("freed with size %d from %d\n", mem->size, mem->beginning);
            return 2;   //try to merge
        }
        return 0;
    }
    else if(mem->childOne == NULL){
        return 0;   //not found
    }
    else if(processSize > mem->childOne->size){
        return 0;   //not found
    }

    int val = memoryDeallocation(processId, processSize, mem->childOne);
    if(val == 1){
        mem->smallestUnit = (mem->childTwo->smallestUnit < mem->childOne->smallestUnit) ? mem->childTwo->smallestUnit : mem->childOne->smallestUnit;
        return 1;   //do not merge
    }
    else if(val == 2){
        if(mem->childTwo->isAllocated == false && mem->childTwo->childOne == NULL){ //merge
            //printf("merging from %d to %d\n", mem->beginning, mem->beginning + mem->size - 1);
            free(mem->childOne);
            free(mem->childTwo);
            mem->childOne = NULL;
            mem->childTwo = NULL;
            mem->smallestUnit = mem->size;
            return 2;   //try to merge
        }
        mem->smallestUnit = mem->childTwo->smallestUnit;
        return 1;   //do not merge
    }

    val = memoryDeallocation(processId, processSize, mem->childTwo);
    if(val == 1){
        mem->smallestUnit = (mem->childTwo->smallestUnit < mem->childOne->smallestUnit) ? mem->childTwo->smallestUnit : mem->childOne->smallestUnit;
        return 1;   //do not merge
    }
    else if(val == 2){
        if(mem->childOne->isAllocated == false && mem->childOne->childOne == NULL){ //merge
            //printf("merging from %d to %d\n", mem->beginning, mem->beginning + mem->size - 1);
            free(mem->childOne);
            free(mem->childTwo);
            mem->childOne = NULL;
            mem->childTwo = NULL;
            mem->smallestUnit = mem->size;
            return 2;   //try to merge
        }
        mem->smallestUnit = mem->childOne->smallestUnit;
        return 1;   //do not merge
    }
    return 0;
}

int largestAvailableMemUnit(struct memUnit* mem)
{
    if(mem->isAllocated){
        return 0;
    }
    else if(mem->childOne == NULL){
        return mem->size;
    }

    int first = largestAvailableMemUnit(mem->childOne);
    int second = largestAvailableMemUnit(mem->childTwo);

    if(first >= second){
        return first;
    }
    else{
        return second;
    }
}

struct msgbuff
{
    long mtype;
    struct processData mtext;
};

union Semun
{
    int val;               /* value for SETVAL */
    struct semid_ds *buf;  /* buffer for IPC_STAT & IPC_SET */
    ushort *array;         /* array for GETALL & SETALL */
    struct seminfo *__buf; /* buffer for IPC_INFO */
    void *__pad;
};

///==============================
//don't mess with this variable//
int * shmaddr;                 //
//===============================



int getClk()
{
    return *shmaddr;
}


/*
 * All process call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
*/
void initClk()
{
    int shmid = shmget(SHKEY, 4, 0444);
    while ((int)shmid == -1)
    {
        //Make sure that the clock exists
        printf("Wait! The clock not initialized yet!\n");
        sleep(1);
        shmid = shmget(SHKEY, 4, 0444);
    }
    shmaddr = (int *) shmat(shmid, (void *)0, 0);
}


/*
 * All process call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether that this is the end of simulation.
 *                      It terminates the whole system and releases resources.
*/

void destroyClk(bool terminateAll)
{
    shmdt(shmaddr);
    if (terminateAll)
    {
        killpg(getpgrp(), SIGINT);
    }
}

void up(int sem)
{
    struct sembuf v_op;

    v_op.sem_num = 0;
    v_op.sem_op = 1;
    v_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &v_op, 1) == -1)
    {
        perror("Error in up()");
        exit(-1);
    }
}

void down(int sem)
{
    struct sembuf p_op;

    p_op.sem_num = 0;
    p_op.sem_op = -1;
    p_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &p_op, 1) == -1)
    {
        perror("Error in down()");
        exit(-1);
    }
}

