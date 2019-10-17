import cv2
import sys
import numpy as np


def show(f):
    faceCascade = cv2.CascadeClassifier("haarcascade_frontalcatface.xml")

    img = cv2.imread(f)
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

    faces = faceCascade.detectMultiScale(
        gray,
        scaleFactor=1.02,
        minNeighbors=3,
        minSize=(150, 150),
        flags=cv2.CASCADE_SCALE_IMAGE
    )

    for (x, y, w, h) in faces:
        cv2.rectangle(img, (x, y), (x + w, y + h), (0, 0, 255), 2)
        cv2.putText(img, 'Cat', (x, y - 7), 3, 1.2, (0, 255, 0), 2, cv2.LINE_AA)

    cv2.imshow('Cat?', img)
    cv2.imwrite("cat33.jpg", img)
    c = cv2.waitKey(0)


def crop(f: str) -> np.ndarray:
    faceCascade = cv2.CascadeClassifier("haarcascade_frontalcatface.xml")

    img = cv2.imread(f)
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

    faces = faceCascade.detectMultiScale(
        gray,
        scaleFactor=1.02,
        minNeighbors=3,
        minSize=(150, 150),
        flags=cv2.CASCADE_SCALE_IMAGE
    )

    for (x, y, w, h) in faces:
        crop_img = img[y:y + h, x:x + w]
        return crop_img


if __name__ == '__main__':
    try:
        f = sys.argv[1]
    except IndexError:
        f = "../face_recogniction/cat1.png"

    show(f=f)
