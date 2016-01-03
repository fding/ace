CC=clang
CFLAGS=-L. -g -O3 -Wall

all: libace.a chess perft benchmark init

libace.a: board.c engine.c search.c
	$(CC) -o board.o -c board.c
	$(CC) -o engine.o -c engine.c
	$(CC) -o search.o -c search.c
	ar rc libace.a board.o engine.o search.o

score: libace.a score.c
	$(CC) $(CFLAGS) score.c -lace -o score

init: libace.a init.c
	$(CC) $(CFLAGS) init.c -lace -o init

chess: libace.a main.c
	$(CC) $(CFLAGS) main.c -lace -o chess

perft: perft.c libace.a
	$(CC) $(CFLAGS) perft.c -lace -o perft

benchmark: benchmark.c libace.a
	$(CC) $(CFLAGS) benchmark.c -lace -o benchmark
