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
#include <semaphore.h>
#include <signal.h>

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
    int num_threads;
    int num_images;
    int png_urls_found;
    struct Queue* q;
    char** all_urls;
    int all_urls_index;
    char* log_file;
    char* url4;

    pthread_mutex_t queue;
    pthread_mutex_t all_urls_array;
    pthread_mutex_t log_file_lock;
    pthread_mutex_t png_file_lock;
    pthread_mutex_t png_urls_found_currently;
    pthread_mutex_t hash;
    pthread_mutex_t threads_mutex;
    pthread_mutex_t waiting_mutex;

    sem_t queue_sem;
    sem_t done;

    int threads_waiting;
    int less_threads_waiting;
};

typedef struct recv_buf2 {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

int isPNG(RECV_BUF *p_recv_buf);
htmlDocPtr mem_getdoc(char *buf, int size, const char *url);
xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath);
int find_http(char *fname, int size, int follow_relative_links, const char *base_url, struct thread_args *args);
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
void create_file(const char* path);
void file_close(void *arg);
int write_file(const char *path, const void *in, size_t len);
CURL *easy_handle_init(RECV_BUF *ptr, const char *url);
int process_html(CURL *curl_handle, RECV_BUF *p_recv_buf, struct thread_args *args);
int process_png(CURL *curl_handle, RECV_BUF *p_recv_buf, struct thread_args *args);
int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf, struct thread_args *args);

void initThreadArgs(struct thread_args* params);
int not_all_urls_found(struct thread_args *args);
int number_of_threads_waiting(struct thread_args *args);
void mutex_cleanup(void* arg);
void curl_cleanup( void* arg );
void buf_cleanup( void* arg );
void free_url(void* arg);
int command_line_options(struct thread_args* params, char *argurl, int argc, char ** argv);

void *retreive_urls(void *arg);