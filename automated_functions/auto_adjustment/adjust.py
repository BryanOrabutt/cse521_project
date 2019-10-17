import numpy as np

with open('weight.csv', 'r') as f:
    data = [float(x) for x in f.readline().split(", ")]


print(data)