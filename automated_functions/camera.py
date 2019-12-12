"""
The script running on raspberry pi to take a picture and
1) do facial recognition locally or
2) send to cloud for processing

"""

from picamera import PiCamera
import datetime
from time import sleep
from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTClient

camera = PiCamera()
camera.resolution = (2592, 1944)

root_cert = '../aws-auth/AmazonRootCA1.pem'
private_key = '../aws-auth/951ad5141e-private.pem.key'
cert = '../aws-auth/951ad5141e-certificate.pem.crt'

def sub_cb(self):
	camera.start_preview()
	sleep(1)
	camera.capture('/home/pi/Desktop/gx_folder/image.jpg')
	camera.stop_preview()
	aws_client.publish(self.pub_topic, "message_received", 0)


ip_addr='a2ot5vs3yt7xtc-ats.iot.us-west-2.amazonaws.com'
port=8883
aws_client = AWSIoTMQTTClient("motion")
aws_client.configureEndpoint(ip_addr, port)
aws_client.configureCredentials(root_cert, private_key, cert)

aws_client.configureAutoReconnectBackoffTime(1, 32, 20)
aws_client.configureOfflinePublishQueueing(-1)  # Infinite offline Publish queueing
aws_client.configureDrainingFrequency(2)  # Draining: 2 Hz
aws_client.configureConnectDisconnectTimeout(10)  # 10 sec
aws_client.configureMQTTOperationTimeout(5)  # 5 sec
aws_client.connect()
sub_topic = "pet-feeder/motion"
pub_topic = "pet-feeder/test"
aws_client.subscribe(sub_topic, 1, sub_cb)







