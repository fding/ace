# 122.62523213685652 352.3887829910136 365.9677995635393 587.5126248335171 1088.5146376097625 49.622765254071936 -26.739147087814416 [-99.94124244 -25.44750413  -9.67818585   2.08228254   8.81787075
#   16.52654823  23.77728808  33.16545736  50.69748546] [-14.99940886  15.75701893  21.33088476  19.4035165   14.88479321
#    9.41883992  -3.86845499 -21.54858387 -40.37860561] [-70.2845025  -29.87949558  16.01294563  46.84088824  54.2587072
#   29.94854645 -46.89708945]
# 287.37053284248077 298.444123763288 479.1123446582515 887.6759037609162 40.4670102468717 -21.805583257100018 [-81.5013686  -20.75225766  -7.89249136   1.69808652   7.19091055
#   13.47728191  19.39020842  27.04619333  41.34343689] [-12.23191067  12.84973627  17.395184    15.82342897  12.13844244
#    7.68099661  -3.15469739 -17.57271607 -32.92846415] [-57.31650924 -24.366515    13.05844267  38.19840944  44.24758776
#   24.42282549 -38.24424112]
import random
import time
import itertools
import cma
import json
import scipy.optimize
import scipy.special
import numpy as np
import subprocess
from logistic_regression import cross_entropy_and_grads, Adam


RES_TO_SCORE = {
    "1-0": 1.0,
    "0-1": 0.0,
    "1/2-1/2": 0.5,
}

def ds_iter():
    shuffle_buffer = []
    shuffle_buffer_size = 725000
    with open("quiet-labeled.epd") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split("\"")
            pos, res = (parts[0][:-4] + " 0 0", RES_TO_SCORE[parts[1]])
            yield pos, res
            # shuffle_buffer.append((pos, res))

# def ds_iter(): #     with open("gm_positions.txt") as f:
#         for i, line in enumerate(f):
#             line = line.strip()
#             if not line:
#                 continue
#             parts = line.split("[")
#             pos = parts[0]
#             score = float(parts[1][:-1])
#             yield pos, score

position_iter = itertools.cycle(ds_iter())
LN10 = np.log(10)


def parse_params(params):
    bishop_v, bishop_pair_v, rook_pair_v, knight_pair_v = params[:4]
    knight_material_adj_table = params[4:13]
    rook_material_adj_table = params[13:22]
    queen_material_adj_table = params[22:29]
    pawn_material_adj_table = params[29:]
    return (bishop_v, bishop_pair_v, rook_pair_v, knight_pair_v,
            knight_material_adj_table, rook_material_adj_table,
            queen_material_adj_table, pawn_material_adj_table)



def position_eval(fen, params):
    fen = fen.split(" ")[0]
    (bishop_v, bishop_pair_v, rook_pair_v, knight_pair_v,
     knight_material_adj_table, rook_material_adj_table,
     queen_material_adj_table, pawn_material_adj_table) = parse_params(params)
    def eval_for_side(side):
        if side == "w":
            npawns = fen.count("P")
            nknight = fen.count("N")
            nbishop = fen.count("B")
            nrook = fen.count("R")
            nqueen = fen.count("Q")
            opposing_minors = fen.count("n") + fen.count("b") + fen.count("r")
        else:
            npawns = fen.count("p")
            nknight = fen.count("n")
            nbishop = fen.count("b")
            nrook = fen.count("r")
            nqueen = fen.count("q")
            opposing_minors = fen.count("N") + fen.count("B") + fen.count("R")
        return bishop_v * nbishop + bishop_pair_v * (nbishop == 2) + rook_pair_v * (nrook == 2) + knight_pair_v * (nknight == 2) + knight_material_adj_table[npawns] * nknight + rook_material_adj_table[npawns] * nrook + queen_material_adj_table[opposing_minors] * nqueen + pawn_material_adj_table[npawns] * npawns


    def phase():
        pieces = 0
        pieces += 2 * fen.count("N")
        pieces += 2* fen.count("B")
        pieces += 2*fen.count("R")
        pieces += 3*fen.count("Q")
        pieces += 2 * fen.count("n")
        pieces += 2 * fen.count("b")
        pieces += 2 * fen.count("r")
        pieces += 3 * fen.count("q")
        return pieces

    white_material = eval_for_side("w")
    black_material = eval_for_side("b")
    return (white_material - black_material) # + np.sign(white_material-black_material) * (40 - 40 * phase()/30)

