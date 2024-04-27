#!/usr/bin/env python3

import argparse
import logging
import random
import time
import uuid

from enum import Enum

import docker
import paho.mqtt.client as mqtt

from firebase import firebase


class AnsiColors(Enum):
    DARK_GREY = "\x1b[30;1m"
    RED       = "\x1b[31;20m"
    BOLD_RED  = "\x1b[31;1m"
    GREEN     = "\x1b[32;20m"
    YELLOW    = "\x1b[33;20m"
    BLUE      = "\x1b[34;20m"
    PURPLE    = "\x1b[35;20m"
    CYAN      = "\x1b[36;20m"
    WHITE     = "\x1b[37;1m"
    GREY      = "\x1b[38;20m"
    DEFAULT   = "\x1b[0m"


class ColorLogFormatter(logging.Formatter):
    formats = {
        logging.DEBUG:    "%(asctime)s [%(name)s] {}%(levelname)s:{} %(message)s".format(AnsiColors.BLUE.value, AnsiColors.DEFAULT.value),
        logging.INFO:     "%(asctime)s [%(name)s] {}%(levelname)s:{} %(message)s".format(AnsiColors.GREEN.value, AnsiColors.DEFAULT.value),
        logging.WARNING:  "%(asctime)s [%(name)s] {}%(levelname)s:{} %(message)s".format(AnsiColors.YELLOW.value, AnsiColors.DEFAULT.value),
        logging.ERROR:    "%(asctime)s [%(name)s] {}%(levelname)s:{} %(message)s".format(AnsiColors.RED.value, AnsiColors.DEFAULT.value),
        logging.CRITICAL: "%(asctime)s [%(name)s] {}%(levelname)s:{} %(message)s".format(AnsiColors.BOLD_RED.value, AnsiColors.DEFAULT.value)
    }

    def format(self, record):
        log_fmt = self.formats.get(record.levelno)
        formatter = logging.Formatter(log_fmt)
        return formatter.format(record)


log_handler = logging.StreamHandler()
log_handler.setFormatter(ColorLogFormatter())

logger = logging.getLogger("controller")
logger.addHandler(log_handler)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawTextHelpFormatter
    )

    parser.add_argument("-v", "--verbose", action="store_true")

    return parser.parse_args()


class ContainerOrchestrator:
    """"""

    def __init__(self):
        """"""
        self._client = docker.APIClient()
        self._container_db = dict()


    def create_container(self, image: str, name: str, start=False, **kwargs):
        container = self._client.create_container(image, name=name, **kwargs)
        self._container_db[name] = container

        if start:
            self._client.start(container["Id"])


    def delete_container(self, name: str):
        self._client.stop(name)
        self._client.remove_container(name)
        self._container_db.pop(name)


    def containers(self):
        return [k for k in self._container_db]


    def clean_up(self):
        for k in self._container_db:
            self._client.stop(k)
            self._client.remove_container(k)
            logger.info(f"Removed container: {k}")


orchestrator = ContainerOrchestrator()
fba = firebase.FirebaseApplication("https://ertos-2024-default-rtdb.europe-west1.firebasedatabase.app/", None)
mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

intervention_requests = []
polytunnel_create_requests = []
polytunnel_delete_requests = []
containers = None
polytunnel_schemas = dict()


def on_intervention_requested(response):
    global intervention_requests

    if response is not None:
        new_keys = [key for key in response.keys() if key not in intervention_requests]
        for key in new_keys:
            intervention_requests.append(key)
            data = response[key]
            mqttc.publish(f"/polytunnels/{data['uuid']}/intervene/devices/{data['device']}", None)


def on_polytunnel_create_requested(response):
    global polytunnel_create_requests
    global polytunnel_schemas

    if response is not None:
        new_keys = [key for key in response.keys() if key not in polytunnel_create_requests]
        for key in new_keys:
            polytunnel_create_requests.append(key)
            data = response[key]
            logger.info(f"Create request: {data}")
            id = str(uuid.uuid4())
            orchestrator.create_container(
                "polytunnel:latest",
                name=id,
                start=True,
                environment={ "UUID": id },
                entrypoint="sh -c 'echo $UUID'",
            )
            polytunnel_schemas[id] = data
            mqttc.subscribe(f"/polytunnels/{id}/#")
            fba.post("/polytunnels/list", data=orchestrator.containers())


