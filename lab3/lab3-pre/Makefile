# Makefile, ECE252
# Yiqing Huang, 2018/11/02

CC = gcc
CFLAGS = -Wall -std=c99 -g -ggdb3 # "curl-config --cflags" output is empty
LD = gcc
LDFLAGS = -std=c99 -g
LDLIBS = -lcurl -lz -lpthread  # "curl-config --libs" output

SRCS   = paster.c shm_queue.c
OBJS1  = paster.o shm_queue.o
TARGETS= paster2

all: ${TARGETS}

paster2: $(OBJS1)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *~ *.d *.o $(TARGETS) *.png
