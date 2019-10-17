import cv2
import sys
import cat_face_detection as detect

#Load the image

def crop_gray(f):

    face_cropped = detect.crop(f)

    resized_image = cv2.resize(face_cropped, (64, 64))

    gray = cv2.cvtColor(resized_image, cv2.COLOR_BGR2GRAY).reshape(1, 4096)

    return gray


