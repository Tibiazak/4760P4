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

#define TIMEOUT 3
#define SHAREKEY 92195
#define MESSAGEKEY 110992
#define BILLION 1000000000
#define PROC_LIMIT 18
#define MaxTimeBetweenNewProcsSecs 1
#define MaxTimeBetweeNewProcsNS 500000000
#define RealTimeProcs 10

typedef unsigned int uint;

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

int ShareID;
share *Share;
int MsgID;
FILE *fp;

// A function that catches SIGINT and SIGALRM
// It prints an alert to the screen then sends a signal to all the child processes to terminate,
// frees all the shared memory and closes the file.
static void interrupt(int signo, siginfo_t *info, void *context)
{
    int errsave;

    errsave = errno;
//    write(STDOUT_FILENO, TIMER_MSG, sizeof(TIMER_MSG) - 1);
    errno = errsave;
    signal(SIGUSR1, SIG_IGN);
    kill(-1*getpid(), SIGUSR1);
    exit(1);
}

// A function from the setperiodic code, it sets up the interrupt handler
static int setinterrupt()
{
    struct sigaction act;

    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = interrupt;
    if (((sigemptyset(&act.sa_mask) == -1) || (sigaction(SIGALRM, &act, NULL) == -1)) || sigaction(SIGINT, &act, NULL) == -1)
    {
        return 1;
    }
    return 0;
}


// A function that sets up a timer to go off after a specified number of seconds
// The timer only goes off once
static int setperiodic(double sec)
{
    timer_t timerid;
    struct itimerspec value;

    if (timer_create(CLOCK_REALTIME, NULL, &timerid) == -1)
    {
        return -1;
    }
    value.it_value.tv_sec = (long)sec;
    value.it_value.tv_nsec = 0;
    value.it_interval.tv_sec = 0;
    value.it_interval.tv_nsec = 0;
    return timer_settime(timerid, 0, &value, NULL);
}


int findopen(int *arr)
{
    int i = 0;
    while(i<18){
        if(!arr[i])
        {
            return i;
        }
        i++;
    }
    return -1;
}

void zeroarray(int * arr, int limit)
{
    int i;
    for (i = 0; i < limit; i++)
    {
        arr[i] = 0;
    }
}

void queueforward(int * arr, int limit)
{
    int i = 0;
    while(i < limit-1 && arr[i+1] != 0)
    {
        arr[i] = arr[i+1];
        arr[i+1] = 0;
        i++;
    }
}

int startTimer()
{
    // Set the timer-kill
    if (setinterrupt() == -1)
    {
        perror("Failed to set up handler");
        return 1;
    }
    if (setperiodic((long) TIMEOUT) == -1)
    {
        perror("Failed to set up timer");
        return 1;
    }
}

int main(int argc, char *argv[]){
    message msg;
    uint idlesec = 0;
    uint idlensec = 0;
    int queue0[PROC_LIMIT];
    int queue1[PROC_LIMIT];
    int queue2[PROC_LIMIT];
    int queue3[PROC_LIMIT];
    int bit_array[PROC_LIMIT];
    int blockQueue[PROC_LIMIT];
    zeroarray(&queue0, (int) PROC_LIMIT);
    zeroarray(&queue1, (int) PROC_LIMIT);
    zeroarray(&queue2, (int) PROC_LIMIT);
    zeroarray(&queue3, (int) PROC_LIMIT);
    zeroarray(&bit_array, (int) PROC_LIMIT);
    zeroarray(&blockQueue, (int) PROC_LIMIT);

    if (startTimer() == 1)
    {
        perror("startTimer failed!");
        return 1;
    }

    // Setup the clock in shared memory
    ShareID = shmget(SHAREKEY, sizeof(share), 0777 | IPC_CREAT);
    if(ShareID == -1)
    {
        perror("Master shmget");
        exit(1);
    }

    Share = (share *)(shmat(ShareID, 0, 0));
    if(Share == -1)
    {
        perror("Master shmat");
        exit(1);
    }

    Share->Clock.sec = 0;
    Share->Clock.nsec = 0;

    MsgID = msgget(MESSAGEKEY, 0666 | IPC_CREAT);
    sprintf(msg.mtext, "This is a test message.\n"); //(sprintf to write the message)
    msg.mtype = 1;
    msgsnd(MsgID, &msg, sizeof(msg), 0);
    if(fork() == 0)
    {
        execl("./user", "./user", "1");
    }
    pid_t wpid;
    int status = 0;
    while((wpid = wait(&status)) > 0);
    printf("OSS terminating\n");
    shmdt(Share);
    shmctl(ShareID, IPC_RMID, NULL);
    msgctl(MsgID, IPC_RMID, NULL);
    return 0;
}