#pragma once

#include "lab_png.h"
#define BUF_SIZE 10240

typedef struct {
    int front;
    int rear;
    int size;
    RECV_BUF *items;
} circular_queue;

int init_shm_queue(circular_queue *p, int queue_size);
int sizeof_shm_queue(int size);
int is_full(circular_queue *p);
int is_empty(circular_queue *p);
int enqueue(circular_queue *p, RECV_BUF item);
int dequeue(circular_queue *p, RECV_BUF *p_item);