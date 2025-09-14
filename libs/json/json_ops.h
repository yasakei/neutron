#ifndef NEUTRON_LIBS_JSON_JSON_OPS_H
#define NEUTRON_LIBS_JSON_JSON_OPS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

char* json_stringify_c(const void* value, bool pretty);
void* json_parse_c(const char* json);

#ifdef __cplusplus
}
#endif

#endif // NEUTRON_LIBS_JSON_JSON_OPS_H