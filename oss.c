/*
 * Joshua Bearden
 * Operating Systems
 * 3/25/18
 *
 * A program to simulate an operating system scheduler. This program is currently incomplete.
 *
 */
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
#include <string.h>

#define TIMEOUT 3
#define SHAREKEY 92195
#define MESSAGEKEY 110992
#define BILLION 1000000000
#define PROC_LIMIT 18
#define MaxTimeBetweenNewProcsSecs 1
#define MaxTimeBetweeNewProcsNS 500000000
#define RealTimeProcs 10
#define TIMESLICE0 "1000000"
#define TIMESLICE1 "2000000"
#define TIMESLICE2 "4000000"
#define TIMESLICE3 "8000000"

typedef unsigned int uint;

typedef struct sysclock {
    uint sec;
    uint nsec;
} sysclock;

typedef struct pcb {
    int simpid;
    int priority;
    int cpu_used;
    sysclock launch_time;
    sysclock time_in_system;
    sysclock last_burst_time;
} pcb;

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
message msg;
int queue0[PROC_LIMIT];
int queue1[PROC_LIMIT];
int queue2[PROC_LIMIT];
int queue3[PROC_LIMIT];
int bit_array[PROC_LIMIT];
int blockQueue[PROC_LIMIT];
int CurrentChild;
sysclock event[PROC_LIMIT];

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

// A function that finds the first open spot in the given array and returns its index
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

// A function that sets every element in an int array to 0.
void zeroarray(int * arr, int limit)
{
    int i;
    for (i = 0; i < limit; i++)
    {
        arr[i] = 0;
    }
}

// A function that simulates a queue using a C array
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

// Starts the real-time alarm
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
    return 0;
}

// Returns the next time to launch a process
sysclock getNextLaunchTime()
{
    sysclock time;
    if ((Share->Clock.nsec + 500000000) > BILLION)
    {
        time.sec = Share->Clock.sec + 1;
        time.nsec = 250000000;
    }
    else
    {
        time.sec = Share->Clock.sec;
        time.nsec = Share->Clock.nsec + 500000000;
    }
    return time;
}

// Calculates the time elapsed between two clock times
sysclock calculateTimeElapsed(sysclock a, sysclock b)
{
    sysclock c = {0, 0};
    // assumes A - B
    if(a.sec < b.sec) // if B > A
    {
        return c;
    }
    if(a.nsec > b.nsec)
    {
        c.nsec = a.nsec - b.nsec;
    }
    else
    {
        if(a.sec == b.sec) // if B > A
        {
            return c;
        }
        c.nsec = b.nsec - a.nsec;
        a.sec -= 1;
    }
    c.sec = a.sec - b.sec;
    return c;
}

// Determines whether a function is real-time or not
int getPriority()
{
    // needs to actually randomly decide whether real time or not
    return 1;
}

// Fork & exec a child with a given simulated pid
void forkchild(int childpid)
{
    if(fork() == 0)
    {
        execl("./user", "./user", childpid, NULL);
        perror("Exec failed!\n");
        exit(1);
    }
}

void addqueue(int * queue, int pid)
{
    int i;
    for(i = 0; i < PROC_LIMIT; i++)
    {
        if(queue[i] == 0)
        {
            queue[i] = pid;
            return;
        }
    }
}

int getNextProcess()
{
    if (queue0[0] != 0)
    {
        return queue0[0];
    }
    else if (queue1[0] != 0)
    {
        return queue1[0];
    }
    else if (queue2[0] != 0)
    {
        return queue2[0];
    }
    else
    {
        return queue3[0];
    }
}


// Makes a new process by initializing the PCB and forking the child
void makeProcess(int childpid)
{
    Share->pcb_array[childpid-1].simpid = childpid;
    Share->pcb_array[childpid-1].priority = getPriority();
    if(Share->pcb_array[childpid-1].priority == 0)
    {
        addqueue(queue0, childpid);
    }
    else
    {
        addqueue(queue1, childpid);
    }
    Share->pcb_array[childpid-1].cpu_used = 0;
    Share->pcb_array[childpid-1].last_burst_time.sec = 0;
    Share->pcb_array[childpid-1].last_burst_time.nsec = 0;
    Share->pcb_array[childpid-1].time_in_system.sec = 0;
    Share->pcb_array[childpid-1].time_in_system.nsec = 0;
    Share->pcb_array[childpid-1].launch_time.sec = Share->Clock.sec;
    Share->pcb_array[childpid-1].launch_time.nsec = Share->Clock.nsec;
    bit_array[childpid-1] = 1;
    forkchild(childpid);
}

