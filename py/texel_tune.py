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
# Tuning order:
# 1. pawns (done)
# 2. pawn pst
# 3. King open file, king closed file, king_attacker table, pawnshield
# 4. King pst, can_castle
# 5. knights: Knight outpost, pst, attack_count_table
# 6. bishops: bishop_obstruction_table and bishop_own_obstruction_table, attack_count_table, outpost
# 7. bishops: pst
# 8. Rooks: open files, attack_count_table, Tarasch, outpost
# 9. Rooks: PST
# 10. Queen: mobility table
# 11. Space table

TUNE_PARAMS = [
    # "MIDGAME_PAWN_VALUE",
    # "ENDGAME_PAWN_VALUE",
    # "passed_pawn_table",
    # "passed_pawn_table_endgame",
    # "passed_pawn_blockade_table",
    # "passed_pawn_blockade_table_endgame",
    # "isolated_pawn_penalty",
    # "doubled_pawn_penalty",
    # "MIDGAME_SUPPORTED_PAWN",
    # "MIDGAME_BACKWARD_PAWN",
    # "pawn_table",
    # "pawn_table_endgame",
    # "bishop_table",
    # "rook_table",
    # "queen_table",
    # "king_table",
    # "king_table_endgame",
    # "CAN_CASTLE_BONUS",
    # "king_attacker_table",
    # "pawn_shield_table",
    # "KING_OPEN_FILE_PENALTY",
    # "KING_SEMIOPEN_FILE_PENALTY",
    # "attack_count_knight",
    # "KNIGHT_ALMOST_OUTPOST_BONUS",
    # "KNIGHT_OUTPOST_BONUS",
    # "knight_table",
    # "attack_count_bishop",
    # "BISHOP_ALMOST_OUTPOST_BONUS",
    # "BISHOP_OUTPOST_BONUS",
    # "bishop_obstruction_table",
    # "bishop_own_obstruction_table",
    # "bishop_table",
    # "ROOK_OUTPOST_BONUS",
    # "ROOK_OPENFILE",
    # "ROOK_SEMIOPENFILE",
    # "ROOK_BLOCKED_FILE",
    # "ROOK_TARASCH_BONUS",
    # "attack_count_table_rook",
    # "rook_table",
    # "attack_count_table_queen",
    # "queen_table",
    # "KING_XRAYED",
    # "QUEEN_XRAYED",
    # "PINNED_PENALTY",
    # "CASTLE_OBSTRUCTION_PENALTY",
    # "CFDE_PAWN_BLOCK",
    "doubled_pawn_penalty",
    "space_table",
]
SYMMETRIFY = ["knight_table", "bishop_table",
              "rook_table", "queen_table",
              "pawn_table", "king_table_endgame",
              "pawn_table_endgame",
              "doubled_pawn_penalty",
              "isolated_pawn_penalty", "king_table"]

with open("params.json.backup") as f:
    params = json.load(f)
global_params = params

RES_TO_SCORE = {
    "1-0": 1.0,
    "0-1": 0.0,
    "1/2-1/2": 0.5,
}
def ds_iter():
    with open("quiet-labeled.epd") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split("\"")
            pos = parts[0][:-4] + " 0 0"
            res = RES_TO_SCORE[parts[1]]
            yield pos, res



# def ds_iter():
#     with open("gm_positions.txt") as f:
#         for i, line in enumerate(f):
#             line = line.strip()
#             if not line:
#                 continue
#             parts = line.split("[")
#             pos = parts[0]
#             score = float(parts[1][:-1])
#             yield pos, score

position_iter = itertools.cycle(ds_iter())


def process_params(params):
    processed_params = {}
    for key in param_keys:
        if key not in TUNE_PARAMS:
            continue
        p = params[key]
        if not isinstance(p, list):
            processed_params[key] = [p]
        else:
            if key in SYMMETRIFY:
                out = np.array(p, dtype=np.int32)
                out = np.reshape(out, [-1, 8])[:, :4]
                out = np.reshape(out, [-1])
                processed_params[key] = list(map(float, out))
            else:
                processed_params[key] = p
    return processed_params

