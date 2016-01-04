CC=clang
CFLAGS=-L. -g -O3 -Wall -mpopcnt -mlzcnt

all: libace.a chess perft benchmark init

generate_magic: generate_magic.c
	$(CC) -o generate_magic generate_magic.c

magic.c: generate_magic
	./generate_magic > magic.c

libace.a: board.c engine.c search.c util.c evaluation.c magic.c
	$(CC) -o magic.o -c magic.c
	$(CC) -o board.o -c board.c
	$(CC) -o util.o -c util.c
	$(CC) -o engine.o -c engine.c
	$(CC) -o search.o -c search.c
	$(CC) -o evaluation.o -c evaluation.c
	ar r libace.a board.o engine.o search.o util.o evaluation.o magic.o

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

playself: playself.c libace.a
	$(CC) $(CFLAGS) playself.c -lace -o playself
