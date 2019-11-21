#!/usr/bin/env python3

import petfeeder
from datetime import datetime, timedelta
from pytz import timezone


device_list = None


while True:
    t = datetime.now().astimezone(timezone('utc'))
    if(devlice_list == None):
        print("No devices in the device_list.")
    else:
        for device in device_list:
            if(device.is_dispense_time()):
                print("{}: Dipsense time for {}@{}.".format(datetime.now().astimezone(t, device.serial_num, device.ip_addr))
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
