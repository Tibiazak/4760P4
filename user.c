#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/msg.h>
#define SHAREKEY 92195
#define MESSAGEKEY 110992
#typedef unsigned int uint;

typedef struct pcb {
    int simpid;
    int priority;
    int cpu_used;
    int time_in_system;
    int last_burst_time;
} pcb;

typedef struct sysclock {
    uint sec;
    uint nsec;
} sysclock;

typedef struct mesg_buf {
    long mtype;
    char mtext[100];
} message;

typedef struct share {
    sysclock Clock;
    pcb pcb_array[PROC_LIMIT];
} share;

// Interrupt handler for SIGUSR1, detaches shared memory and exits cleanly.
static void interrupt()
{
    printf("Received interrupt!\n");
    shmdt(Clock);
    exit(1);
}

int MsgID;
int ShareID;
share *Share;

int main(int argc, char *argv[]) {
    int timeslice;
    if(argc == 1)
    {
        printf("Error! Need to pass the pid!\n");
        return 1;
    }
    int pid = atoi(argv[1]);
    signal(SIGUSR1, interrupt); // registers interrupt handler

    ShareID = shmget(SHAREKEY, sizeof(share), 0777);
    if(ShareID == -1)
    {
        perror("User shmget");
        exit(1);
    }

    Share = (share *)(shmat(ShareID, 0, 0));
    if(Share == -1)
    {
        perror("User shmat");
        exit(1);
    }
    message msg;
    MsgID = msgget(MESSAGEKEY, 0666);
    msgrcv(MsgID, &msg, sizeof(msg), pid, 0);

    printf("The message is: %s\n", msg.mtext);
    shmdt(Share);
    return 0;
}