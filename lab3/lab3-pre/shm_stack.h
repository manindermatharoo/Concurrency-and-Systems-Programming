
/* The code is
 * Copyright(c) 2018-2019 Yiqing Huang, <yqhuang@uwaterloo.ca>.
 *
 * This software may be freely redistributed under the terms of the X11 License.
 */
/**
 * @brief  stack to push/pop integer(s), API header
 * @author yqhuang@uwaterloo.ca
 */

struct int_stack;

int sizeof_shm_stack(int size);
int init_shm_stack(struct int_stack *p, int stack_size);
struct int_stack *create_stack(int size);
void destroy_stack(struct int_stack *p);
int isFull(struct int_stack *p);
int isEmpty(struct int_stack *p);
int push(struct int_stack *p, int item);
void push_all(struct int_stack *p, int size);
int pop(struct int_stack *p, int *p_item);
