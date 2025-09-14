#include "time_ops.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

double time_now_c() {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

char* time_format_c(double timestamp, const char* format) {
    time_t t = (time_t)(timestamp / 1000.0);
    struct tm* tm_info = localtime(&t);
    char* buffer = (char*)malloc(256);
    strftime(buffer, 256, format, tm_info);
    return buffer;
}

void time_sleep_c(int milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}
