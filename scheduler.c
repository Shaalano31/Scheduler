#include "priority_queue.h"
#include <math.h>

int count = 0;
int numProcesses = 0;
int shmid;

float sumWTA = 0;
int choice = 0;
int sumWaiting = 0;
int usefultime = 0;
int timestarted = 0;
float standardDeviation = 0;
float *WTA;
FILE *fp;
FILE *fp2;

void handler(int signum)
{
    runningProcess->data.turnaroundtime = getClk() - runningProcess->data.arrivaltime;
    runningProcess->data.weightedturnaroundtime = (float)runningProcess->data.turnaroundtime /runningProcess->data.runningtime;
    runningProcess->data.waitingtime += runningProcess->data.responsetime;

    fprintf(fp,"At time %d process %d finished arr %d total %d remain 0 wait %d TA %d WTA %.2f\n", getClk(), runningProcess->data.id, runningProcess->data.arrivaltime, runningProcess->data.runningtime, runningProcess->data.waitingtime, runningProcess->data.turnaroundtime, runningProcess->data.weightedturnaroundtime);
    //printf("\nFinished process with id %d at time %d\n", runningProcess->data.id, stat_loc>>8);
    sumWTA += runningProcess->data.weightedturnaroundtime;
    sumWaiting += runningProcess->data.waitingtime;
    usefultime += runningProcess->data.runningtime;
    WTA[count] = runningProcess->data.weightedturnaroundtime;

    //Deallocate memory
    fprintf(fp2,"At time %d freed %d bytes from process %d from %d to %d\n", getClk(), runningProcess->data.memsize, runningProcess->data.id, runningProcess->data.memAssigned->beginning, runningProcess->data.memAssigned->beginning + runningProcess->data.memAssigned->size - 1);
    int val = memoryDeallocation(runningProcess->data.id, runningProcess->data.memsize, mainMemory);

    //checking waiting queue
    if(waitingQueue != NULL){
        int memUnit = largestAvailableMemUnit(mainMemory);
        struct node* processToAllocate = dequeueWaiting(memUnit);
        if(processToAllocate != NULL){
            struct memUnit* memoryAllocated = memoryAllocation(processToAllocate->data.id, processToAllocate->data.memsize, mainMemory);
            fprintf(fp2,"At time %d allocated %d bytes for process %d from %d to %d\n", getClk(), processToAllocate->data.memsize, processToAllocate->data.id, memoryAllocated->beginning, memoryAllocated->beginning + memoryAllocated->size - 1);
            processToAllocate->data.memAssigned = memoryAllocated;

            if(choice == 1){
                enqueue(processToAllocate->data, processToAllocate->data.priority);
            }
            else if(choice == 2){
                enqueue(processToAllocate->data, processToAllocate->data.runningtime);
            }
            else if(choice == 3){
                enqueue(processToAllocate->data, 1);
            }     
            free(processToAllocate);
        }
    }
    
    free(runningProcess);
    runningProcess = NULL;

    count++;
    if(count == numProcesses) {
        //printf("All processes done\n");
        fclose(fp);
        fp = fopen("scheduler.perf","w");
        
        fprintf(fp,"CPU utilization = %d%%\n", (usefultime * 100)/(getClk() - timestarted));
        fprintf(fp,"Avg WTA = %.2f\n", (float)sumWTA/count);
        fprintf(fp,"Avg Waiting = %.2f\n", (float)sumWaiting/count);
        for(int i=0; i<count; i++) {
            standardDeviation += pow(((float)sumWTA/count - (WTA[i])),2);
        }
        fprintf(fp,"Std WTA = %.2f\n", sqrt((float)standardDeviation/count));
        
        fclose(fp);
        shmctl(shmid, IPC_RMID, (struct shmid_ds *)0);
        destroyClk(false);
        exit(0);
    }   
}


