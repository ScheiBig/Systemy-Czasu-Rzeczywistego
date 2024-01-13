#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <signal.h>
#include <unistd.h>

#include "logger.h"

volatile sig_atomic_t is_running = false;

void finalize_handler(int _signal);

int main(int argc, char const* argv[])
{
    printf("Running program with pid: %ld\n", (long)getpid());

    sigset_t mask;
    sigfillset(&mask);
    struct sigaction sa = {
        .sa_handler = finalize_handler
        , .sa_mask = mask
    };
    sigaction(SIGINT, &sa, NULL);

    sigdelset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, NULL);


    log_init(NULL);

    is_running = true;

    for (long f_2 = 0, f_1 = 1, f_c = f_1 + f_2;
        is_running;
        (f_2 = f_1), (f_1 = f_c), (f_c = f_1 + f_2))
    {
        printf("Fibonacci iteration: %ld + %ld = %ld\n", f_2, f_1, f_c);

        log_printf(LOG_LVL_MIN, "f current  = %ld", f_c);
        log_printf(LOG_LVL_STANDARD, "f[-1] = %ld", f_1);
        log_printf(LOG_LVL_MAX, "f[-2] = %ld", f_2);

        sleep(3);
    }

    log_deinit();
    
    printf("\nFinished terminating program. Waiting for input...");
    getchar();

    return 0;
}

void finalize_handler(int _signal)
{
    is_running = false;
}