// Returns true if there is a process running, false otherwise
int procsRunning(int *arr)
{
    int i;
    for(i = 0; i < PROC_LIMIT; i++)
    {
        if (arr[i])
            return 1;
    }
    return 0;
}

void checkBlock()
{
    return;
}

void blocked()
{
    return;
}

void terminate()
{
    return;
}

int isTimeToLaunch()
{
    if (Share->Clock.sec > nextLaunch.sec)
    {
        return 1;
    }
    else if ((Share->Clock.sec == nextLaunch.sec) && (Share->Clock.nsec > nextLaunch.nsec))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void launchProc()
{
    int index;
    if ((index = findopen(bit_array)) != -1)
    {
        makeProcess(index + 1);
    }
}

// Actual scheduler algorithm
void scheduler()
{
    checkBlock(); // Checks the blocked queue
    if (isTimeToLaunch()) // Checks if its time to launch a new process
    {
        launchProc();
    }

    // Gets the pid of the next process to run
    CurrentChild = getNextProcess();
    message msg;

    // prepares the message - mtype is pid of child, message is timeslice its given
    msg.mtype = CurrentChild;
    int qNum;
    qNum = Share->pcb_array[CurrentChild-1].priority;
    switch(qNum)
    {
        case 0:
            sprintf(msg.mtext, TIMESLICE0);
            break;
        case 1:
            sprintf(msg.mtext, TIMESLICE1);
            break;
        case 2:
            sprintf(msg.mtext, TIMESLICE2);
            break;
        case 3:
            sprintf(msg.mtext, TIMESLICE3);
            break;
    }
    msgsnd(MsgID, &msg, sizeof(msg), 0); // sends message
}

// receives and interprets messages received from the child processes
void checkMsg(message msg)
{
    // get message and get the first argument of the message text (a flag stating what happened)
    msgrcv(MsgID, &msg, sizeof(msg), 0, 0);
    char messageString[100];
    strcpy(messageString, msg.mtext);
    char * temp;
    temp = strtok(messageString, " ");
    int flag = atoi(temp);
    temp = strtok(NULL, " ");
    int slice = atoi(temp);
    if((Share->Clock.nsec + slice) >= BILLION)
    {
        Share->Clock.sec++;
        Share->Clock.nsec = (Share->Clock.nsec + slice) - BILLION;
    }
    else
    {
        Share->Clock.nsec += slice;
    }
    // based on the results of the flag, perform an action
    switch(flag)
    {
        case 0:
            // timeslice used, do nothing
            break;
        case 1:
            // terminating, call function to handle termination
            terminate();
            break;
        case 2:
            // blocked, get the "event" its blocked on and call function to handle blocking
            temp = strtok(NULL, " ");
            event[CurrentChild-1].sec = atoi(temp);
            temp = strtok(NULL, " ");
            event[CurrentChild-1].nsec = atoi(temp);
            blocked();
            break;
    }
    // Now that the current process has finished, call the scheduler again.
    scheduler();
}



int main(int argc, char *argv[]){
    uint idlesec = 0;
    uint idlensec = 0;
    zeroarray(queue0, PROC_LIMIT);
    zeroarray(queue1, PROC_LIMIT);
    zeroarray(queue2, PROC_LIMIT);
    zeroarray(queue3, PROC_LIMIT);
    zeroarray(bit_array, PROC_LIMIT);
    zeroarray(blockQueue, PROC_LIMIT);

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

    // initialize clock
    Share->Clock.sec = 0;
    Share->Clock.nsec = 0;

    // get message queue
    MsgID = msgget(MESSAGEKEY, 0666 | IPC_CREAT);

    // test message
    sprintf(msg.mtext, "This is a test message.\n"); //(sprintf to write the message)
    msg.mtype = 1;
    msgsnd(MsgID, &msg, sizeof(msg), 0);


    // waits for child processes to finish
    pid_t wpid;
    int status = 0;
    while((wpid = wait(&status)) > 0);

    // signal termination
    printf("OSS terminating\n");
    shmdt(Share);
    shmctl(ShareID, IPC_RMID, NULL);
    msgctl(MsgID, IPC_RMID, NULL);
    return 0;
}