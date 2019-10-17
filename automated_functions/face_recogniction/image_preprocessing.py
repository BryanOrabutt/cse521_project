#Load and show an image with Pillow
from PIL import Image
import numpy as np
import matplotlib.pyplot as plt

#Load the image
img = Image.open('cat1.png')

#Get basic details about the image
# print(img.format)
# print(img.mode)
# print(img.size)
print(img)

pix = np.array(img.convert('L'))

print(pix)

#show the image
plt.imshow(pix, cmap=plt.cm.gray)
plt.show()