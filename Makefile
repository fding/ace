CC=clang
CFLAGS=-L. -O3 -Wall -Wno-char-subscripts -mpopcnt -mlzcnt

all: libace.a chess perft benchmark init ace-uci score

generate_magic: generate_magic.c
	$(CC) -O3 -o generate_magic generate_magic.c

magic.c: generate_magic
	./generate_magic > magic.c

libace.a: book.c board.c engine.c search.c util.c evaluation.c magic.c moves.c timer.c pawns.c
	$(CC) $(CFLAGS) -o magic.o -c magic.c
	$(CC) $(CFLAGS) -o moves.o -c moves.c
	$(CC) $(CFLAGS) -o pawns.o -c pawns.c
	$(CC) $(CFLAGS) -o board.o -c board.c
	$(CC) $(CFLAGS) -o util.o -c util.c
	$(CC) $(CFLAGS) -o engine.o -c engine.c
	$(CC) $(CFLAGS) -o search.o -c search.c
	$(CC) $(CFLAGS) -o evaluation.o -c evaluation.c
	$(CC) $(CFLAGS) -o timer.o -c timer.c
	$(CC) $(CFLAGS) -flto -r -o ace.o magic.o moves.o board.o util.o engine.o search.o evaluation.o pawns.o
	ar rc libace.a ace.o timer.o

clean:
	rm -f *.o


score: libace.a score.c
	$(CC) $(CFLAGS) score.c -lace -o score

init: libace.a init.c
	$(CC) $(CFLAGS) init.c -lace -o init

openings.acebase: init openings.txt
	rm -f openings.acebase
	./init openings.txt

ace-uci: libace.a ace_uci.c
	$(CC) -L. -g ace_uci.c -lace -o ace-uci -pthread

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
