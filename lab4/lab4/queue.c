#include "queue.h"

// A utility function to create a new linked list node.
struct QNode* newNode(char* k, int size)
{
    struct QNode* temp = (struct QNode*)malloc(sizeof(struct QNode));
    temp->url = (char*)malloc(sizeof(char) * size + 1);
    strncpy(temp->url, k, size);
    temp->url[size] = '\0';
    temp->size = size;
    temp->next = NULL;
    return temp;
}

// A utility function to create an empty queue
struct Queue* createQueue()
{
    struct Queue* q = (struct Queue*)malloc(sizeof(struct Queue));
    q->front = q->rear = NULL;
    return q;
}

// The function to add a key k to q
void enQueue(struct Queue* q, char* k, int size)
{
    // Create a new LL node
    struct QNode* temp = newNode(k, size);
    // printf("Enqeued url is = %s \n", temp->url);

    // If queue is empty, then new node is front and rear both
    if (q->rear == NULL) {
        q->front = q->rear = temp;
        return;
    }

    // Add the new node at the end of queue and change rear
    q->rear->next = temp;
    q->rear = temp;
}

// Function to remove a key from given queue q
char* deQueue(struct Queue* q)
{
    // If queue is empty, return NULL.
    if (q->front == NULL) {
        return NULL;
    }

    // Store previous front and move front one node ahead
    struct QNode* temp = q->front;
    q->front = q->front->next;
    char* url = (char*)malloc(temp->size * (sizeof(char)) + 1);
    strncpy(url, temp->url, temp->size);
    url[temp->size] = '\0';
    free(temp->url);
    free(temp);

    // If front becomes NULL, then change rear also as NULL
    if (q->front == NULL) {
        q->rear = NULL;
    }

    return url;
}
