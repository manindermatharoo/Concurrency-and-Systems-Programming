// A C program to demonstrate linked list based implementation of queue
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A linked list (LL) node to store a queue entry
struct QNode {
    char* url;
    int size;
    struct QNode* next;
};

// The queue, front stores the front node of LL and rear stores the
// last node of LL
struct Queue {
    struct QNode *front, *rear;
};

struct QNode* newNode(char* k, int size);
struct Queue* createQueue();
void enQueue(struct Queue* q, char* k, int size);
char* deQueue(struct Queue* q);
