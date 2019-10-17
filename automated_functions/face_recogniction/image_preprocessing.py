import cv2
import sys
import numpy as np
import automated_functions.face_detection.cat_face_detection as detect

#Load the image
f= sys.argv[1]

face_cropped = detect.crop(f)




