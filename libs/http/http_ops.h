#ifndef NEUTRON_LIBS_HTTP_HTTP_OPS_H
#define NEUTRON_LIBS_HTTP_HTTP_OPS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct HttpHeader {
    char* key;
    char* value;
};

struct HttpResponse {
    long status;
    char* body;
    struct HttpHeader* headers;
    size_t num_headers;
};

struct HttpResponse* http_get_c(const char* url, const struct HttpHeader* headers, size_t num_headers);
struct HttpResponse* http_post_c(const char* url, const char* data, const struct HttpHeader* headers, size_t num_headers);
struct HttpResponse* http_put_c(const char* url, const char* data, const struct HttpHeader* headers, size_t num_headers);
struct HttpResponse* http_delete_c(const char* url, const struct HttpHeader* headers, size_t num_headers);
struct HttpResponse* http_head_c(const char* url, const struct HttpHeader* headers, size_t num_headers);
struct HttpResponse* http_patch_c(const char* url, const char* data, const struct HttpHeader* headers, size_t num_headers);

void free_http_response(struct HttpResponse* response);

#ifdef __cplusplus
}
#endif

#endif // NEUTRON_LIBS_HTTP_HTTP_OPS_H