def on_polytunnel_delete_requested(response):
    global polytunnel_delete_requests
    global polytunnel_schemas

    if response is not None:
        new_keys = [key for key in response.keys() if key not in polytunnel_delete_requests]
        for key in new_keys:
            polytunnel_delete_requests.append(key)
            id = response[key]
            logger.info(f"Delete request: {id}")
            polytunnel_schemas.pop(id)
            polytunnel_fba_channels.pop(id)
            orchestrator.delete_container(id)
            mqttc.unsubscribe(f"/polytunnels/{id}/#")
            fba.post("/polytunnels/list", data=orchestrator.containers() if len(orchestrator.containers()) > 0 else "")


def on_log(client, userdata, paho_log_level, message):
    if paho_log_level == mqtt.LogLevel.MQTT_LOG_ERR:
        logger.error(message)
    elif paho_log_level == mqtt.LogLevel.MQTT_LOG_WARNING:
        logger.warn(message)
    elif paho_log_level == mqtt.LogLevel.MQTT_LOG_NOTICE:
        logger.info(message)
    elif paho_log_level == mqtt.LogLevel.MQTT_LOG_INFO:
        logger.info(message)
    elif paho_log_level == mqtt.LogLevel.MQTT_LOG_DEBUG:
        logger.debug(message)


def on_subscribe(client, userdata, mid, reason_code_list, properties):
    for rc in reason_code_list:
        if rc.is_failure:
            logger.error(f"Broker rejected you subscription: {rc}")
        else:
            logger.info(f"Broker granted the following QoS: {rc.value}")


def on_unsubscribe(client, userdata, mid, reason_code_list, properties):
    # Be careful, the reason_code_list is only present in MQTTv5.
    # In MQTTv3 it will always be empty

    if len(reason_code_list) == 0:
        logger.info("Unsubscribe succeeded")

    for rc in reason_code_list:
        if rc.is_failure:
            logger.info("Unsubscribe succeeded")
        else:
            logger.error(f"Broker replied with failure: {rc}")


def on_message(client, userdata, message):
    global polytunnel_schemas

    userdata[message.topic] = message.payload
    logger.info(f"Topic: {message.topic}\tPayload: {message.payload}")
    if message.topic.startswith("/env/info/"):
        fba.post(message.topic, data=message.payload.decode("utf-8"))
    elif "crop" in message.topic:
        value = float(message.payload.decode("utf-8"))
        id = message.topic.split("/")[2]
        schema = polytunnel_schemas[id]
        attrib = schema["crop"]["attributes"]
        if "temp" in message.topic:
            min = attrib["temp"]["range"]["min"]
            max = attrib["temp"]["range"]["max"]
        elif "light" in message.topic:
            min = attrib["light"]["range"]["min"]
            max = attrib["light"]["range"]["max"]
        elif "humidity" in message.topic:
            min = attrib["humidity"]["range"]["min"]
            max = attrib["humidity"]["range"]["max"]
        elif "soilMoisture" in message.topic:
            min = attrib["soilMoisture"]["range"]["min"]
            max = attrib["soilMoisture"]["range"]["max"]
        status = "Normal" if min <= value and value <= max else "Critical"
        fba.post(message.topic, data={ "status": status, "value": value })


def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code.is_failure:
        logger.error(f"Failed to connect: {reason_code}. loop_forever() will retry connection")
    else:
        logger.info("Successfully connected!")


def main():
    """..."""
    args = parse_arguments()

    if args.verbose:
        logger.setLevel(logging.DEBUG)
    else:
        logger.setLevel(logging.INFO)

    mqttc.on_connect = on_connect
    mqttc.on_log = on_log
    mqttc.on_message = on_message
    mqttc.on_unsubscribe = on_unsubscribe
    mqttc.on_subscribe = on_subscribe

    mqtt_data = dict()
    mqttc.user_data_set(mqtt_data)
    mqttc.connect("pi.ystre.org")

    mqttc.subscribe("/env/info/light")
    mqttc.subscribe("/env/info/temp")

    mqttc.loop_start()

    global containers

    while True:
        fba.get_async("/polytunnels/create", None, callback=on_polytunnel_create_requested)
        fba.get_async("/polytunnels/delete", None, callback=on_polytunnel_delete_requested)
        fba.get_async("/polytunnels/intervene", None, callback=on_intervention_requested)

        data = fba.get("/polytunnels/list", None)
        if data is not None:
            data = data[max(data.keys())]
            if containers != data:
                containers = data
                logger.info(f"Containers: {containers}")

        time.sleep(1)


if __name__ == "__main__":
    try:
        main()
    except Exception as ex:
        logger.error(f"Exception: {ex}")
    finally:
        orchestrator.clean_up()

        fba.delete("/polytunnels", None)
        fba.delete("/env", None)

        mqttc.disconnect()
