import os
import fcntl
import sys
import random
import time

import json
import argparse

import chess
import chess.engine
import chess.pgn
import elo
# import math

# import sqlite3

OPENINGS = [
    (("e4", "e5"), "E4 E5"),
    (("e4", "e5", "Nf3", "Nf6"), "Petroff"),
    (("e4", "e5", "Nf3", "Nc6", "Bb5", "a6"), "Ruy Lopez"),
    (("e4", "e5", "Nf3", "Nc6", "Bb5", "a6", "Ba4", "Nf6", "O-O", "Be7", "Re1", "b5", "Bb3", "O-O", "c3", "d5"), "Ruy Lopez"),  # Marshall attack, included for sharpness
    (("e4", "e5", "Nf3", "Nc6", "Bb5", "Nf6"), "Ruy Lopez"),
    (("e4", "e5", "Nf3", "Nc6", "Bc4"), "Giuco Piano"),
    (("e4", "e5", "Nf3", "Nc6", "Bc4", "Bc5", "b4"), "Gambit"),
    (("e4", "e5", "Nf3", "Nc6", "Bc4", "Nf6", "Ng5", "d5", "exd5"), "Gambit"),
    # e4 e5: 5 openings
    (("e4", "c5"), "Sicilian"),
    (("e4", "c5", "Nc3"), "Closed Sicilian"),
    (("e4", "c5", "Nf3", "Nc6", "d4", "cxd4", "Nxd4",), "Open Sicilian"),
    (("e4", "c5", "Nf3", "d6", "d4", "cxd4", "Nxd4", "Nf6", "Nc3", "a6"), "Open Sicilian"),
    (("e4", "c5", "Nf3", "d6", "d4", "cxd4", "Nxd4", "Nf6", "Nc3", "g6"), "Open Sicilian"),
    (("e4", "c5", "Nf3", "d6", "d4", "cxd4", "Nxd4", "Nf6", "Nc3", "Nc6"), "Open Sicilian"),
    (("e4", "c5", "Nf3", "d6", "Bb5"), "Sicilian Rossolimo"),
    (("e4", "c5", "Nf3", "e6", "d4", "cxd4", "Nxd4"), "Open Sicilian"),
    # Sicilian: 8 openings
    (("e4", "e6", "d4", "d5", "Nc3"), "French"),
    (("e4", "e6", "d4", "d5", "Nd2"), "French"),
    # French: 2 openings
    (("e4", "c6", "d4", "d5"), "Caro Kann"),
    (("e4", "d6", "d4", "Nf6"), "Phillidor"),
    # e4: 17 openings
    (("d4", "d5"), "D4 D5"),
    (("d4", "d5", "c4"), "Queens Gambit"),
    (("d4", "d5", "c4", "e6"), "Queens Gambit Declined"),
    (("d4", "d5", "c4", "c6"), "Slav defense"),
    (("d4", "Nf6"), "D4 Nf6"),
    (("d4", "Nf6", "Nf3", "g6"), "Kings Indian"),
    (("d4", "Nf6", "c4", "e6"), "D4 Nf6"),
    (("d4", "Nf6", "c4", "e6", "Nc3", "Bb4"), "Nimzo-Indian"),
    (("d4", "Nf6", "c4", "g6"), "Kings Indian"),
    (("d4", "Nf6", "c4", "g6", "Nc3", "Bg7", "e4", "d6", "Nf3", "O-O", "Be2"), "Kings Indian"),
    # d4: 10 openings
    (("c4", "e5"), "English"),
    (("c4", "Nf6"), "English"),
    (("Nf3", "d5"), "Reti"),
    (("Nf3", "Nf6", "c4"), "Reti"),
    # 4 misc openings
]

score_for_opening = {}

for opening, name in OPENINGS:
    board = chess.Board()
    score_for_opening[name] = [0, 0, 0]
    for move in opening:
        board.push_san(move)
random.shuffle(OPENINGS)
print("Validated openings")

parser = argparse.ArgumentParser()
parser.add_argument("-w", dest="white")
parser.add_argument("-b", dest="black")
parser.add_argument("-n", dest="ngames", default=150, type=int)
parser.add_argument("--opening", dest="opening", default="")

args = parser.parse_args()


