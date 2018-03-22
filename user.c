
// Interrupt handler for SIGUSR1, detaches shared memory and exits cleanly.
static void interrupt()
{
    printf("Received interrupt!\n");
    shmdt(Clock);
    exit(1);
}

int main(int argc, char *argv[]) {
    signal(SIGUSR1, interrupt); // registers interrupt handler
}