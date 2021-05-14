from logistic_regression import cross_entropy_and_grads, Adam
import numpy as np
import scipy.special

weights = np.array([1.34, -2.0, 0.5])
beta = 0.07

def ground_truth(x):
    return np.random.binomial(1, scipy.special.expit(np.sum(weights[None, :] * x, axis=1) + beta))

params = np.array([0, 0, 0, 0], dtype=np.float32)

optimizer = Adam(params, l1_decay=0.01)
for i in range(10000):
    Xs = np.random.normal(0, 1, size=(100, 3))
    yhat = ground_truth(Xs)
    loss, grad = cross_entropy_and_grads(
        np.sum(params[None, 1:] * Xs, axis=1) + beta + params[0],
        np.concatenate([np.ones((100, 1)), Xs], axis=1),
        yhat,
        2)
    params -= 1 / np.sqrt(i+1) * optimizer.step(params, grad)
    if i % 100 == 0:
        print(i, loss, params)
print("Ground truth: %s, %s" % (beta / 2, weights / 2))
        


