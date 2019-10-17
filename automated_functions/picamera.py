"""
The script running on raspberry pi to take a picture and
1) do facial recognition locally or 2) send to cload for processing

"""


from picamera import PiCamera
from time import sleep
from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTClient


camera = PiCamera()
camera.resolution = (2592, 1944)

# while
camera.start_preview()
sleep(5)
camera.capture('/home/pi/Desktop/gx_folder/image.jpg')
camera.stop_preview()
