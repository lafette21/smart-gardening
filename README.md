# smart-gardening

## Project structure

```
.
├── LICENSE
├── README.md
├── api
│   └── asyncapi.yaml
├── controller
│   ├── controller.py
│   ├── fba_tester.py
│   └── requirements.txt
├── polytunnel
│   ├── Dockerfile
│   ├── measure_simulation.h
│   ├── measure_simulation_test.h
│   └── polytunnel.c
└── rpi
    ├── requirements.txt
    └── sensors.py
```

## Requirements

For each component written in Python there's a `requirements.txt` file containing the dependencies.
> NOTE: There is one missing dependency in the `rpi` component called `picamera`.

For developing and running the `polytunnel` component there's a Dockerfile under the `polytunnel` folder.

### Building the Docker image

```
cd polytunnel
docker build -t polytunnel .
```
> NOTE: Docker may require you to install the `buildx` component for the image building.

## Running the solution

### Controller

```
cd polytunnel
FIREBASE_HOST=<CUSTOM FIREBASE HOST> MQTT_HOST=<CUSTOM MQTT HOST> python3 controller.py
```

### RPI

```
cd rpi
MQTT_HOST=<CUSTOM MQTT HOST> python3 sensors.py
```
