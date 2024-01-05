#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "logger.h"

void usage(const char* _cmd);

int main(int argc, char const *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr,
            "\033[0;31m"
            "Argument number too low!\n\n"
            "\33[0m");
        usage(argv[0]);
        return 1;
    }

    long pid;
    errno = 0;
    pid = strtol(argv[argc - 1], NULL, 10);
    if (errno != 0 || pid == 0)
    {
        fprintf(stderr,
            "\033[0;31m"
            "Invalid pid number: %s!\n\n"
            "\33[0m",
            argv[argc - 1]);
        usage(argv[0]);
        return 1;
    }
    char cmd[128] = { 0 };
    sprintf(cmd, "ls /proc/%ld > /dev/null 2>&1", pid);
    if (system(cmd) != 0)
    {
        fprintf(stderr, "\033[0;31m" 
            "No process with pid number: %ld!\n\n"
            "\33[0m",
            pid);
        usage(argv[0]);
        return 1;
    }


    if (strcmp(argv[1], "log_lvl") == 0)
    {
        if (argc < 4)
        {
            fprintf(stderr,
                "\033[0;31m"
                "Argument number too low!\n\n"
                "\33[0m");
            usage(argv[0]);
            return 1;
        }

        if (strcmp(argv[2], "off") == 0)
        {
            log_dispatch_log_level(pid, LOG_LVL_OFF);
            return 0;
        }
        if (strcmp(argv[2], "min") == 0)
        {
            log_dispatch_log_level(pid, LOG_LVL_MIN);
            return 0;
        }
        if (strcmp(argv[2], "std") == 0)
        {
            log_dispatch_log_level(pid, LOG_LVL_STANDARD);
            return 0;
        }
        if (strcmp(argv[2], "max") == 0)
        {
            log_dispatch_log_level(pid, LOG_LVL_MAX);
            return 0;
        }

        fprintf(stderr,
            "\033[0;31m"
            "Unknown value for log_lvl: %s\n\n"
            "\33[0m",
            argv[2]);
        usage(argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "dump_ord") == 0)
    {
        if (argc == 3 || strcmp(argv[2], "norm") == 0)
        {
            log_dispatch_log_dump(pid, LOG_DUMP_LVL_NORMAL);
            return 0;
        }
        if (strcmp(argv[2], "detl") == 0)
        {
            log_dispatch_log_dump(pid, LOG_DUMP_LVL_DETAIL);
            return 0;
        }
        if (strcmp(argv[2], "extd") == 0)
        {
            log_dispatch_log_dump(pid, LOG_DUMP_LVL_EXTENDED);
            return 0;
        }
        if (strcmp(argv[2], "full") == 0)
        {
            log_dispatch_log_dump(pid, LOG_DUMP_LVL_FULL);
            return 0;
        }

        fprintf(stderr,
            "\033[0;31m"
            "Unknown value for dump_ord: %s\n\n"
            "\33[0m"
            , argv[2]);
        usage(argv[0]);
        return 1;
    }
    fprintf(stderr,
        "\033[0;31m"
        "Unknown command: %s!\n\n"
        "\33[0m"
        , argv[1]);
    usage(argv[0]);

    return 1;
}

void usage(const char* _cmd)
{
    fprintf(stderr, "Usage %s <command> <arg> <pid>\n", _cmd);
    fprintf(stderr, "------------------------\n");
    fprintf(stderr, "Commands are:\n"
        "    log_lvl\n"
        "    dump_order\n"
    );
    fprintf(stderr, "For log_lvl, args are:\n"
        "    off\n"
        "    min\n"
        "    std\n"
        "    max\n"
    );
    fprintf(stderr, "For dump_ord, args are:\n"
        "    [norm] (can be omitted)\n"
        "    detl\n"
        "    extd\n"
        "    full\n"
    );
}
