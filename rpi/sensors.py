#!/usr/bin/env python3

import os
import numpy as np
import time

import picamera
import paho.mqtt.publish as publish

from sense_hat import SenseHat

camera = picamera.PiCamera()
sense = SenseHat()

def capture_image():
    image = np.empty((camera.resolution[1], camera.resolution[0], 3), dtype=np.uint8)
    camera.capture(image, "rgb")
    return image

def calc_brightness(image):
    gray = np.dot(image[...,:3], [0.2989, 0.5870, 0.1140])
    return gray

def temperature():
    return sense.get_remperature()


camera.exposure_mode = "off"
HOSTNAME = os.getenv("MQTT_HOSTNAME", "pi.ystre.org")

while True:
    brightness = np.mean(calc_brightness(capture_image())) / 256.0
    temp = temperature()
    print(brightness)
    print(temp)
    publish.single("/env/info/light", brightness, hostname=HOSTNAME)
    publish.single("/env/info/temp", temp, hostname=HOSTNAME)
    time.sleep(1)
