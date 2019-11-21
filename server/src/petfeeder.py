#!/usr/bin/env python3

from datetime import datetime, timedelta
from pytz import timezone
import json
from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTClient
import logging
import time

valid_keys = ['weight','motion']


class PetFeeder:

	def __init__(self, serial_num=0, ip_addr='127.0.0.1', port=8883, dispense_amount=0, weight=0, dispense_times=None):
		self.serial_num = serial_num
		self.ip_addr = ip_addr
		self.port = port
		self.dispense_amount = dispense_amount
		self.weight = weight
		self.dispense_times = dispense_times
		if(self.dispense_times == None):
			print("Can not create PetFeeder without a dispense time list!")
			exit(2)
		self.root_cert = '../aws-auth/AmazonRootCA1.pem'
		self.private_key = '../aws-auth/951ad5141e-private.pem.key'
		self.cert = '../aws-auth/951ad5141e-certificate.pem.crt'
		self.pub_topic = None
		self.sub_topic = None		
		self.time_iter = 0
		self.ready = True

	def aws_init(self, pub_topic=None, sub_topic=None):
		self.aws_client = AWSIoTMQTTClient('petfeeder{}@{}'.format(self.serial_num, self.ip_addr))
		self.aws_client.configureEndpoint(self.ip_addr, self.port)
		self.aws_client.configureCredentials(self.root_cert, self.private_key, self.cert)
	
		self.aws_client.configureAutoReconnectBackoffTime(1, 32, 20)
		self.aws_client.configureOfflinePublishQueueing(-1)  # Infinite offline Publish queueing
		self.aws_client.configureDrainingFrequency(2)  # Draining: 2 Hz
		self.aws_client.configureConnectDisconnectTimeout(10)  # 10 sec
		self.aws_client.configureMQTTOperationTimeout(5)  # 5 sec
		self.aws_client.connect()
		self.pub_topic = pub_topic
		self.sub_topic = sub_topic
		self.aws_client.subscribe(sub_topic, 1, self.sub_cb)
	
	def update_schedule(self, msg_json):
		if(msg_json['update']['amount]']):
			print("Changing food dispense amount for {}@{}: {}".format(self.serial_num, self.ip_addr, msg_json['update']['amount']))
			self.dispense_amount = int(msg_json['update']['amount'])
			data = {}
			data['weight'] = self.dispense_amount
			jsonData = json.dumps(data)
			print("Publishing to {}@{}:\n{}".format(self.serial_num, self.ip_addr, jsonData))
			self.aws_client.publish(self.pub_topic, jsonData, 1)		
		if(msg_json['update']['time']):
			year = msg_json['update']['time']['year']
			month = msg_json['update']['time']['year']
			day = msg_json['update']['time']['day']
			hour = msg_json['update']['time']['hour']
			minute = msg_json['update']['time']['minute']
			timezone = msg_json['update']['time']['timezone']			
			
			date_str = "{}-{}-{} {}:{}:{} {}".format(year, month, day, hour, minute, timezone)
			new_time = datetime.strptime(date_str, "%Y-%m-%d %H:%M %Z%z")
			new_time_utc = new_time.astimezone('UTC')
			
			print("Changing dispense time for {}@{}: {}".format(self.serial_num, self.ip_addr, new_time_utc.time()))
			self.dispense_times = new_time_utc

	def is_dispense_time(self):
		if(not self.ready):
			return False
		delta = datetime.now().astimezone(timezone('utc')) - self.dispense_times[self.time_iter]
		if(delta.total_seconds() >= 0):
			self.ready = False
			return True
		else:
			return False
	
	def publish_msg(self, msg_json):
		print("Publishing message to {}@{}:\n{}".format(self.serial_num, self.ip_addr, json.dumps(msg_json, sort_keys=True, indent=4)))
		self.aws_client.publish(self.pub_topic, json.dumps(msg_json), 1)

	def sub_cb(self, client, userdata, message):
		msg_json = json.loads(message.payload)
		valid = 0;
		t = datetime.now().astimezone(timezone('utc'))
		print("{}: JSON received from {}@{}:\n".format(t, self.serial_num, self.ip_addr))
		print(json.dumps(msg_json))
		if('heartbeat' in msg_json):
			print("{}: Received heartbeat from {}@{}".format(t, self.serial_num, self.ip_addr))
			valid = 1
		if('weight' in msg_json):
			print("{}: Bowl weight read from {}@{}: {}".format(t, self.serial_num, self.ip_addr, msg_json['weight']))
			valid = 1
			self.weight = msg_json['weight']
		if('update' in msg_json):
			print("{}: Schedule update read from {}@{}: {}".format(t, self.serial_num, self.ip_addr, msg_json['update']))
			self.update_schedule(msg_json)
			valid = 1
		if('status' in msg_json):
			valid = 1
			print("{}: Received status from {}@{}: {}".format(t, self.serial_num, self.ip_addr, msg_json['status']))
			self.time_iter += 1
			self.ready = True
			if(self.time_iter >= len(self.dispense_times)):
				print("No more dispenses needed today. Adding one day to each scheduled time...")
				for index in range(len(self.dispense_times)):
					self.dispense_times[index] = self.dispense_times[index] + timedelta(days=1)
				self.time_iter = 0
		if('motion' in msg_json):
			print("{}: Motion sensor for {}@{}".format(t, self.serial_num, self.ip_addr))
		if(valid == 0):
			print("Invalid json:\n{}".format(json.dumps(msg_json, sort_keys=True, indent=4)))

if(__name__ == "__main__"):
	tmp = datetime.now().astimezone(timezone('US/Central'))
	disp_time = [tmp + timedelta(seconds=5), tmp + timedelta(seconds=50)]
	uut = PetFeeder(ip_addr='a2ot5vs3yt7xtc-ats.iot.us-west-2.amazonaws.com', serial_num='12345', port=8883, dispense_amount=100, weight=0, dispense_times=disp_time)
	print("Creating new pet-feeder for {}:{}".format(uut.ip_addr, uut.port))
	uut.aws_init(sub_topic="pet-feeder/to_aws", pub_topic="pet-feeder/from_aws")
	prev = datetime.now().astimezone(timezone('utc'))

	while True:
		delta = datetime.now().astimezone(timezone('utc')) - prev
		if(uut.is_dispense_time()):
			print("Dipsense time!")
			json_string = "{\"request\":[\"dispense\",\"weight\"]}"
			data = json.loads(json_string)
			print("String: {}".format(json_string))
			print("JSON: {}".format(json.dumps(data, sort_keys=True, indent=4)))
			uut.publish_msg(data)
		elif(delta.total_seconds() >= 60):
			print("Requesting information from pet-feeder")
			prev = datetime.now().astimezone(timezone('utc'))
			json_string = "{\"request\":[\"weight\"]}"
			data = json.loads(json_string)
			uut.publish_msg(data)
		time.sleep(0.5)

