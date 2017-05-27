CFLAGS=-std=c99 -pedantic -Wall -O3

all: kpk pfkpk test

pfkpk: pfkpk.c kpk.c kpk.h
	$(CC) $(CFLAGS) -o $@ pfkpk.c kpk.c

kpk: kpk.h kpk.c
	$(CC) $(CFLAGS) -c -o kpk.o kpk.c

test:
	./pfkpk

# vi: noexpandtab
