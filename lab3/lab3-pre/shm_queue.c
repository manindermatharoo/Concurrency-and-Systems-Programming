#include "shm_queue.h"

int sizeof_shm_queue(int size)
{
    return (sizeof(circular_queue) + ((sizeof(RECV_BUF)) * size));
}

int init_shm_queue(circular_queue *p, int queue_size)
{
    if ( p == NULL || queue_size == 0 ) {
        return 1;
    }

    p->front = -1;
    p->rear = -1;
    p->size = queue_size;
    p->items = (RECV_BUF *)p + sizeof(circular_queue);

    return 0;
}

int init_shm_stack_RECV_BUF_buf(circular_queue *p, char *RECV_BUF_buf, int buf_size, int nbytes)
{
    for(int i = 0; i < buf_size; i++)
    {
        p->items[i].buf = RECV_BUF_buf + (i * BUF_SIZE);
        p->items[i].size = 0;
        p->items[i].max_size = nbytes;
        p->items[i].seq = -1;              /* valid seq should be non-negative */
    }

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

int enqueue(circular_queue *p, RECV_BUF *item, char *item_buf)
{
    if(is_full(p))
    {
        printf("Here \n");
        return -1;
    }
    else
    {
        if(p->front == -1)
        {
            p->front = 0;
        }
        p->rear = (p->rear + 1) % p->size;
        memcpy(&p->items[p->rear].size, &item->size, sizeof(item->size));
        memcpy(&p->items[p->rear].max_size, &item->max_size, sizeof(item->max_size));
        memcpy(&p->items[p->rear].seq, &item->seq, sizeof(item->seq));
        memset(item_buf + (p->rear * BUF_SIZE), 0, BUF_SIZE);
        memcpy(item_buf + (p->rear * BUF_SIZE), item->buf, BUF_SIZE);
    }
    return 0;
}

int dequeue(circular_queue *p, RECV_BUF *p_item, char *p_item_buf)
{
    if(is_empty(p))
    {
        printf("Here2 \n");
        return -1;
    }
    else
    {
        memcpy(&p_item->size, &p->items[p->front].size, sizeof(p_item->size));
        memcpy(&p_item->max_size, &p->items[p->front].max_size, sizeof(p_item->max_size));
        memcpy(&p_item->seq, &p->items[p->front].seq, sizeof(p_item->seq));
        memcpy(p_item->buf, p_item_buf + (p->front * BUF_SIZE), BUF_SIZE);
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
