import numpy as np


def cross_entropy_and_grads(xs, grad_xs, yhat, scale):
    log_prob = np.where(
        xs > 0,
        -np.log(1 + np.exp(-scale * xs)),
        scale * xs - np.log(1 + np.exp(scale * xs)))

    log_prob_grad = np.where(
        (xs > 0)[:, None],
        (-1 / (1 + np.exp(-scale * xs)) * np.exp(-scale * xs) * (-scale))[:, None] * grad_xs,
        scale * grad_xs - (1 / (1 + np.exp(scale * xs)) * np.exp(scale * xs) * scale)[:, None] * grad_xs
    )

    log_other_prob = np.where(
        xs > 0,
        -scale * xs - np.log(1 + np.exp(-scale * xs)),
        -np.log(1 + np.exp (scale * xs)))

    log_other_prob_grad = np.where(
        (xs > 0)[:, None],
        -scale * grad_xs - (1 / (1 + np.exp(-scale * xs)) * np.exp(-scale * xs) * (-scale))[:, None] * grad_xs,
        (-1 / (1 + np.exp(scale * xs)) * np.exp(scale * xs) * scale)[:, None] * grad_xs
    )

    err = yhat * log_prob + (1 - yhat) * log_other_prob
    total_grad = yhat[:, None] * log_prob_grad + (1-yhat)[:, None] * log_other_prob_grad
    return -np.mean(err, axis=0), -np.mean(total_grad, axis=0)


class Adam(object):
    def __init__(self, params, b1=0.98, b2=0.999, eps=1e-8, l1_decay=0.0):
        self.m1 = np.zeros_like(params)
        self.m2 = np.zeros_like(params)
        self.b1 = b1
        self.b2 = b2
        self.eps = eps
        self.l1_decay = l1_decay
        self.i = 0

    def step(self, params, grad):
        self.m1 = self.b1 * self.m1 + (1 - self.b1) * grad
        self.m2 = self.b2 * self.m2 + (1 - self.b2) * np.square(grad)
        mhat = self.m1 / (1 - self.b1 ** (self.i + 1))
        vhat = self.m2 / (1 - self.b2 ** (self.i + 1))
        self.i += 1
        return mhat / (np.sqrt(vhat) + self.eps) + 0.001 * np.sign(params)

