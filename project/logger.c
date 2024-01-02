#include <stdarg.h>
#include <stdio.h>
#include "logger.h"

log_res_e log_printf(log_lvl_e lvl, const char* format, ...)
{
    FILE* my_file = (FILE*)(void*)420;


    va_list format_args;
    va_start(format_args, format);
    int ret = vfprintf(my_file, format, format_args);
    va_end(format_args);

    return ret == 0 ? LOG_RES_SUCCESS : LOG_RES_ERROR_OTHER;
}
