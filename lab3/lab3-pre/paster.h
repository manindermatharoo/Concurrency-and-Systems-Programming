#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <curl/curl.h>
#include "catpng.h"
#include "lab_png.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <unistd.h>
#include "shm_queue.h"
#include "shm_stack.h"
#include <time.h>
#include <sys/time.h>
#include <pthread.h>

#define IMG1_URL "http://ece252-2.uwaterloo.ca:2520/image?img=1"
#define IMG2_URL "http://ece252-2.uwaterloo.ca:2520/image?img=2"
#define IMG3_URL "http://ece252-2.uwaterloo.ca:2520/image?img=3"

#define DUM_URL "https://example.com/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 10240
#define BUF_INC  524288   /* 1024*512  = 0.5M */
#define TOTAL_PNG_CHUNKS 50     /* Web Server contains 50 chunks of the image */

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct {
    int buf_size;               /* buffer size */
    int num_producers;          /* number of producers */
    int num_consumers;          /* number of consumers */
    int consumer_sleep_ms;      /* milliseconds a consumer sleeps */
    int img_number;             /* image you want to receive */
} arguments;

/******************************************************************************
 * FUNCTION PROTOTYPES
 *****************************************************************************/
int size_of_IDAT_formatted_data(int size);

void* send_curl(RECV_BUF* p_shm_recv_buf, int img, int img_part);

void extract_IDAT(RECV_BUF *img, U8* IDAT_data);

void producer(sem_t* sems, pthread_mutex_t* mutex, pthread_mutex_t* mutex_stack, circular_queue *p, int img, struct int_stack *s, int buf_size);

void consumer(sem_t* sems, pthread_mutex_t* mutex, circular_queue *p, int sleep_ms, U8* IDAT_data, int buf_size);

int command_line_options(arguments* args, int argc, char ** argv);

int initializeSems(sem_t* sems, int buf_size);

int sizeof_shm_recv_buf(size_t nbytes);

/**
 * @brief  cURL header call back function to extract image sequence number from
 *         http header data. An example header for image part n (assume n = 2) is:
 *         X-Ece252-Fragment: 2
 * @param  char *p_recv: header data delivered by cURL
 * @param  size_t size size of each memb
 * @param  size_t nmemb number of memb
 * @param  void *userdata user defined data structurea
 * @return size of header data received.
 * @details this routine will be invoked multiple times by the libcurl until the full
 * header data are received.  we are only interested in the ECE252_HEADER line
 * received so that we can extract the image sequence number from it. This
 * explains the if block in the code.
 */
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);


/**
 * @brief write callback function to save a copy of received data in RAM.
 *        The received libcurl data are pointed by p_recv,
 *        which is provided by libcurl and is not user allocated memory.
 *        The user allocated memory is at p_userdata. One needs to
 *        cast it to the proper struct to make good use of it.
 *        This function maybe invoked more than once by one invokation of
 *        curl_easy_perform().
 */
size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata);

int shm_recv_buf_init(RECV_BUF *ptr, size_t max_size);

int recv_buf_cleanup(RECV_BUF *ptr);
