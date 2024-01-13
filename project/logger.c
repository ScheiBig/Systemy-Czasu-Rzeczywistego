#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <unistd.h>

#include "logger.h"

/** @brief Signal used for LOG level change */
#define SIGLOG ( SIGRTMIN )
/** @brief Signal used for DUMP ordering */
#define SIGDUMP ( SIGRTMIN + 1 )

/** @brief Temporary placeholder for `errno` in throwing blocks */
int __errno;

/** @brief Path that LOG and DUMP files are created in */
const char* __path;
/** @brief Length of `__path` (used to ignore trailing characters) */
size_t __path_len;
/** @brief `File descriptor` for current LOG file */
int __log_fd;
/** @brief Current log level - should only allow values of `log_lvl_e` */
volatile sig_atomic_t __current_log_lvl = LOG_LVL_OFF;
/** @brief `File descriptor`s for pipe queue used by DUMP */
struct
{
    int read;
    int write;
} __dump_pipe;
/** @brief DUMP thread identifier */
pthread_t __dump_tid;


/** @brief Whether `pmap` command is present in system */
bool __pmap_present;
/** @brief Whether library is already initialized */
bool __init_ready = false;
/** @brief Mutex protecting from duplicate `init` / `deinit` calls */
pthread_mutex_t __init_mux = PTHREAD_MUTEX_INITIALIZER;
/** @brief Mutex protecting from synchronous writes to LOG file */
pthread_mutex_t __write_mux = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Create a file name of template:
 *        pid${PID}_${YYYY-MM-DD-HH.mm.ss}.${_extension}
 * @return Dynamically-allocated path
 */
char* __create_file_name(const char* _extension);
/**
 * @brief If `_lock == true`, then lock file `_fd`, else unlock it
 */
log_res_e __change_file_lock(int _fd, bool _lock);
/**
 * @brief Send rt signal `_signal` with `_value` to process `_pid`
 */
log_res_e __dispatch_signal(pid_t _pid, int _signal, int _value);
/**
 * @brief Registers signal action `_action` for `_signal`, or if `_action` is NULL, restores default
 */
log_res_e __register_signal(int _signal, void (*_action) (int, siginfo_t*, void*));

/**
 * @brief `Runnable` for creating DUMP files
 */
void* __dump_thread(void* _);
/**
 * @brief Signal action for changing LOG level
 */
void __log_action(int _signal, siginfo_t* _info, void* _);
/**
 * @brief Signal action for ordering DUMP files
 */
void __dump_action(int _signal, siginfo_t* _info, void* _);


//============================================================================//


