#include "json_ops.h"
#include "c_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration
char* json_stringify_recursive(const struct CValue* v);

char* json_stringify_c(const void* value, bool pretty) {
    return json_stringify_recursive((const struct CValue*)value);
}

char* json_stringify_recursive(const struct CValue* v) {
    CValueType type = c_value_get_type(v);

    switch (type) {
        case C_NIL:
            return strdup("null");
        case C_BOOLEAN:
            return strdup(c_value_get_boolean(v) ? "true" : "false");
        case C_NUMBER: {
            char buffer[32];
            sprintf(buffer, "%g", c_value_get_number(v));
            return strdup(buffer);
        }
        case C_STRING: {
            const char* str = c_value_get_string(v);
            // Escape special characters
            char* result = (char*)malloc(strlen(str) * 2 + 3); // worst case: all chars escaped + quotes + null
            char* p = result;
            *p++ = '"';
            while (*str) {
                switch (*str) {
                    case '"': *p++ = '\\'; *p++ = '"'; break;
                    case '\\': *p++ = '\\'; *p++ = '\\'; break;
                    case '\b': *p++ = '\\'; *p++ = 'b'; break;
                    case '\f': *p++ = '\\'; *p++ = 'f'; break;
                    case '\n': *p++ = '\\'; *p++ = 'n'; break;
                    case '\r': *p++ = '\\'; *p++ = 'r'; break;
                    case '\t': *p++ = '\\'; *p++ = 't'; break;
                    default: *p++ = *str; break;
                }
                str++;
            }
            *p++ = '"';
            *p = '\0';
            return result;
        }
        case C_OBJECT: {
            size_t size = c_object_get_size(v);
            if (size == 0) {
                return strdup("{}");
            }

            char* result = strdup("{");
            for (size_t i = 0; i < size; i++) {
                const char* key = c_object_get_key_at(v, i);
                const struct CValue* value = c_object_get_value_at(v, i);
                char* value_str = json_stringify_recursive(value);

                char* pair;
                // asprintf is a GNU extension
                asprintf(&pair, "%s\"%s\":%s", (i > 0 ? "," : ""), key, value_str);
                
                result = (char*)realloc(result, strlen(result) + strlen(pair) + 1);
                strcat(result, pair);

                free(value_str);
                free(pair);
            }
            result = (char*)realloc(result, strlen(result) + 2);
            strcat(result, "}");
            return result;
        }
        case C_ARRAY: {
            size_t size = c_array_get_size(v);
            if (size == 0) {
                return strdup("[]");
            }

            char* result = strdup("[");
            for (size_t i = 0; i < size; i++) {
                const struct CValue* element = c_array_get_element_at(v, i);
                char* element_str = json_stringify_recursive(element);

                char* pair;
                // asprintf is a GNU extension
                asprintf(&pair, "%s%s", (i > 0 ? "," : ""), element_str);

                result = (char*)realloc(result, strlen(result) + strlen(pair) + 1);
                strcat(result, pair);

                free(element_str);
                free(pair);
            }
            result = (char*)realloc(result, strlen(result) + 2);
            strcat(result, "]");
            return result;
        }
        default:
            return strdup("\"<unsupported>\"");
    }
}

void* json_parse_c(const char* json) {
    // For now, just return a null pointer.
    return NULL;
}
