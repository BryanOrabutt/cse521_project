import numpy as np
import sys


if __name__ == '__main__':

    with open('weight.csv', 'r') as f:
        data = [float(x) for x in f.readline().split(", ")]

    average = np.mean(data)
    new_entry=int(sys.argv[1])

    diff =new_entry-average

    if diff/average > 0.2:
        print("Pet eats too much")
    elif diff / average < -0.2:
        print("Pet eats too less")
    else:
        print("Diet is normal")
        data.append(new_entry)
        

