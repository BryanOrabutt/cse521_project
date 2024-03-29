import pickle
import numpy as np
import sys
from sklearn.decomposition import PCA as RandomizedPCA
from sklearn.model_selection import train_test_split
from sklearn.svm import SVC
from sklearn.metrics import confusion_matrix, classification_report
from sklearn.model_selection import GridSearchCV

import image_preprocessing

cat_names = ['Cat1', 'Cat2', 'Cat3']

# Load the images and class labels from files and split randomly into
# training and test sets

with open('ims.pkl', 'rb') as f:
    X = pickle.load(f)

with open('labels.pkl', 'rb') as f:
    y = pickle.load(f)

X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=15)

# Perform unsupervised dimensionality reduction using principal component analysis. 
# The resulting components are the eigenfaces. RandomizedPCA is faster on highly-dimensional data
# and produces very similar components to deterministic PCA.

pca = RandomizedPCA(n_components=16, whiten=True)
pca.fit(X_train)
X_train_tr = pca.transform(X_train)
X_test_tr = pca.transform(X_test)


# search for optimal SVM parameters using grid search with 3-fold cross validation

Cs = np.logspace(0, 4, 5)
gammas = np.logspace(-6, 1, 8)
param_grid = {'C': Cs, 'kernel': ['rbf'], 'gamma': gammas}
clf = GridSearchCV(estimator=SVC(), param_grid=param_grid,  n_jobs=-1, cv=3, iid=True)

clf.fit(X_train_tr, y_train)
y_hat = clf.predict(X_test_tr)

print(clf.best_estimator_)
print(classification_report(y_test, y_hat, target_names=cat_names))
print(confusion_matrix(y_test, y_hat))

# load a completely new image and classify it - done for demonstration purposes.

# with open('test_image.pkl', 'rb') as f:
#     test = pickle.load(f).reshape((1, -1))
test = image_preprocessing.crop_gray(sys.argv[1])

# test_fig = plt.figure(3)
# test_im = test_fig.add_subplot(1, 1, 1)
# test_im.imshow(test.reshape((64, 64)), cmap=plt.cm.gray)
# plt.show()

test = pca.transform(test)
print(cat_names[clf.predict(test)[0]])