class Engine(object):
    def __init__(self, binary):
        self._binary = binary
        self._started = False
        self._uci_input = None
        self._uci_output = None

    @property
    def binary(self):
        return self._binary

    def start(self):
        r1, w1 = os.pipe()
        r2, w2 = os.pipe()
        r3, w3 = os.pipe()

        self._uci_input = w1
        self._uci_output = r2
        flags = fcntl.fcntl(self._uci_output, fcntl.F_GETFL)
        fcntl.fcntl(self._uci_output, fcntl.F_SETFL,
                    flags | os.O_NONBLOCK)
        flags = fcntl.fcntl(w2, fcntl.F_GETFL)
        fcntl.fcntl(w2, fcntl.F_SETFL,
                    flags | fcntl.F_NOCACHE | os.O_NDELAY | os.O_SYNC)
        flags = fcntl.fcntl(self._uci_input, fcntl.F_GETFL)
        fcntl.fcntl(self._uci_input, fcntl.F_SETFL,
                    flags | fcntl.F_NOCACHE | os.O_SYNC | os.O_NDELAY)

        pid = os.fork()
        if pid == 0:
            print('Starting engine')
            os.close(w1)
            os.close(r2)
            os.dup2(r1, sys.stdin.fileno())
            os.dup2(w2, sys.stdout.fileno())
            os.dup2(w3, sys.stderr.fileno())
            os.close(r1)
            os.close(w2)
            os.close(w3)
            os.execl(self._binary, self._binary)

    def send(self, message):
        os.write(self._uci_input, (message + '\n').encode("utf-8"))

    def read(self):
        buf = []
        while True:
            try:
                c = os.read(self._uci_output, 1).decode("utf-8")
            except BlockingIOError:
                s = ''.join(buf)
                if s:
                    return [c for c in s.split("\n") if c.strip()]
                return []
            buf.append(c)

    def query_best_move(self, fen, moves, think_time=1.2):
        message = ''
        if fen == 'startpos':
            message = 'position startpos'
        else:
            message = 'position fen ' + fen
        moves = moves.strip()
        if moves:
            message = message + ' moves ' + moves

        self.send(message)
        self.send('go infinite')
        time.sleep(think_time)
        self.send("stop")
        stop_count = 0
        while True:
            reply = []
            while not reply:
                reply = self.read()

            for r in reply:
                pass
                # print('Received reply ', r)
            if reply[-1].startswith('bestmove'):
                return reply[-1][9:]

# engines = [Engine(args.white), Engine(args.black)]
# for engine in engines:
#     engine.start()

def to_pgn(board):
    pgn = chess.pgn.Game()
    pgn.headers["Result"] = board.result(claim_draw=True)
    node = pgn
    for move in board.move_stack:
        node = node.add_variation(move)
    return pgn

def play_game(engines, opening):
    current_player = 0
    moves = []
    winner = -1
    board = chess.Board()
    for move in opening:
        board.push_san(move)
    while not board.is_game_over(claim_draw=True):
        try:
            result = engines[current_player].play(
                board, chess.engine.Limit(time=1))
        except Exception as e:
            time.sleep(5)
            print(to_pgn(board))
            raise e
        board.push(result.move)
        current_player = 1 - current_player

    outcome = board.result(claim_draw=True)
    if outcome == "*":
        outcome = "1/2-1/2"
    return outcome, board, moves


wins = [0, 0]
draws = [0, 0]
losses = [0, 0]
for j in range(0, args.ngames):
    reversed_players = False
    if j % 2 == 1:
        reversed_players = True

    opening, name = OPENINGS[(j // 2) % len(OPENINGS)]
    if args.opening:
        if name != args.opening:
            continue
    engines = [
        chess.engine.SimpleEngine.popen_uci(args.white),
        chess.engine.SimpleEngine.popen_uci(args.black),
    ]
    try:
        if reversed_players:
            outcome, board, moves = play_game(list(reversed(engines)), opening)
        else:
            outcome, board, moves = play_game(engines, opening=opening)
    except KeyboardInterrupt:
        pass
    except Exception as e:
        print("Error raised: ", e)
        continue
    print("Outcome: ", outcome)

    pgn = to_pgn(board)
    if reversed_players:
        pgn.headers["White"] = args.black
        pgn.headers["Black"] = args.white
    else:
        pgn.headers["White"] = args.white
        pgn.headers["Black"] = args.black
    print(pgn)

    points = [0, 0]
    if reversed_players:
        if outcome == "1-0":
            outcome = "0-1"
        elif outcome == "0-1":
            outcome = "1-0"

    if outcome == "1/2-1/2":
        draws[0] += 1
        draws[1] += 1
        score_for_opening[name][1] += 1
    else:
        if outcome == "1-0":
            wins[0] += 1
            losses[1] += 1
            score_for_opening[name][0] += 1
        elif outcome == "0-1":
            wins[1] += 1
            losses[0] += 1
            score_for_opening[name][2] += 1
    for e in engines:
        e.close()

    print("=============================================")
    print("Opening: %s. Moves: %s" % (name, opening))
    print("Engine %s: %s - %s - %s" % (args.white, wins[0], draws[0], losses[0]))
    print("Engine %s: %s - %s - %s" % (args.black, wins[1], draws[1], losses[1]))
    if wins[0] + draws[0] > 0 and losses[0] + draws[0] > 0:
        elo_diff, confidence = elo.estimate_elo_diff(wins[0], draws[0], losses[0])
        print("Elo diff: %d. 95%% confidence interval: (%.1f, %.1f)" % (elo_diff, confidence[0], confidence[1]))
    for name, scores in score_for_opening.items():
        print("Score for %s: %d - %d - %d" % (name, scores[0], scores[1], scores[2]))
    print("=============================================")

