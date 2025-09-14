#ifndef NEUTRON_LIBS_TIME_TIME_OPS_H
#define NEUTRON_LIBS_TIME_TIME_OPS_H

#ifdef __cplusplus
extern "C" {
#endif

double time_now_c();
char* time_format_c(double timestamp, const char* format);
void time_sleep_c(int milliseconds);

#ifdef __cplusplus
}
#endif

#endif // NEUTRON_LIBS_TIME_TIME_OPS_H
