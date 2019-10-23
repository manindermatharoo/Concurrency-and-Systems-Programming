#include "shm_queue.h"

int sizeof_shm_queue(int size)
{
    return (sizeof(circular_queue) + sizeof(RECV_BUF) + (sizeof(char) * BUF_SIZE * size));
}

int init_shm_queue(circular_queue *p, int queue_size)
{
    if ( p == NULL || queue_size == 0 ) {
        return 1;
    }

    p->front = -1;
    p->rear = -1;
    p->size = queue_size;
    p->items = (RECV_BUF *) (p + sizeof(circular_queue));
    return 0;
}

int is_full(circular_queue *p)
{
    if((p->front == p->rear + 1) || (p->front == 0 && p->rear == p->size-1))
    {
        return 1;
    }
    return 0;

}

int is_empty(circular_queue *p)
{
    if(p->front == -1)
    {
        return 1;
    }
    return 0;
}

int enqueue(circular_queue *p, RECV_BUF item)
{
    if(is_full(p))
    {
        return -1;
    }
    else
    {
        if(p->front == -1)
        {
            p->front = 0;
        }
        p->rear = (p->rear + 1) % p->size;
        p->items[p->rear] = item;
    }
    return 0;
}

int dequeue(circular_queue *p, RECV_BUF *p_item)
{
    if(is_empty(p))
    {
        return -1;
    }
    else
    {
        *p_item = p->items[p->front];
        if (p->front == p->rear){
            p->front = -1;
            p->rear = -1;
        } /* Q has only one element, so we reset the queue after dequeing it. ? */
        else
        {
            p->front = (p->front + 1) % p->size;

        }
    }
    return 0;
}
