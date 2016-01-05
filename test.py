import os
import subprocess
import time
import sys

PERFT_CASES = [
    ("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", [20, 400, 8902, 197281, 4865609, 119060324]),
    ("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", [48, 2039, 97862, 4085603, 193690690]),
    ("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", [14, 191, 2812, 43238, 674624, 11030083]),
    ("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", [6, 264, 9467, 422333, 15833292, 706045033]),
    ("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", [44, 1486, 62379, 2103487, 89941194]),
    ("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", [46, 2079, 89890, 3894594, 164075551]),
]

MOVEGEN_CASES = [
    ("1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - - 0 1", ["d6d1"]),
    ("3r1k2/4npp1/1ppr3p/p6P/P2PPPP1/1NR5/5K2/2R5 w - - 0 1", ["d4d5"]),
    ("2q1rr1k/3bbnnp/p2p1pp1/2pPp3/PpP1P1P1/1P2BNNP/2BQ1PRK/7R b - -", ["f6f5"]),
    ("rnbqkb1r/p3pppp/1p6/2ppP3/3N4/2P5/PPP1QPPP/R1B1KB1R w KQkq -", ["e5e6"]),
    ("r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4RK1 w - -", ["c3d5", "a2a4"]),
    ("2r3k1/pppR1pp1/4p3/4P1P1/5P2/1P4K1/P1P5/8 w - -", ["g5g6"]),
    ("1nk1r1r1/pp2n1pp/4p3/q2pPp1N/b1pP1P2/B1P2R2/2P1B1PP/R2Q2K1 w - -", ["h5f6"]),
    ("4b3/p3kp2/6p1/3pP2p/2pP1P2/4K1P1/P3N2P/8 w - -", ["f4f5"]),
    ("2kr1bnr/pbpq4/2n1pp2/3p3p/3P1P1B/2N2N1Q/PPP3PP/2KR1B1R w - -", ["f4f5"]),
    ("3rr1k1/pp3pp1/1qn2np1/8/3p4/PP1R1P2/2P1NQPP/R1B3K1 b - -", ["c6e5"]),
    ("2r1nrk1/p2q1ppp/bp1p4/n1pPp3/P1P1P3/2PBB1N1/4QPPP/R4RK1 w - -", ["f2f4"]),
    ("r3r1k1/ppqb1ppp/8/4p1NQ/8/2P5/PP3PPP/R3R1K1 b - -", ["d7f5"]),
    # ("r2q1rk1/4bppp/p2p4/2pP4/3pP3/3Q4/PP1B1PPP/R3R1K1 w - -", "b4"),
    # ("rnb2r1k/pp2p2p/2pp2p1/q2P1p2/8/1Pb2NP1/PB2PPBP/R2Q1RK1 w - -", "Qd2 Qe1"),
    # ("2r3k1/1p2q1pp/2b1pr2/p1pp4/6Q1/1P1PP1R1/P1PN2PP/5RK1 w - -", "Qxg7+"),
    # ("r1bqkb1r/4npp1/p1p4p/1p1pP1B1/8/1B6/PPPN1PPP/R2Q1RK1 w kq -", "Ne4"),
    # ("r2q1rk1/1ppnbppp/p2p1nb1/3Pp3/2P1P1P1/2N2N1P/PPB1QP2/R1B2RK1 b - -", "h5"),
    # ("r1bq1rk1/pp2ppbp/2np2p1/2n5/P3PP2/N1P2N2/1PB3PP/R1B1QRK1 b - -", "Nb3"),
    # ("3rr3/2pq2pk/p2p1pnp/8/2QBPP2/1P6/P5PP/4RRK1 b - -", "Rxe4"),
    # ("r4k2/pb2bp1r/1p1qp2p/3pNp2/3P1P2/2N3P1/PPP1Q2P/2KRR3 w - -", "g4"),
    # ("3rn2k/ppb2rpp/2ppqp2/5N2/2P1P3/1P5Q/PB3PPP/3RR1K1 w - -", "Nh6"),
    # ("2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - -", "Bxe4"),
    # ("r1bqk2r/pp2bppp/2p5/3pP3/P2Q1P2/2N1B3/1PP3PP/R4RK1 b kq -", "f6"),
    # ("r2qnrnk/p2b2b1/1p1p2pp/2pPpp2/1PP1P3/PRNBB3/3QNPPP/5RK1 w - -", "f4"),
]



nodes = 0
T = 0.000001
any_failed = False

FNULL = open(os.devnull, 'w')

for fen, results in PERFT_CASES:
    for d, ans in enumerate(results):
        print 'Perft(%d) of "%s",' % (d + 1, fen)
        output = subprocess.check_output(["./perft", "--depth", str(d + 1), "--starting", fen], stderr=FNULL)
        parts = output.split('\n')[0].split(' ')
        nodes += int(parts[1])
        T += float(parts[4])
        if ans != int(parts[1]):
            print 'Failed.'
            print 'Expected %d. Got %d.' % (ans, int(parts[1]))
            print
            any_failed = True
            break
        else:
            print 'Pass.'
            print

print 'Done with perft tests. %d nodes in %.2f seconds (%.2f nodes/s).' % (nodes, T, nodes/T)
if any_failed:
    sys.exit(1)

score = 0
for fen, move in MOVEGEN_CASES:
    with open("testinput", "w") as f:
        f.write("exit\n")
    with open("testinput", "r") as f:
        white = 'h'
        black = 'h'
        if fen.split(" ")[1] == 'w':
            white = 'c'
        else:
            black = 'c'
        output = subprocess.check_output(["./chess", "--white", white, "--black", black, "--depth", "7", "--starting", fen], stderr=FNULL, stdin=f)
        if output.strip() in move:
            print 'Selected best move for "%s"' % fen
            score += 1
        else:
            print 'Selected %s instead of %s for "%s"' % (output.strip(), ';'.join(move), fen)

print 'Final score: %.2f' % (score / float(len(MOVEGEN_CASES)))

sys.exit(0)
