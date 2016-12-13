CC=clang
CFLAGS=-L. -Ofast  -Wall -Wno-char-subscripts -mpopcnt -mlzcnt

all: CFLAGS+= -fprofile-instr-use=code.profdata
all: libace.a chess perft benchmark init ace-uci score

debug: CFLAGS+= -D DEBUG
debug: score

instrument: CFLAGS+= -fprofile-instr-generate
instrument: benchmark
	./benchmark
	mv default.profraw benchmark.profraw
	xcrun llvm-profdata merge -output=code.profdata benchmark.profraw

generate_magic: generate_magic.c
	$(CC) -O3 -o generate_magic generate_magic.c

magic.c: generate_magic
	./generate_magic > magic.c

libace.a: board.c board.h parse.c engine.c search.c search.h util.c util.h evaluation.c magic.c magic.h moves.c moves.h timer.c timer.h pawns.c pawns.h
	$(CC) $(CFLAGS) -flto -o magic.o -c magic.c
	$(CC) $(CFLAGS) -flto -o moves.o -c moves.c
	$(CC) $(CFLAGS) -flto -o pawns.o -c pawns.c
	$(CC) $(CFLAGS) -flto -o parse.o -c parse.c
	$(CC) $(CFLAGS) -flto -o board.o -c board.c
	$(CC) $(CFLAGS) -flto -o util.o -c util.c
	$(CC) $(CFLAGS) -flto -o engine.o -c engine.c
	$(CC) $(CFLAGS) -flto -o search.o -c search.c
	$(CC) $(CFLAGS) -flto -o evaluation.o -c evaluation.c
	$(CC) $(CFLAGS) -flto -o timer.o -c timer.c
	$(CC) $(CFLAGS) -flto -r -o ace.o magic.o moves.o parse.o board.o util.o engine.o search.o evaluation.o pawns.o
	ar rc libace.a ace.o timer.o

clean:
	rm -f *.o libace.a


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
