#ifndef NEUTRON_LIBS_CONVERT_CONVERT_OPS_H
#define NEUTRON_LIBS_CONVERT_CONVERT_OPS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

double char_to_int_c(const char* s);
char* int_to_char_c(double n);
char* string_get_char_at_c(const char* s, int i);
double string_length_c(const char* s);
double string_to_int_c(const char* s);
char* int_to_string_c(double n);
double bin_to_int_c(const char* s);
char* int_to_bin_c(int n);

#ifdef __cplusplus
}
#endif

#endif // NEUTRON_LIBS_CONVERT_CONVERT_OPS_H