def position_eval_grad(fen, params):
    fen = fen.split(" ")[0]
    def one_hot(idx, val=1, maxn=9):
        ls = [0.0] * maxn
        ls[idx] = val
        return ls
    def eval_for_side(side):
        if side == "w":
            npawns = fen.count("P")
            nknight = fen.count("N")
            nbishop = fen.count("B")
            nrook = fen.count("R")
            nqueen = fen.count("Q")
            opposing_minors = fen.count("n") + fen.count("b") + fen.count("r")
        else:
            npawns = fen.count("p")
            nknight = fen.count("n")
            nbishop = fen.count("b")
            nrook = fen.count("r")
            nqueen = fen.count("q")
            opposing_minors = fen.count("N") + fen.count("B") + fen.count("R")
        return np.array([
            nbishop, nbishop == 2, nrook == 2, nknight == 2] + one_hot(npawns, nknight) + one_hot(npawns, nrook) + one_hot(opposing_minors, nqueen, 7) + one_hot(npawns, npawns))
    
    return eval_for_side("w") - eval_for_side("b")


batch_size = 200
def grad_eval_params(params):
    scores = []
    grads = []
    res_v = []
    for _, (position, res) in zip(range(batch_size), position_iter):
        res_v.append(res)
        scores.append(position_eval(position, params))
        grads.append(position_eval_grad(position, params))
    scores = np.array(scores)
    grad = np.array(grads)
    res_v = np.array(res_v)
    scale = LN10 / 300
    return cross_entropy_and_grads(scores, grad, res_v, scale)


flattened_params = [300, 0, 0, 0,
                    250, 300, 300, 300, 300, 300, 300, 300, 300,
                    515, 500, 500, 500, 500, 500, 500, 500, 500,
                    900, 900, 900, 900, 900, 900, 900,
                    115, 100, 100, 100, 100, 100, 100, 100, 100,
                   ]
flattened_params = np.array(flattened_params, dtype=np.float32)
# flattened_params = np.zeros_like(flattened_params)
learning_rate = 1
warmup = 100
optimizer = Adam(flattened_params, l1_decay=0.1)

def learning_rate_schedule(i):
    if i < warmup:
        return i / warmup
    if i < 1000:
        return 1
    return np.sqrt(1000) / np.sqrt(i)

for i in range(1000000):
    scores = []
    try:
        score, grad = grad_eval_params(flattened_params)
        update = learning_rate * learning_rate_schedule(i) * optimizer.step(
            flattened_params, grad)
        scores.append(score)
        if i % 128 == 0:
            (bishop_v, bishop_pair_v, rook_pair_v, knight_pair_v,
             knight_material_adj_table, rook_material_adj_table,
             queen_material_adj_table, pawn_material_adj_table) = parse_params(flattened_params)
            pawn_v = pawn_material_adj_table[8]
            knight_v = np.mean(knight_material_adj_table)
            rook_v = np.mean(rook_material_adj_table)
            queen_v = np.mean(queen_material_adj_table)
            def normalize(s):
                return 100 * s / pawn_v
            print("%d Score: %s" % (i * batch_size, np.mean(scores)))
            print(pawn_v, knight_v, bishop_v, rook_v, queen_v,
                  bishop_pair_v, rook_pair_v, knight_pair_v,
                  knight_material_adj_table - knight_v,
                  rook_material_adj_table - rook_v,
                  queen_material_adj_table - queen_v
                 )
            print(normalize(knight_v),
                  normalize(bishop_v), normalize(rook_v),
                  normalize(queen_v),
                  normalize(bishop_pair_v),
                  normalize(rook_pair_v),
                  normalize(knight_pair_v),
                  normalize(knight_material_adj_table - knight_v),
                  normalize(rook_material_adj_table - rook_v),
                  normalize(queen_material_adj_table - queen_v),
                  normalize(pawn_material_adj_table - pawn_v)
                 )
            scores = []

        flattened_params -= update
        with open("training_loss.txt", "a") as f:
            f.write("%.3f " % score)
    except KeyboardInterrupt:
        break


# niters = 1000
# es = cma.CMAEvolutionStrategy(flattened_params, 0.5)
# while not es.stop():
#     X = es.ask()
#     es.tell(X, [eval_params(x) for x in X])
#     es.disp()
