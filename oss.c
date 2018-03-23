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
    pcb pcb_array[18];
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

int main(int argc, char *argv[]){
    message msg;

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
    int proc_table[PROC_LIMIT];

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

    Share->Clock.sec = 5;
    Share->Clock.nsec = 5;

    MsgID = msgget(MESSAGEKEY, 0666 | IPC_CREAT);

    printf("The clock is at %u seconds and %u nanoseconds.\n", Share->Clock->sec, Share->Clock->nsec);

    shmdt(Share);
    shmctl(ShareID, IPC_RMID, NULL);
    msgctl(MsgID, IPC_RMID, NULL);
    return 0;
}