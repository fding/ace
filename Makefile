CC=clang
CFLAGS=-L. -O3 -Wall -mpopcnt -mlzcnt

all: libace.a chess perft benchmark init

generate_magic: generate_magic.c
	$(CC) -o generate_magic generate_magic.c

magic.c: generate_magic
	./generate_magic > magic.c

libace.a: board.c engine.c search.c util.c evaluation.c magic.c moves.c
	$(CC) $(CFLAGS) -o magic.bc -c magic.c
	$(CC) $(CFLAGS) -o moves.bc -c moves.c
	$(CC) $(CFLAGS) -o board.bc -c board.c
	$(CC) $(CFLAGS) -o util.bc -c util.c
	$(CC) $(CFLAGS) -o engine.bc -c engine.c
	$(CC) $(CFLAGS) -o search.bc -c search.c
	$(CC) $(CFLAGS) -o evaluation.bc -c evaluation.c
	$(CC) $(CFLAGS) -flto -r -o ace.o magic.o moves.o board.o util.o engine.o search.o evaluation.o
	ar rc libace.a ace.o

clean:
	rm -f *.o


score: libace.a score.c
	$(CC) $(CFLAGS) score.c -lace -o score

init: libace.a init.c
	$(CC) $(CFLAGS) init.c -lace -o init

openings.acebase: init openings.txt
	rm -f openings.acebase
	./init openings.txt

chess: libace.a main.c openings.acebase
	$(CC) $(CFLAGS) main.c -lace -o chess

perft: perft.c libace.a
	$(CC) $(CFLAGS) perft.c -lace -o perft

benchmark: benchmark.c libace.a
	$(CC) $(CFLAGS) benchmark.c -lace -o benchmark

playself: playself.c libace.a
	$(CC) $(CFLAGS) playself.c -lace -o playself

test: test.py perft chess
	python test.py
