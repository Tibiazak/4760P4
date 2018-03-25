
// Interrupt handler for SIGUSR1, detaches shared memory and exits cleanly.
static void interrupt()
{
    printf("Received interrupt!\n");
    shmdt(Clock);
    exit(1);
}

int main(int argc, char *argv[]) {
    int timeslice;
    if(argc == 1)
    {
        printf("Error! Need to pass the pid!\n");
        return 1;
    }
    int pid = atoi(argv[1]);
    signal(SIGUSR1, interrupt); // registers interrupt handler


}