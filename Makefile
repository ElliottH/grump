CFLAGS=-g -O2 -Wall --std=c99 -Werror -D_BSD_SOURCE=1
CC=gcc

all: grump

clean:
	rm -f grump *.o

grump: grump.c
	$(CC) $(CFLAGS) -o $@ $<

# End file.
