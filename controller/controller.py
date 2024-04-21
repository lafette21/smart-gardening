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
        self._client = docker.APIClient() # .from_env() # .APIClient()
        self._container_db = dict()


    def create_container(self, image: str, name: str, start=False, **kwargs):
        container = self._client.create_container(image, name=name, **kwargs)
        self._container_db[name] = container

        if start:
            self._client.start(container["Id"])


    def containers(self):
        return [k for k in self._container_db]


    def clean_up(self):
        for k in self._container_db:
            self._client.stop(k)
            self._client.remove_container(k)
            logger.info(f"Removed container: {k}")


orchestrator = ContainerOrchestrator()
fba = firebase.FirebaseApplication("https://ertos-2024-default-rtdb.europe-west1.firebasedatabase.app/", None)

polytunnel_create_requests = []
containers = None

def on_polytunnel_create_requested(response):
    global polytunnel_create_requests

    if response is not None:
        new_keys = [key for key in response.keys() if key not in polytunnel_create_requests]
        for key in new_keys:
            polytunnel_create_requests.append(key)
            data = response[key]
            logger.info(f"Create request: {data}")
            orchestrator.create_container("polytunnel:latest", name=str(uuid.uuid4()), start=True, entrypoint="sleep 60")
            fba.post("/polytunnels/list", data=orchestrator.containers())


def temperature():
    # TODO: measure real temp with sense_hat
    return random.uniform(20, 30)


def brightness():
    # TODO: calculate brightness based on the image from the pycam
    return random.uniform(0, 1)


def main():
    """..."""
    args = parse_arguments()

    if args.verbose:
        logger.setLevel(logging.DEBUG)
    else:
        logger.setLevel(logging.INFO)

    global containers

    while True:
        fba.get_async("/polytunnels/create", None, callback=on_polytunnel_create_requested)

        data = fba.get("/polytunnels/list", None)
        if data is not None:
            data = data[max(data.keys())]
            if containers != data:
                containers = data
                logger.info(f"Containers: {containers}")

        fba.post("/environment/info", data={ "temperature": temperature(), "brightness": brightness() })

        time.sleep(1)


if __name__ == "__main__":
    try:
        main()
    except Exception as ex:
        logger.error(f"Exception: {ex}")
    finally:
        orchestrator.clean_up()

        fba.delete("/polytunnels", None)
        fba.delete("/environment", None)
