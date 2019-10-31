# Makefile, ECE252
# Yiqing Huang, 2018/11/02

CC = gcc
CFLAGS = -Wall -g -ggdb3 # "curl-config --cflags" output is empty
LD = gcc
LDFLAGS = -g
LDLIBS = -lcurl -lz -lpthread  # "curl-config --libs" output

SRCS   = paster.c shm_queue.c shm_stack.c pnginfo.c crc.c catpng.c zutil.c
OBJS1  = paster.o shm_queue.o shm_stack.o pnginfo.o crc.o catpng.o zutil.o
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
	../starter/tools/clean_ipcs.sh
