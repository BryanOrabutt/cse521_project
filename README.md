# cse521_project

ESP32 code for the project is located in espressif_code/pet-feeder/main. To build the ESP32 code first get the ESP-IDF following these instructions https://docs.espressif.com/projects/esp-idf/en/v3.3/get-started/index.html. Then simply enter the pet-feeder directory and run `make menuconfig` and configure your programmer and WiFi SSID. Then run `make flash -j4` to program the device.

The directory server/src/ contains the code to run the dispense scheduler. This can also be run locally without an AWS EC2 instance. Simply install AWSIoTPythonSDK.MQTTLib with pip and run the petfeeder.py program.
