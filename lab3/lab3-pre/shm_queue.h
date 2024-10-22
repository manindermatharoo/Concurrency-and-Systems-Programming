#pragma once

#include "lab_png.h"
#include <string.h>

#define BUF_SIZE 10240

typedef struct {
    int front;
    int rear;
    int size;
    RECV_BUF *items;
} circular_queue;

int init_shm_queue(circular_queue *p, int queue_size);
int init_shm_stack_RECV_BUF_buf(circular_queue *p, char *RECV_BUF_buf, int buf_size, int nbytes);
int sizeof_shm_queue(int size);
int is_full(circular_queue *p);
int is_empty(circular_queue *p);
int enqueue(circular_queue *p, RECV_BUF *item, char *item_buf);
int dequeue(circular_queue *p, RECV_BUF *p_item, char *p_item_buf);