log_res_e log_init(const char* _path)
{
    if (EBUSY == pthread_mutex_trylock(&__init_mux)
        || __init_ready == true)
    {
        return LOG_RES_ERROR_DUP;
    }

    if (_path == NULL) { __path = "."; }
    else
    {
        // Check if path exists
        DIR* path_dir = opendir(_path);
        if (path_dir) { closedir(path_dir); }
        else { return LOG_RES_ERROR_ARG; }
        __path = _path;
    }

    size_t path_len = strlen(__path);
    // Check for trailing folder separator
    if (__path[path_len - 1] == '/') { --path_len; }
    __path_len = path_len; 
  
    // Create LOG file
    char* log_path = __create_file_name("LOG");
    __log_fd = open(log_path, O_CREAT | O_RDWR,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_ISGID);
    __errno = errno;
    free(log_path);
    if (__log_fd < 0)
    {
        errno = __errno;
        return LOG_RES_ERROR_FILE;
    }
 
    // Check if mandatory locking is enabled
    // if not - attempt to remount filesystem to enable it
    // if remounting fails (requires elevated permissions), proceed without erroring
    struct statfs sfs;
    if (statfs("/", &sfs) == 0
        && (sfs.f_flags & MS_MANDLOCK) == 0)
    {
        if (mount("/", "/", NULL, MS_REMOUNT | MS_MANDLOCK, NULL) != 0
            && errno == EPERM)
        {
            perror(
                "\033[0;33m"
                "Cannot remount filesystem! Relaunch program using 'sudo'"
                " if you wish to place mandatory locks of LOG and DUMP files"
                "\33[0m"
            );

        }
    }

    // Check if `pmap` command is present
    __pmap_present = (system("pmap -V > /dev/null 2>&1") == 0);

    // Lock LOG file
    log_res_e ret = __change_file_lock(__log_fd, true);
    if (ret != LOG_RES_SUCCESS)
    {
        __errno = errno;
        close(__log_fd);
        errno = __errno;
        return ret;
    }

    // Create pipe (used as queue) for DUMP orders
    if (pipe((int*)&__dump_pipe) != 0)
    {
        __errno = errno;
        __change_file_lock(__log_fd, false);
        close(__log_fd);
        errno = __errno;
        return LOG_RES_ERROR_SYNC;
    }

    // Launch DUMP thread
    if ((__errno = pthread_create(&__dump_tid, NULL, __dump_thread, NULL)) != 0)
    {
        __change_file_lock(__log_fd, false);
        close(__log_fd);
        close(__dump_pipe.read);
        close(__dump_pipe.write);
        errno = __errno;
        return LOG_RES_ERROR_PTHREAD;
    }

    // Unlock signals used for communication
    sigset_t sset;
    sigemptyset(&sset);
    sigaddset(&sset, SIGLOG);
    sigaddset(&sset, SIGDUMP);
    pthread_sigmask(SIG_UNBLOCK, &sset, NULL);

    // Set signal handlers
    ret = __register_signal(SIGLOG, __log_action);
    if (ret != LOG_RES_SUCCESS)
    {
        __errno = errno;
        __change_file_lock(__log_fd, false);
        close(__log_fd);
        close(__dump_pipe.read);
        close(__dump_pipe.write);
        pthread_cancel(__dump_tid);
        errno = __errno;
        return ret;
    }
    ret = __register_signal(SIGDUMP, __dump_action);
    if (ret != LOG_RES_SUCCESS)
    {
        __errno = errno;
        __change_file_lock(__log_fd, false);
        close(__log_fd);
        close(__dump_pipe.read);
        close(__dump_pipe.write);
        pthread_cancel(__dump_tid);
        __register_signal(SIGLOG, NULL);
        errno = __errno;
        return ret;
    }

    __init_ready = true;
    __current_log_lvl = LOG_LVL_MAX;
    pthread_mutex_unlock(&__init_mux);
    return LOG_RES_SUCCESS;
}


log_res_e log_deinit(void)
{
    if (EBUSY == pthread_mutex_trylock(&__init_mux)
        || __init_ready == false)
    {
        return LOG_RES_ERROR_DUP;
    }

    // Remove signal handlers
    __register_signal(SIGLOG, NULL);
    __register_signal(SIGDUMP, NULL);

    // Cancel DUMP thread
    pthread_cancel(__dump_tid);

    // Unlock LOG file
    __change_file_lock(__log_fd, false);

    // Close LOG file
    close(__log_fd);

    // Close pipe
    close(__dump_pipe.read);
    close(__dump_pipe.write);

    __current_log_lvl = LOG_LVL_OFF;
    __init_ready = false;
    pthread_mutex_unlock(&__init_mux);
    return LOG_RES_SUCCESS;
}


