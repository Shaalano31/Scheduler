#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "headers.h"

#define MAX_FILE_NAME 200

void clearResources(int);
int Up;


int main(int argc, char * argv[])
{
    signal(SIGINT, clearResources);
    // TODO Initialization
       
    // 1. Read the input files.
    int numProcesses=0;
    char* programs[100];
    char line[50];

    FILE *file;
    file = fopen("processes.txt", "r");
    while(fgets(line, sizeof line, file)!=NULL) {
        numProcesses++;
    }
    fclose(file);
    struct processData data[numProcesses]; 
    numProcesses=0;
    file = fopen("processes.txt", "r");

    while(fgets(line, sizeof line, file)!=NULL) {
        //add each filename into array of programs
        programs[numProcesses]=malloc(sizeof(line));
        strcpy(programs[numProcesses],line);
        if(numProcesses!=0) {
            char *p = strtok (programs[numProcesses], "\t");
            data[numProcesses-1].id = atoi(p);
            p = strtok (NULL, "\t");
            data[numProcesses-1].arrivaltime = atoi(p);
            p = strtok (NULL, "\t");
            data[numProcesses-1].runningtime = atoi(p);
            p = strtok (NULL, "\t");
            data[numProcesses-1].priority = atoi(p);
            data[numProcesses-1].pid = -1;
            data[numProcesses-1].waitingtime = 0;
            p = strtok (NULL, "\t");
            data[numProcesses-1].memsize = atoi(p);
            
            printf("\n%d\t%d\t%d\t%d\n", data[numProcesses-1].id, data[numProcesses-1].arrivaltime, data[numProcesses-1].runningtime, data[numProcesses-1].priority);
        }
        numProcesses++;
    }
    numProcesses--;
    fclose(file);
    
    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    int choice; 
    printf("\n Choose the scheduling algorithm: \n 1-Non Pre-emptive HPF \n 2-Pre-emptive SRTN \n 3-Round Robin\n"); 
    scanf("%d", &choice);
    int quantum = 0;
    if(choice == 3) {
        printf("Enter value for quantum:\n");
        scanf("%d", &quantum);
    }

    //getting message queues
    key_t key_id;
    int send_val, mypid;
    key_id = ftok("keyfile.txt", 1);
    Up = msgget(key_id, 0666 | IPC_CREAT);
    if (Up == -1)
    {
        perror("Error in create");
        exit(-1);
    }
    mypid = getpid() % 10000;
    struct msgbuff message;

    // 3. Initiate and create the scheduler and clock processes.
    int pid;
    for(int i = 0; i<2; i++) {
        pid = fork();
        if(pid == 0 && i==0) { // child one
            char* const par[]={(char*)0};
            execv("./clk.out",par);
        }
        else if(pid == 0 && i==1) { // child two
            char algo[2];
            char quan[4];
            char numberProcesses[4];
            sprintf(algo, "%d", choice);
            sprintf(quan, "%d", quantum);
            sprintf(numberProcesses, "%d", numProcesses);
            char *argv[]={"./scheduler.out", algo , quan, numberProcesses, NULL};
            execv(argv[0],argv);
        }
    }

    // 4. Use this function after creating the clock process to initialize clock
    initClk();
    // To get time use this
    int x = getClk();
    printf("current time is %d\n", x);
    // TODO Generation Main Loop
    // 5. Create a data structure for processes and provide it with its parameters.

    // 6. Send the information to the scheduler at the appropriate time.
    int i=numProcesses;
    while(i > 0){
        x = getClk();
        if(data[numProcesses - i].arrivaltime <= x){
            //printf("sent %d at time : %d\n",data[numProcesses - i].id,x);
            message.mtext = data[numProcesses - i];
            message.mtype = mypid;
            send_val = msgsnd(Up, &message, sizeof(struct processData), !IPC_NOWAIT);
	        if (send_val == -1){
		        perror("Errror in send");	
                continue;	
    	    }
            i--;
        }
    }
    //printf("\nprocess generator sent all processes\n");
    int stat_loc;
    pid = wait(&stat_loc);
    msgctl(Up, IPC_RMID, (struct msqid_ds *)0);
    //printf("\nprocess generator sent all processes\n");
    // 7. Clear clock resources
    destroyClk(true);
}

void clearResources(int signum)
{
    //TODO Clears all resources in case of interruption
    msgctl(Up, IPC_RMID, (struct msqid_ds *)0);
    //exit(0);
}
