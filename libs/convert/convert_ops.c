#include "convert_ops.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

double char_to_int_c(const char* s) {
    return (double)s[0];
}

char* int_to_char_c(double n) {
    char* s = (char*)malloc(2);
    s[0] = (char)n;
    s[1] = '\0';
    return s;
}

char* string_get_char_at_c(const char* s, int i) {
    if (i < 0 || i >= strlen(s)) {
        return NULL;
    }
    char* c = (char*)malloc(2);
    c[0] = s[i];
    c[1] = '\0';
    return c;
}

double string_length_c(const char* s) {
    return (double)strlen(s);
}

double string_to_int_c(const char* s) {
    return atof(s);
}

char* int_to_string_c(double n) {
    char* s = (char*)malloc(32);
    sprintf(s, "%g", n);
    return s;
}

double bin_to_int_c(const char* s) {
    return (double)strtol(s, NULL, 2);
}

char* int_to_bin_c(int n) {
    char* s = (char*)malloc(33);
    s[32] = '\0';
    for (int i = 31; i >= 0; i--) {
        s[i] = (n & 1) + '0';
        n >>= 1;
    }
    return s;
}
