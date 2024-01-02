#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

typedef const char* cstr_t;
typedef char* str_t;
#define null 0


#define SIG_1 (SIGRTMIN)
#define SIG_2 (SIGRTMIN + 1)


int main_receiver(void);
int main_sender(int argc, cstr_t* argv);

void* thread_listener(void* args);

int usage_r(cstr_t name);
int usage_s(cstr_t name);

int main(int argc, cstr_t* argv)
{
    // Check, if program type is specified
    if (argc < 2)
    {
        fprintf(stderr, "Please specify program type\n");
        usage_r(argv[0]);
        usage_s(argv[0]);
        return EXIT_FAILURE;
    }

    // Parse program type, delegate further execution to auxilary function
    if (strcmp(argv[1], "send") == 0)
    {
        return main_sender(argc, argv);
    }
    else if (strcmp(argv[1], "receive") == 0)
    {
        return main_receiver();
    }
    else
    {
        fprintf(stderr, "Unknown option %s\n", argv[2]);
        usage_r(argv[0]);
        usage_s(argv[0]);
        return EXIT_FAILURE;
    }
}

int main_receiver(void)
{
    printf("Program is running on process [%d]\n", getpid());
    pthread_t t1;
    pthread_t t2;
    int s1 = SIG_1;
    int s2 = SIG_2;

    sigset_t set;
    sigfillset(&set);
    sigdelset(&set, SIGINT);
    if ((errno = pthread_sigmask(SIG_SETMASK, &set, null)) != 0)
    {
        perror("Error while masking signals in parent thread\n");
        exit(EXIT_FAILURE);
    }

    if ((errno = pthread_create(&t1, null, thread_listener, (void*)&s1)) != 0)
    {
        perror("Error while creating first thread\n");
        exit(EXIT_FAILURE);
    }

    printf("Thread 1 [%ld] is listening for signal [%d]\n", t1, SIG_1);

    if ((errno = pthread_create(&t2, null, thread_listener, (void*)&s2)) != 0)
    {
        perror("Error while creating second thread\n");
        pthread_cancel(t1);
        exit(EXIT_FAILURE);
    }

    printf("Thread 2 [%ld] is listening for signal [%d]\n", t2, SIG_2);

    pthread_join(t1, null);
    pthread_join(t2, null);

    return EXIT_SUCCESS;
}

/// @brief Program delegate for sender mode
int main_sender(int argc, cstr_t* argv)
{
    // Check if number of passed arguments is correct
    if (argc != 5)
    {
        fprintf(stderr, "Wrong number of arguments\n");
        usage_s(argv[0]);
        return EXIT_FAILURE;
    }

    // Parse [PID]
    str_t end = null;
    long pid = strtol(argv[2], &end, 10);
    // Check if [signal_id] is a number
    if (pid == 0 && end != null)
    {
        fprintf(stderr, "[PID] is not a number\n");
        return EXIT_FAILURE;
    }

    // Parse [signal_id]
    end = null;
    long signal_id = strtol(argv[3], &end, 10);
    // Check if [signal_id] is a number
    if (signal_id == 0 && end != null)
    {
        fprintf(stderr, "[signal_id] is not a number\n");
        return EXIT_FAILURE;
    }
    // Check if [signal_id] is in bounds of supported signals
    else if (signal_id != SIG_1 && signal_id != SIG_2)
    {
        fprintf(stderr, "[signal_id] is invalid: allowed signals are [%d, %d]\n", SIG_1, SIG_2);
        return EXIT_FAILURE;
    }

    // Parse [value] 
    end = null;
    long value = strtol(argv[4], &end, 10);
    // Check if [signal_id] is a number
    if (value == 0 && end != null)
    {
        fprintf(stderr, "[value] is not a number\n");
        return EXIT_FAILURE;
    }

    union sigval val = { .sival_int = value };

    sigqueue(pid, signal_id, val);

    return EXIT_SUCCESS;
}

/// @brief Runnable that waits for one of specified signals, and returns after accepting it
/// @param args actual: sigset_t with signals to wait for
/// @return actual: void
void* thread_listener(void* args)
{
    int arg = *(int*)args;
    sigset_t set;
    sigfillset(&set);
    sigdelset(&set, SIGINT);

    if ((errno = pthread_sigmask(SIG_SETMASK, &set, null)) != 0)
    {
        perror("Error while masking signals in thread runnable\n");
        exit(EXIT_FAILURE);
    }

    sigemptyset(&set);
    sigaddset(&set, arg);
    siginfo_t info;
    // for (;;)
    {
        sigwaitinfo(&set, &info);
        printf(
            "Thread [%ld] accepted signal [%d] with associated value [%d]\n",
            pthread_self(),
            info.si_signo,
            info.si_value.sival_int
        );
    }

    return EXIT_SUCCESS;
}

/// @brief Prints command usage for receiver mode
/// @param name Name that is used for this program - usually `argv[0]`
int usage_r(cstr_t name) { return printf("usage: %s receive\n", name); }

/// @brief Prints command usage for sender mode
/// @param name Name that is used for this program - usually `argv[0]`
int usage_s(cstr_t name) { return printf("usage: %s send [PID] [signal_id] [value]\n", name); }
