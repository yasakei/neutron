#include "http_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

struct MemoryStruct {
    char* memory;
    size_t size;
};

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct* mem = (struct MemoryStruct*)userp;

    char* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

struct HttpResponse* perform_request(const char* url, const char* method, const char* data, const struct HttpHeader* headers, size_t num_headers) {
    CURL* curl;
    CURLcode res;
    struct HttpResponse* response = (struct HttpResponse*)malloc(sizeof(struct HttpResponse));
    response->body = NULL;
    response->headers = NULL;
    response->num_headers = 0;

    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

        struct curl_slist* header_list = NULL;
        for (size_t i = 0; i < num_headers; i++) {
            char header[256];
            sprintf(header, "%s: %s", headers[i].key, headers[i].value);
            header_list = curl_slist_append(header_list, header);
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

        if (strcmp(method, "POST") == 0) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        } else if (strcmp(method, "PUT") == 0) {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        } else if (strcmp(method, "PATCH") == 0) {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        } else if (strcmp(method, "DELETE") == 0) {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        } else if (strcmp(method, "HEAD") == 0) {
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        }

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            response->status = -1;
        } else {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            response->status = response_code;
            response->body = chunk.memory;
        }

        curl_slist_free_all(header_list);
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    return response;
}

struct HttpResponse* http_get_c(const char* url, const struct HttpHeader* headers, size_t num_headers) {
    return perform_request(url, "GET", NULL, headers, num_headers);
}

struct HttpResponse* http_post_c(const char* url, const char* data, const struct HttpHeader* headers, size_t num_headers) {
    return perform_request(url, "POST", data, headers, num_headers);
}

struct HttpResponse* http_put_c(const char* url, const char* data, const struct HttpHeader* headers, size_t num_headers) {
    return perform_request(url, "PUT", data, headers, num_headers);
}

struct HttpResponse* http_delete_c(const char* url, const struct HttpHeader* headers, size_t num_headers) {
    return perform_request(url, "DELETE", NULL, headers, num_headers);
}

struct HttpResponse* http_head_c(const char* url, const struct HttpHeader* headers, size_t num_headers) {
    return perform_request(url, "HEAD", NULL, headers, num_headers);
}

struct HttpResponse* http_patch_c(const char* url, const char* data, const struct HttpHeader* headers, size_t num_headers) {
    return perform_request(url, "PATCH", data, headers, num_headers);
}

void free_http_response(struct HttpResponse* response) {
    if (response) {
        if (response->body) {
            free(response->body);
        }
        free(response);
    }
}