tuner_engine = subprocess.Popen(
    ["./tuner_eval"], stdout=subprocess.PIPE, stdin=subprocess.PIPE)
time.sleep(1)

def position_eval(fen):
    tuner_engine.stdin.write(("fen " + fen + "\n").encode("utf-8"))
    tuner_engine.stdin.flush()
    ret = tuner_engine.stdout.readline()
    return int(ret)

def symmetrize_board(l):
    for row in range(len(l) // 8):
        for col in range(4):
            avg = int((l[row * 8 + col] + l[row * 8 + 7-col]) / 2)
            l[row * 8 + col] = avg
            l[row * 8 + 7 - col] = avg

LN10 = np.log(10)


def write_params(params, name="params.json"):
    param_dict = {}
    idx = 0
    for key, shape in zip(param_keys, param_shapes):
        if shape == 1:
            param_dict[key] = int(params[idx])
        else:
            param_dict[key] = list(map(int, params[idx:idx+shape]))
            if key in SYMMETRIFY:
                out = np.array(param_dict[key], dtype=np.int32)
                out = np.reshape(out, [-1, 4])
                out = np.concatenate([out, np.flip(out, axis=1)], axis=1)
                param_dict[key] = list(map(int, np.reshape(out, [-1])))
        idx += shape
    with open(name, "w") as f:
        f.write(json.dumps(param_dict, indent=4))
    tuner_engine.stdin.write("load\n".encode("utf-8"))
    tuner_engine.stdin.flush()


batch_size = 320
def grad_eval_params(params):
    def compute_score_grads():
        positions = []
        res_v = []
        for i, (position, res) in zip(range(batch_size), position_iter):
            positions.append(position)
            res_v.append(res)
        res_v = np.array(res_v)

        all_scores = []
        all_score_derivs = []

        write_params(params)
        for position in positions:
            all_scores.append(position_eval(position))

        grad_list = []
        for i, p in enumerate(params):
            params[i] = p + 2
            score_p1s = []
            write_params(params)
            for position in positions:
                score = position_eval(position)
                score_p1s.append(score)
            score_deriv = [(score_p1 - score) / 2 for score_p1, score in zip(score_p1s, all_scores)]
            grad_list.append(score_deriv)
            params[i] = p

        scores = np.array(all_scores) * 1.35
        grad = np.transpose(np.array(grad_list))
        return scores, grad, res_v
    scores, grad, res_v = compute_score_grads()
    return cross_entropy_and_grads(scores, grad, res_v, LN10 / 300)

param_keys = sorted(list(k for k in params.keys() if k in TUNE_PARAMS))
processed_params = process_params(params)
param_shapes = [len(processed_params.get(k, [])) for k in param_keys]
flattened_params = sum(
     (processed_params.get(k, []) for k in param_keys), [])

flattened_params = np.array(flattened_params, dtype=np.float32)
flattened_params = np.zeros_like(flattened_params)
learning_rate = 0.5
optimizer = Adam(flattened_params, l1_decay=0.001)
warmup = 50

def learning_rate_schedule(i):
    if i < warmup:
        return i / warmup
    if i < 1000:
        return 1
    return np.sqrt(1000) / np.sqrt(i)

for i in range(0, 1000000):
    tuner_engine.kill()
    tuner_engine = subprocess.Popen(
        ["./tuner_eval"], stdout=subprocess.PIPE, stdin=subprocess.PIPE)
    try:
        score, grad = grad_eval_params(flattened_params)
        update = learning_rate * learning_rate_schedule(i) * optimizer.step(flattened_params, grad)
        print("%d Score: %s, %s, %s" % (i * batch_size, score, np.max(np.abs(update)), np.mean(np.abs(update))))
        flattened_params -= update
        write_params(flattened_params, name="params_best.json")
        with open("training_loss.txt", "a") as f:
            f.write("%.3f " % score)
    except KeyboardInterrupt:
        break
    except IOError:
        print("Pipe error")
        pass
    except ValueError:
        print("Value error")
        pass


# niters = 1000
# es = cma.CMAEvolutionStrategy(flattened_params, 0.5)
# while not es.stop():
#     X = es.ask()
#     es.tell(X, [eval_params(x) for x in X])
#     es.disp()