log_res_e log_printf(log_lvl_e _lvl, const char* _format, ...)
{
    if (LOG_LVL_MIN > _lvl || _lvl > LOG_LVL_MAX) { return LOG_RES_ERROR_OTHER; }
    if (__current_log_lvl == LOG_LVL_OFF)
    {
        return LOG_RES_OFF;
    }
    if (__current_log_lvl < _lvl)
    {
        return LOG_RES_IGNORED;
    }
    if (pthread_mutex_lock(&__write_mux) != 0
        || __init_ready == false)
    {
        return LOG_RES_ERROR_DUP;
    }
    
    char head[29] = { 0 };
    switch (_lvl)
    {
    case LOG_LVL_MIN:
        sprintf(head, "[MIN @ ");
        break;
    case LOG_LVL_STANDARD:
        sprintf(head, "[STD @ ");
        break;
    case LOG_LVL_MAX:
        sprintf(head, "[MAX @ ");
        break;
    default: break;
    }
    time_t now = time(NULL);
    struct tm* now_info = localtime(&now);
    strftime(head + 7, 22, "%Y-%m-%d|%H:%M:%S] ", now_info);
    dprintf(__log_fd, "%s", head);

    va_list format_args;
    va_start(format_args, _format);
    int ret = vdprintf(__log_fd, _format, format_args);
    va_end(format_args);

    dprintf(__log_fd, "\n");
    fsync(__log_fd);
    pthread_mutex_unlock(&__write_mux);
    
    return ret >= 0 ? LOG_RES_SUCCESS : LOG_RES_ERROR_OTHER;
}


log_res_e log_dispatch_log_level(pid_t _pid, log_lvl_e _lvl)
{
    pid_t pid;
    if (_pid == 0) { pid = getpid(); }
    else { pid = _pid; }

    if (_lvl != LOG_LVL_OFF
        && _lvl != LOG_LVL_MIN
        && _lvl != LOG_LVL_STANDARD
        && _lvl != LOG_LVL_MAX)
    {
        return LOG_RES_ERROR_ARG;
    }

    return __dispatch_signal(pid, SIGLOG, _lvl);
}


log_res_e log_dispatch_log_dump(pid_t _pid, log_dump_lvl_e _lvl)
{
    pid_t pid;
    if (_pid == 0) { pid = getpid(); }
    else { pid = _pid; }

    if (_lvl != LOG_DUMP_LVL_NORMAL
        && _lvl != LOG_DUMP_LVL_DETAIL
        && _lvl != LOG_DUMP_LVL_EXTENDED
        && _lvl != LOG_DUMP_LVL_FULL)
    {
        return LOG_RES_ERROR_ARG;
    }

    return __dispatch_signal(pid, SIGDUMP, _lvl);
}


//============================================================================//


char* __create_file_name(const char* _extension)
{
    const size_t path_max_len =
        __path_len + 1 // "${_path }/"
        + 3 + sizeof(intmax_t) * 8 + 1 // "pid${ PID }_"
        + 20 + 1 + strlen(_extension) // "${YYYY-MM-DD-HH.mm.ss}.${extension}"
        + 1 // null-string-terminator
        ;
    size_t path_len = __path_len;

    char* path = (char*)calloc(path_max_len, sizeof(char));

    strncpy(path, __path, path_len);
    path[path_len] = '/';
    sprintf(path + path_len + 1, "pid%" PRIdMAX "_", (intmax_t)getpid());
    path_len = strlen(path);
    time_t now = time(NULL);
    struct tm* now_info = localtime(&now);
    strftime(path + path_len, 21, "%Y-%m-%d-%H.%M.%S.", now_info);
    path_len = strlen(path);
    strncpy(path + path_len, _extension, 8);

    return path;
}
 

log_res_e __change_file_lock(int _fd, bool _lock)
{
    short type;
    if (_lock == 1) { type = F_WRLCK; }
    else { type = F_UNLCK; }

    struct flock log_lock = {
        .l_type = type,
        .l_whence = SEEK_SET,
        .l_start = 0,
        .l_len = 0,
        .l_pid = getpid()
    };
    if (fcntl(_fd, F_SETLK, &log_lock) < 0) { return LOG_RES_ERROR_SYNC; };
    return LOG_RES_SUCCESS;
}


log_res_e __dispatch_signal(pid_t _pid, int _signal, int _value)
{
    union sigval value = { .sival_int = _value };
    if (sigqueue(_pid, _signal, value) != 0) { return LOG_RES_ERROR_SYNC; }
    return LOG_RES_SUCCESS;
}


