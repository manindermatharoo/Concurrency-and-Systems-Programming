
CC = gcc       # compiler
CFLAGS = -Wall -g # compilation flags
LD = gcc       # linker
LDFLAGS = -g   # debugging symbols in build
LDLIBS = -lz   # link with libz

LIB_UTIL = crc.o pnginfo.o zutil.o
SRCS   = catpng.c findpng.c pnginfo.c crc.c zutil.c
OBJS_FIND   = findpng.o $(LIB_UTIL)
OBJS_CAT = catpng.o $(LIB_UTIL)

TARGETS= findpng catpng

all: ${TARGETS}

findpng: $(OBJS_FIND)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

catpng: $(OBJS_CAT)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *.d *.o $(TARGETS)
