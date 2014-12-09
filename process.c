#include <sys/prctl.h>
#include <stdarg.h>
#include <stdio.h>

char *argv0;

void setname (char *format, ...) {
    va_list args;
    va_start (args, format);
    vsprintf (argv0, format, args);
    va_end (args);
    prctl (PR_SET_NAME, argv0);
}