int main(int argc, char * argv[])
{
    initClk();
    timestarted = getClk();
    printf("I am scheduler\n");
    choice = atoi(argv[1]);
    int quantum = choice == 3 ? atoi(argv[2]) : 1;
    printf("Please wait until all processes terminate!\n");
    signal (SIGUSR1, handler);
    //TODO implement the scheduler :) 
    //upon termination release the clock resources.
    numProcesses = atoi(argv[3]);
    WTA = (float*)malloc(numProcesses * sizeof(int));
    
    mainMemory = (struct memUnit*)malloc(sizeof(struct memUnit));
    mainMemory->isAllocated = false;
    mainMemory->size = 1024;
    mainMemory->smallestUnit = mainMemory->size;
    mainMemory->beginning = 0;

    key_t key_id;
    int Up, send_val, rec_val;
    key_id = ftok("keyfile.txt", 1);
    Up = msgget(key_id, 0666 | IPC_CREAT);
    key_id = ftok("keyfile.txt", 2);
    shmid = shmget(key_id, 12, IPC_CREAT | 0644);
    if (Up == -1 | shmid == -1)
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
    struct msgbuff message;

    fp = fopen("scheduler.log","w");
    fprintf(fp,"#At time x process y state arr w total z remain y wait k\n");
    fp2 = fopen("memory.log","w");
    fprintf(fp2,"#At time x allocated y bytes for process z from i to j\n");
    
    char remTime[4];
    sprintf(remTime, "%d", 0);
    strcpy((char *)sharedWaitedTime, remTime);

    int pid, RRtime = 0;
    while(1){

        rec_val = msgrcv(Up, &message, sizeof(message.mtext), 0, IPC_NOWAIT);
        if (rec_val != -1){ //chech if a new process was received
            if(choice == 1){
                enqueueReceived(message.mtext, message.mtext.priority);
            }
            else if(choice == 2){
                enqueueReceived(message.mtext, message.mtext.runningtime);
            }
            else if(choice == 3){
                enqueueReceived(message.mtext, 1);
            }
            continue;
        }

        if(receivedQueue != NULL){
            struct node* receivedProcess = dequeueReceived();
            struct memUnit* memoryAllocated = memoryAllocation(receivedProcess->data.id, receivedProcess->data.memsize, mainMemory);
            if(memoryAllocated == NULL){
                enqueueWaiting(receivedProcess->data);
                free(receivedProcess);
                continue;
            }
            fprintf(fp2,"At time %d allocated %d bytes for process %d from %d to %d\n", getClk(), receivedProcess->data.memsize, receivedProcess->data.id, memoryAllocated->beginning, memoryAllocated->beginning + memoryAllocated->size - 1);
            receivedProcess->data.memAssigned = memoryAllocated;

            if(choice == 1){
                enqueue(receivedProcess->data, receivedProcess->data.priority);
            }
            else if(choice == 2){
                enqueue(receivedProcess->data, receivedProcess->data.runningtime);
                if(runningProcess){
                    int remainingtime = (runningProcess->data.runningtime - (getClk() - runningProcess->data.starttime - atoi((char *)sharedWaitedTime)));
                    if((remainingtime != 0) && (receivedProcess->data.runningtime < remainingtime)){
                        kill(pid, SIGSTOP);
                        runningProcess->data.sleeptime = getClk();
                        //printf("Stop process with ID %d at time %d\n", runningProcess->data.id, getClk());
                        fprintf(fp,"At time %d process %d stopped arr %d total %d remain %d wait %d\n", getClk(), runningProcess->data.id, runningProcess->data.arrivaltime, runningProcess->data.runningtime, remainingtime, runningProcess->data.waitingtime + runningProcess->data.responsetime);
                        enqueue(runningProcess->data, remainingtime);
                        runningProcess = NULL;
                    }
                }
            }
            else if(choice == 3){
                enqueue(receivedProcess->data, 1);
            }
            free(receivedProcess);
            continue;
        }
               
        if(choice == 1 && runningProcess == NULL && readyQueue != NULL){
            dequeue();
            //printf("\n--------------------------------------\nHPF FORKING!\n");   
            pid = fork();
            if(pid == 0) { // child
                char runTime[4];
                char startTime[4];
                char id[4];
                char arrived[4];
                sprintf(id, "%d", runningProcess->data.id);
                sprintf(arrived, "%d", runningProcess->data.arrivaltime);
                sprintf(startTime, "%d", getClk());
                sprintf(runTime, "%d", runningProcess->data.runningtime);
                char *argv[]={"./process.out", runTime, startTime, id, arrived, NULL};
                execv(argv[0],argv);
            }
            runningProcess->data.pid = pid;
            runningProcess->data.starttime = getClk();
            runningProcess->data.responsetime = getClk() - runningProcess->data.arrivaltime;
            fprintf(fp,"At time %d process %d started arr %d total %d remain %d wait %d\n", getClk(), runningProcess->data.id, runningProcess->data.arrivaltime, runningProcess->data.runningtime, runningProcess->data.runningtime, runningProcess->data.waitingtime + runningProcess->data.responsetime);
        }
        else if(choice == 2 && runningProcess == NULL && readyQueue != NULL){
            dequeue();          
            pid = runningProcess->data.pid;
            if(pid == -1){
                //printf("\n--------------------------------------\nSRTN FORKING!\n"); 
                char remTime[4];
                sprintf(remTime, "%d", 0);               
                strcpy((char *)sharedWaitedTime, remTime);
                pid = fork();
                if(pid == 0) { // child
                    char runTime[4];
                    char startTime[4];
                    char id[4];
                    char arrived[4];
                    sprintf(id, "%d", runningProcess->data.id);
                    sprintf(arrived, "%d", runningProcess->data.arrivaltime);
                    sprintf(startTime, "%d", getClk());
                    sprintf(runTime, "%d", runningProcess->data.runningtime);
                    char *argv[]={"./process.out", runTime, startTime, id, arrived, NULL};
                    execv(argv[0],argv);
                }
                runningProcess->data.pid = pid;
                runningProcess->data.starttime = getClk();
                runningProcess->data.responsetime = getClk() - runningProcess->data.arrivaltime;
                fprintf(fp,"At time %d process %d started arr %d total %d remain %d wait %d\n", getClk(), runningProcess->data.id, runningProcess->data.arrivaltime, runningProcess->data.runningtime, runningProcess->data.runningtime, runningProcess->data.waitingtime + runningProcess->data.responsetime);
            }
            else{
                runningProcess->data.waitingtime = runningProcess->data.waitingtime + (getClk() - runningProcess->data.sleeptime);
                //printf("Continue process with ID %d at time %d\n", runningProcess->data.id, getClk());
                int remainingtime = (runningProcess->data.runningtime - (getClk() - runningProcess->data.starttime - runningProcess->data.waitingtime));
                fprintf(fp,"At time %d process %d resumed arr %d total %d remain %d wait %d\n", getClk(), runningProcess->data.id, runningProcess->data.arrivaltime, runningProcess->data.runningtime, remainingtime, runningProcess->data.waitingtime + runningProcess->data.responsetime);
                char remTime[4];
                sprintf(remTime, "%d", runningProcess->data.waitingtime);
                strcpy((char *)sharedWaitedTime, remTime);
                kill(pid, SIGCONT);
            }
        }
        else if(choice == 3 && (runningProcess == NULL || getClk()-RRtime >= quantum)){
            if(readyQueue == NULL){
                RRtime = getClk();
                continue;
            }
            if(runningProcess != NULL){               
                int remainingtime = (runningProcess->data.runningtime - (getClk() - runningProcess->data.starttime - atoi((char *)sharedWaitedTime)));
                if(remainingtime == 0){
                    continue;
                }
                kill(pid, SIGSTOP);
                //printf("Stop process with ID %d at time %d\n", runningProcess->data.id, getClk());
                runningProcess->data.sleeptime = getClk();
                fprintf(fp,"At time %d process %d stopped arr %d total %d remain %d wait %d\n", getClk(), runningProcess->data.id, runningProcess->data.arrivaltime, runningProcess->data.runningtime, remainingtime, runningProcess->data.waitingtime + runningProcess->data.responsetime);
                enqueue(runningProcess->data, 1);               
            }
            dequeue();
            pid = runningProcess->data.pid;
            if(pid == -1){
                //printf("\n--------------------------------------\nRR FORKING!\n");
                char remTime[4];
                sprintf(remTime, "%d", 0);               
                strcpy((char *)sharedWaitedTime, remTime);
                pid = fork();
                if(pid == 0) { // child
                    char runTime[4];
                    char startTime[4];
                    char id[4];
                    char arrived[4];
                    sprintf(id, "%d", runningProcess->data.id);
                    sprintf(arrived, "%d", runningProcess->data.arrivaltime);
                    sprintf(startTime, "%d", getClk());
                    sprintf(runTime, "%d", runningProcess->data.runningtime);
                    char *argv[]={"./process.out", runTime, startTime, id, arrived,NULL};
                    execv(argv[0],argv);
                }
                runningProcess->data.pid = pid;
                runningProcess->data.starttime = getClk();
                runningProcess->data.responsetime = getClk() - runningProcess->data.arrivaltime;
                fprintf(fp,"At time %d process %d started arr %d total %d remain %d wait %d\n", getClk(), runningProcess->data.id, runningProcess->data.arrivaltime, runningProcess->data.runningtime, runningProcess->data.runningtime, runningProcess->data.waitingtime + runningProcess->data.responsetime);
            }
            else{
                
                runningProcess->data.waitingtime = runningProcess->data.waitingtime + (getClk() - runningProcess->data.sleeptime);
                //printf("Continue process with ID %d at time %d\n", runningProcess->data.id, getClk());
                int remainingtime = (runningProcess->data.runningtime - (getClk() - runningProcess->data.starttime - runningProcess->data.waitingtime));
                fprintf(fp,"At time %d process %d resumed arr %d total %d remain %d wait %d\n", getClk(), runningProcess->data.id, runningProcess->data.arrivaltime, runningProcess->data.runningtime, remainingtime, runningProcess->data.waitingtime + runningProcess->data.responsetime);
                char remTime[4];
                sprintf(remTime, "%d", runningProcess->data.waitingtime);
                strcpy((char *)sharedWaitedTime, remTime);
                kill(pid, SIGCONT);
            }
            RRtime = getClk();
        }
    }

    // destroyClk(true);
    // exit(0);
}
