import matplotlib.pyplot as plt
import numpy as np

with open("training_loss.txt") as f:
    data = list(map(float, f.read().split()))

BINSIZE = 120
new_data = []
for i in range(0, len(data), BINSIZE):
    new_data.append(np.mean(data[i:i+BINSIZE]))
data = new_data
plt.scatter(list(range(len(data))), data)
plt.ylim((0.4, 0.45))
plt.show()
