import functools
import numpy as np
import scipy.optimize

def elo_prob(elo_delta):
    return 1 / (1 + np.exp(-elo_delta * np.log(10) / 400))

def log_likelihood(elo_delta, wins, draws, losses):
    pwin = elo_prob(elo_delta)
    return wins * np.log(pwin) + losses * np.log(1 - pwin) + 0.5 * draws * np.log(pwin) + 0.5 * draws * np.log(1 - pwin)

def estimate_elo_diff(wins, draws, losses, confidence=0.9):
    pwin = (wins + 0.5 * draws) / (wins + draws + losses)
    def pwin_to_elo(pwin):
        return -400 / np.log(10) * np.log(1 / pwin - 1)
    plower = pwin
    pupper = pwin

    for _ in range(100):
        plower = pwin - confidence * np.sqrt(plower * (1-plower) / (wins + draws + losses))
        pupper = pwin + confidence * np.sqrt(pupper * (1-pupper) / (wins + draws + losses))

    return pwin_to_elo(pwin), (pwin_to_elo(plower), pwin_to_elo(pupper))
