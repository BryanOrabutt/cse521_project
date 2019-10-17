import pickle
import sys
import numpy as np
from sklearn.decomposition import PCA as RandomizedPCA
from sklearn.model_selection import train_test_split
from sklearn.svm import SVC
from sklearn.metrics import confusion_matrix, classification_report
from sklearn.model_selection import GridSearchCV

num = input("How many cats are going to use this feeder?\n")
try:
    num = int(num)
except ValueError:
    sys.exit("Input is not correct, only integer will be accepted.")

names = []
for i in range(num):
    names.append(input(f"Cat {i}'s name is?\n"))
if len(set(names)) != len(names):
    sys.exit("Duplication in names.")