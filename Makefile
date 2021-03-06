CC=clang
CFLAGS=-Ofast  -Wall -Wno-char-subscripts -mpopcnt -mlzcnt

all: CFLAGS+= -fprofile-instr-use=code.profdata
all: libace.a perft benchmark ace-uci tuner_eval score score_debug

instrument: CFLAGS+= -fprofile-instr-generate
instrument: benchmark
	./benchmark
	mv default.profraw benchmark.profraw
	xcrun llvm-profdata merge -output=code.profdata benchmark.profraw

generate_magic: generate_magic.c
	$(CC) -O3 -o generate_magic generate_magic.c

magic.c: generate_magic
	./generate_magic > magic.c

libace.a: board.c board.h parse.c engine.c cJSON.c search.c search.h util.c util.h evaluation.c magic.c magic.h moves.c moves.h timer.c timer.h pawns.c pawns.h evaluation_parameters.c evaluation_parameters.h endgame.c endgame.h pfkpk/kpk.o
	$(CC) $(CFLAGS) -flto -o magic.o -c magic.c
	$(CC) $(CFLAGS) -flto -o moves.o -c moves.c
	$(CC) $(CFLAGS) -flto -o pawns.o -c pawns.c
	$(CC) $(CFLAGS) -flto -o parse.o -c parse.c
	$(CC) $(CFLAGS) -flto -o board.o -c board.c
	$(CC) $(CFLAGS) -flto -o util.o -c util.c
	$(CC) $(CFLAGS) -flto -o cJSON.o -c cJSON.c
	$(CC) $(CFLAGS) -flto -o engine.o -c engine.c
	$(CC) $(CFLAGS) -flto -o search.o -c search.c
	$(CC) $(CFLAGS) -flto -o evaluation_parameters.o -c evaluation_parameters.c
	$(CC) $(CFLAGS) -flto -o endgame.o -c endgame.c
	$(CC) $(CFLAGS) -flto -o evaluation.o -c evaluation.c
	$(CC) $(CFLAGS) -flto -o timer.o -c timer.c
	$(CC) $(CFLAGS) -flto -r -o ace.o cJSON.o magic.o moves.o parse.o board.o util.o engine.o search.o evaluation_parameters.o evaluation.o pawns.o endgame.o pfkpk/kpk.o
	ar rc libace.a ace.o timer.o

libace_debug.a: board.c board.h parse.c engine.c cJSON.c search.c search.h util.c util.h evaluation.c magic.c magic.h moves.c moves.h timer.c timer.h pawns.c pawns.h evaluation_parameters.c evaluation_parameters.h endgame.c endgame.h pfkpk/kpk.o
	$(CC) $(CFLAGS) -D DEBUG -flto -o magic.o -c magic.c
	$(CC) $(CFLAGS) -D DEBUG -flto -o moves.o -c moves.c
	$(CC) $(CFLAGS) -D DEBUG -flto -o pawns.o -c pawns.c
	$(CC) $(CFLAGS) -D DEBUG -flto -o parse.o -c parse.c
	$(CC) $(CFLAGS) -D DEBUG -flto -o board.o -c board.c
	$(CC) $(CFLAGS) -D DEBUG -flto -o util.o -c util.c
	$(CC) $(CFLAGS) -D DEBUG -flto -o engine.o -c engine.c
	$(CC) $(CFLAGS) -D DEBUG -flto -o cJSON.o -c cJSON.c
	$(CC) $(CFLAGS) -D DEBUG -flto -o search.o -c search.c
	$(CC) $(CFLAGS) -D DEBUG -flto -o evaluation_parameters.o -c evaluation_parameters.c
	$(CC) $(CFLAGS) -D DEBUG -flto -o endgame.o -c endgame.c
	$(CC) $(CFLAGS) -D DEBUG -flto -o evaluation.o -c evaluation.c
	$(CC) $(CFLAGS) -D DEBUG -flto -o timer.o -c timer.c
	$(CC) $(CFLAGS) -flto -r -o ace.o cJSON.o magic.o moves.o parse.o board.o util.o engine.o search.o evaluation_parameters.o evaluation.o pawns.o endgame.o pfkpk/kpk.o
	ar rc libace_debug.a ace.o timer.o

clean:
	rm -f *.o libace.a

score: libace.a score.c
	$(CC) $(CFLAGS) score.c -L. -lace -o score

tuner_eval: libace.a tuner_eval.c
	$(CC) $(CFLAGS) tuner_eval.c -L. -lace -o tuner_eval

score_debug: libace_debug.a score.c
	$(CC) $(CFLAGS) -D DEBUG score.c -L. -lace_debug -o score_debug

ace-uci: libace.a ace_uci.c
	$(CC) $(CFLAGS) ace_uci.c -L. -lace -o ace-uci -pthread

perft: perft.c libace.a
	$(CC) $(CFLAGS) perft.c -L. -lace -o perft

benchmark: benchmark.c libace.a
	$(CC) $(CFLAGS) benchmark.c -L. -lace -o benchmark

test: test.py perft chess
	python test.py
