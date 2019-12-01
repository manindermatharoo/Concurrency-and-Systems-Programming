#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <pthread.h>
#include "search.h"
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include "queue.h"

#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */
#define MAX_URLS 1000

#define CT_PNG  "image/png"
#define CT_HTML "text/html"
#define CT_PNG_LEN  9
#define CT_HTML_LEN 9

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

struct thread_args
{
    int num_connections;
    int num_images;
    int png_urls_found;
    struct Queue* q;
    char** all_urls;
    int all_urls_index;
    char* log_file;
};

typedef struct recv_buf2 {
    char *url;
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

int isPNG(RECV_BUF *p_recv_buf);
htmlDocPtr mem_getdoc(char *buf, int size, const char *url);
xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath);
int find_http(char *fname, int size, int follow_relative_links, const char *base_url, struct Queue* q);
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size, char* url);
int recv_buf_cleanup(RECV_BUF *ptr);
void create_file(const char* path);
int write_file(const char *path, const void *in, size_t len);
void multi_handle_init(CURLM *cm, RECV_BUF *ptr, char *url);
int process_html(CURL *curl_handle, RECV_BUF *p_recv_buf, struct thread_args* arguments);
int process_png(CURL *curl_handle, RECV_BUF *p_recv_buf, struct thread_args* arguments);
int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf, struct thread_args* arguments);

void initThreadArgs(struct thread_args* params);
int command_line_options(struct thread_args* params, char *argurl, int argc, char ** argv);

void *retreive_urls(void *arg);