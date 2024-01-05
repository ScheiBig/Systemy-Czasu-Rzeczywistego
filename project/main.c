#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <signal.h>

#include "logger.h"

int main()
{
    printf("Running program with pid: %ld\n", (long)getpid());

    sigset_t mask;
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    log_init(NULL);

    for (int i = 0; i < 60; i++)
    {
        printf("Printing: %d\n", i);
        log_printf(LOG_LVL_MIN, "Minimal message [%d]", i);
        log_printf(LOG_LVL_STANDARD, "Standard message [%d]", i);
        log_printf(LOG_LVL_MAX, "Maximal message [%d]", i);

        sleep(1);
    }

    log_deinit();
    printf("Waiting...");

    getchar();

    return 0;
}
