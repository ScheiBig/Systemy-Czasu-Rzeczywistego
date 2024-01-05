#pragma once
#include <unistd.h>

typedef enum
{
    /** @brief Logging is turned off. Cannot pass as argument to `log_printf` */
    LOG_LVL_OFF = -1,
    /** @brief Lowest detail level - prints always if logging is currently on */
    LOG_LVL_MIN,
    /** @brief Standard detail level - prints if level is set to STANDARD or MAX */
    LOG_LVL_STANDARD,
    /** @brief Largest detail level - prints only if level is set to MAX */
    LOG_LVL_MAX,
} log_lvl_e;

typedef enum
{
    /** @brief Basic DUMP (no options to `pmap`, or `cat /proc/[PID]/maps`
     *        if `pmap` is not available) */
    LOG_DUMP_LVL_NORMAL,
    /** @brief Detailed DUMP (`pmap -x`, defaults to `LOG_DUMP_LVL_NORMAL`
     *        if `pmap` is not available) */
    LOG_DUMP_LVL_DETAIL,
    /** @brief Extended DUMP (`pmap -X`, defaults to `LOG_DUMP_LVL_NORMAL`
     *        if `pmap` is not available) */
    LOG_DUMP_LVL_EXTENDED,
    /** @brief Full Kernel DUMP (`pmap -XX`, defaults to `LOG_DUMP_LVL_NORMAL`
     *        if `pmap` is not available) */
    LOG_DUMP_LVL_FULL,
} log_dump_lvl_e;

typedef enum
{
    /** @brief Message was discarded due to logging being off */
    LOG_RES_OFF = -2,
    /** @brief Message was discarded due to level higher than current */
    LOG_RES_IGNORED = -1,
    /** @brief "All correct" */
    LOG_RES_SUCCESS = 0,
    /** @brief Duplicate call to `init` or `deinit` */
    LOG_RES_ERROR_DUP,
    /** @brief Error related to passed arguments */
    LOG_RES_ERROR_ARG,
    /** @brief Error related to file IO */
    LOG_RES_ERROR_FILE,
    /** @brief Error related to multithread / multiprocess synchronization */
    LOG_RES_ERROR_SYNC,
    /** @brief Error related to PThreads */
    LOG_RES_ERROR_PTHREAD,
    /** @brief Error not related to other descriptions */
    LOG_RES_ERROR_OTHER = 255,
} log_res_e;


/**
 * @brief Initializes resources for logging
 *
 * @param _path Path used to store LOG files - if `NULL`,
 *        then path in which program resides is used.
 *
 * @return LOG_RES_SUCCESS - everything OK
 * @return LOG_RES_ERROR_DUP - already initialized, or in process of initializing already
 * @return LOG_RES_ERROR_ARG - _path does not point to existing directory
 * @return LOG_RES_ERROR_FILE - cannot create file in _path
 * @return LOG_RES_ERROR_SYNC - cannot place lock on LOG file
 * @return LOG_RES_ERROR_PTHREAD - cannot create thread for DUMP
 */
log_res_e log_init(const char* _path);

/**
 * @brief Cleans resources for logging
 *
 * @return LOG_RES_SUCCESS - everything OK
 * @return LOG_RES_ERROR_DUP - already initialized, or in process of initializing already
 */
log_res_e log_deinit(void);

/**
 * @brief Prints formatted message to LOG file
 *
 * @param _lvl Level of logged message - if level is lower than current logging
 *        level, (or logging is off) then message is discarded
 * @param _format Format sting of message. For consistency, don't use newline characters!
 * @param ... Arguments to formatted string
 *
 * @return LOG_RES_SUCCESS - everything OK
 * @return LOG_RES_ERROR_FILE - error while writing to file
 * @return LOG_RES_ERROR_SYNC - error while (un)locking mutex
 * @return LOG_RES_ERROR_OTHER - error while printing / formatting message
 */
log_res_e log_printf(log_lvl_e _lvl, const char* _format, ...)
__attribute__((format(printf, 2, 3)));


/**
 * @brief Changes logging level, by sending signal using sigqueue
 * 
 * @param _pid ID of process that should have logging level changed - if 0,
 *        then current process receives signal
 * @param _lvl New logging level
 *
 * @return LOG_RES_SUCCESS - everything OK
 * @return LOG_RES_ERROR_ARG - _lvl is invalid
 * @return LOG_RES_ERROR_SYNC - cannot send signal
 */
log_res_e log_dispatch_log_level(pid_t _pid, log_lvl_e _lvl);

/**
 * @brief Dumps process memory map, by sending signal using sigqueue
 *
 * @param _pid ID of process that should have memory dumped - if 0,
 *        then current process receives signal
 * @param _lvl Level of DUMP detail
 *
 * @return LOG_RES_SUCCESS - everything OK
 * @return LOG_RES_ERROR_ARG - _lvl is invalid
 * @return LOG_RES_ERROR_SYNC - cannot send signal
 */
log_res_e log_dispatch_log_dump(pid_t _pid, log_dump_lvl_e _lvl);
