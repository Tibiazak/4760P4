
#define TIMEOUT 3
// A function that catches SIGINT and SIGALRM
// It prints an alert to the screen then sends a signal to all the child processes to terminate,
// frees all the shared memory and closes the file.
static void interrupt(int signo, siginfo_t *info, void *context)
{
    int errsave;

    errsave = errno;
    write(STDOUT_FILENO, TIMER_MSG, sizeof(TIMER_MSG) - 1);
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