log_res_e __register_signal(int _signal, void (*_action) (int, siginfo_t*, void*))
{
    sigset_t all;
    if (_action != NULL)
    {
        sigfillset(&all);

        struct sigaction action_config = {
            .sa_sigaction = _action,
            .sa_mask = all,
            .sa_flags = SA_SIGINFO,
        };
        if (sigaction(_signal, &action_config, NULL) != 0) { return LOG_RES_ERROR_SYNC; }
    }
    else
    {
        sigemptyset(&all);

        struct sigaction action_config = {
            .sa_handler = SIG_DFL,
            .sa_mask = all,
            .sa_flags = 0,
        };
        if (sigaction(_signal, &action_config, NULL) != 0) { return LOG_RES_ERROR_SYNC; }
    }
    return LOG_RES_SUCCESS;
}


void* __dump_thread(void* _)
{
    char buf;
    int ret;
    while (read(__dump_pipe.read, &buf, 1) > 0)
    {
        char* dump_path = __create_file_name("DUMP");
        char* command;
        if (__pmap_present)
        {
            command = (char*)calloc(
                9 // "pmap " + 3 max for args + " "
                + sizeof(intmax_t) * 8 // PID
                + 3 // " > "
                + strlen(dump_path),
                sizeof(char)
            );
            sprintf(command, "pmap     %" PRIdMAX " > %s", (intmax_t)getpid(), dump_path);
            free(dump_path);
            switch (buf)
            {
            case LOG_DUMP_LVL_NORMAL:
                strncpy(command + 5, "", 0);
                break;
            case LOG_DUMP_LVL_DETAIL:
                strncpy(command + 5, "-x", 2);
                break;
            case LOG_DUMP_LVL_EXTENDED:
                strncpy(command + 5, "-X", 2);
                break;
            case LOG_DUMP_LVL_FULL:
                strncpy(command + 5, "-XX", 3);
                break;
            default:
                free(command);
                log_printf(LOG_LVL_MIN, "Failed to create DUMP file using `pmap`: invalid DUMP_LVL [%d]", (int)buf);
                continue;
            }
            
            if ((ret = system(command)) != 0)
            {
                log_printf(LOG_LVL_MIN, "Failed to create DUMP file using `pmap`: command returned code [%d]", ret);
            }
            else
            {
                log_printf(LOG_LVL_MIN, "Created DUMP file using command: `%*s`", 8, command);
            }
        }
        else
        {
            command = (char*)calloc(
                10 // "cat /proc/"
                + sizeof(intmax_t) * 8 // PID
                + 8 // "/maps > "
                + strlen(dump_path),
                sizeof(char)
            );
            sprintf(command, "cat /proc/%" PRIdMAX "/maps > %s", (intmax_t)getpid(), dump_path);
            free(dump_path);

            if ((ret = system(command)) != 0)
            {
                log_printf(LOG_LVL_MIN, "Failed to create DUMP file using `cat /proc/[PID]/maps`: command returned code [%d]", ret);
            }
            else
            {
                log_printf(LOG_LVL_MIN, "Created DUMP file using command: `cat /proc/[PID]/maps`");
            }
        }
        free(command);
    }
    return NULL;
}


void __log_action(int _signal, siginfo_t* _info, void* _)
{
    int value = _info->si_value.sival_int;
    if (value != LOG_LVL_OFF
        && value != LOG_LVL_MIN
        && value != LOG_LVL_STANDARD
        && value != LOG_LVL_MAX)
    {
        return;
    }
    __current_log_lvl = value;
}


void __dump_action(int _signal, siginfo_t* _info, void* _)
{
    int value = _info->si_value.sival_int;
    if (value != LOG_DUMP_LVL_NORMAL
        && value != LOG_DUMP_LVL_DETAIL
        && value != LOG_DUMP_LVL_EXTENDED
        && value != LOG_DUMP_LVL_FULL)
    {
        return;
    }
    char msg = (char)value;
    write(__dump_pipe.write, &msg, 1);
}
