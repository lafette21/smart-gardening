#include <stdio.h>
#include <stdbool.h>
#include <mosquitto.h>
#include <signal.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "./measure_simulation.h"

// Mosquitto
#define MQTT_HOSTNAME "localhost"
#define MQTT_PORT 1884
#define MQTT_CLIENTID "erts_sub"
#define MQTT_TOPIC "hello"

#define TIME_BETWEEN_MEASUREMENTS 2 // sec
#define ID_CHARACTER_LENGTH 5 // without terminating null

struct callback_params {
	float *humidity;
	float *soil_moisture;
	float *temperature;
	float *outer_temperature;

	bool *sprinkler_on;
	bool *heater_on;
	bool *lamp_on;
};

// Mosquitto
void message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message);
void connect_callback(struct mosquitto *mosq, void *userdata, int result);
void subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_ops);
void subscribe_to_topic(char *topic_snd_part, bool full_url);
void send_message(char *topic_snd_part, char *payload);

// measurements
void send_measurements();
void take_measurements(float *humidity, float *temperature, float* outer_temperature, float *soil_moisture, const bool *sprinler_on, 
							const bool *heater_on, const bool *lamp_on, const float elapsed_seconds);
void measure_humidity(float *humidity, const float *soil_moisture, const float *temperature, const bool *sprinkler_on, const float elapsed_seconds);
void measure_soil_moisture(float *soil_moisture, const float *temperature, const bool *sprinkler_on, const float elapsed_seconds);
void measure_temperature(float *temperature, const float *outer_temperature, const bool *heater_on, const bool *lamp_on, const float elapsed_seconds);

// create shared memory ids
unsigned int humidity_shared_mem_id = 0;
unsigned int temperature_shared_mem_id = 0;
unsigned int soil_moisture_shared_mem_id = 0;
unsigned int outer_temperature_shared_mem_id = 0;

unsigned int sprinkler_on_shared_mem_id = 0;
unsigned int heater_on_shared_mem_id = 0;
unsigned int lamp_on_shared_mem_id = 0;

struct mosquitto *mosq = NULL;

char poly_id[5];

int main(int argc, char *argv[])
{
	if (argc < 2) 
	{
		printf("ID is missing!\n");
		exit(1);
	}

	if (strlen(argv[1]) > ID_CHARACTER_LENGTH)
	{
		printf("ID's maximal length is %d, given length is: %ld\n", ID_CHARACTER_LENGTH, strlen(argv[1]));
		exit(1);
	}

	strcpy(poly_id, argv[1]);

	humidity_shared_mem_id = shmget(1000, sizeof(float), IPC_CREAT | S_IRUSR | S_IWUSR);
	temperature_shared_mem_id = shmget(2000, sizeof(float), IPC_CREAT | S_IRUSR | S_IWUSR);
	soil_moisture_shared_mem_id = shmget(3000, sizeof(float), IPC_CREAT | S_IRUSR | S_IWUSR);
	outer_temperature_shared_mem_id = shmget(4000, sizeof(float), IPC_CREAT | S_IRUSR | S_IWUSR);

	sprinkler_on_shared_mem_id = shmget(5000, sizeof(bool), IPC_CREAT | S_IRUSR | S_IWUSR);
	heater_on_shared_mem_id = shmget(6000, sizeof(bool), IPC_CREAT | S_IRUSR | S_IWUSR);
	lamp_on_shared_mem_id = shmget(7000, sizeof(bool), IPC_CREAT | S_IRUSR | S_IWUSR);

	pid_t child = fork();

	if (child > 0) // Parent
	{
		// shared memory
		float *humidity = shmat(humidity_shared_mem_id, NULL, 0);
		float *temperature = shmat(temperature_shared_mem_id, NULL, 0);
		float *soil_moisture = shmat(soil_moisture_shared_mem_id, NULL, 0);
		float *outer_temperature = shmat(outer_temperature_shared_mem_id, NULL, 0);

		*humidity = DEFAULT_HUMIDITY;
		*temperature = DEFAULT_TEMPERATURE;
		*soil_moisture = DEFAULT_SOIL_MOISTURE;
		*outer_temperature = DEFAULT_OUTER_TEMPERATURE;

		bool *sprinkler_on = shmat(sprinkler_on_shared_mem_id, NULL, 0);
		bool *heater_on = shmat(heater_on_shared_mem_id, NULL, 0);
		bool *lamp_on = shmat(lamp_on_shared_mem_id, NULL, 0);

		*sprinkler_on = false;
		*heater_on = false;
		*lamp_on = false;

		// timer
		clock_t last_update = clock();
		clock_t elapsed_time = 0; // elapsed time from last update

		while (true)
		{
			elapsed_time = clock() - last_update;
			float elapsed_seconds = ((double)elapsed_time / CLOCKS_PER_SEC);

			if (elapsed_seconds >= TIME_BETWEEN_MEASUREMENTS)
			{
				take_measurements(humidity, temperature, outer_temperature, soil_moisture, sprinkler_on, heater_on, lamp_on, elapsed_seconds);
				kill(child, SIGRTMAX);
				last_update = clock();
			}

		}

		exit(0);
	}
	else // Child
	{
		//signals
		signal(SIGRTMAX, send_measurements);

		float *humidity = shmat(humidity_shared_mem_id, NULL, SHM_RDONLY);
		float *soil_moisture = shmat(soil_moisture_shared_mem_id, NULL, SHM_RDONLY);
		float *temperature = shmat(temperature_shared_mem_id, NULL, SHM_RDONLY);
		float *outer_temperature = shmat(outer_temperature_shared_mem_id, NULL, 0);
		bool *sprinkler_on = shmat(sprinkler_on_shared_mem_id, NULL, 0);
		bool *heater_on = shmat(heater_on_shared_mem_id, NULL, 0);
		bool *lamp_on = shmat(lamp_on_shared_mem_id, NULL, 0);
		
		struct callback_params params = {humidity, soil_moisture, temperature, outer_temperature, sprinkler_on, heater_on, lamp_on};
		mosquitto_lib_init();

		mosq = mosquitto_new(NULL, true, &params);
		if (!mosq)
		{
		 	fprintf(stderr, "Out of memory.\n");
		 	exit(1);
		}
		mosquitto_connect_callback_set(mosq, connect_callback);
    	mosquitto_message_callback_set(mosq, message_callback);
    	mosquitto_subscribe_callback_set(mosq, subscribe_callback);

		if (mosquitto_connect(mosq, MQTT_HOSTNAME, MQTT_PORT, 60)) // keepalive=60
    	{
    	    printf("Unable to connect.\n");
    	    exit(1);
    	}

		mosquitto_loop_forever(mosq, -1, 1);

		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();
		exit(0);
	}

	exit(0);
}

void send_measurements()
{
	printf("Sending measurements.\n");

	float *humidity = shmat(humidity_shared_mem_id, NULL, SHM_RDONLY);
	float *soil_moisture = shmat(soil_moisture_shared_mem_id, NULL, SHM_RDONLY);
	float *temperature = shmat(temperature_shared_mem_id, NULL, SHM_RDONLY);
	bool *sprinkler_on = shmat(sprinkler_on_shared_mem_id, NULL, 0);
	bool *heater_on = shmat(heater_on_shared_mem_id, NULL, 0);
	bool *lamp_on = shmat(lamp_on_shared_mem_id, NULL, 0);

	char humidity_str[6];
	sprintf(humidity_str, "%5.2f", *humidity);

	char soil_moisture_str[6];
	sprintf(soil_moisture_str, "%5.2f", *soil_moisture);

	char temperature_str[6];
	sprintf(temperature_str, "%5.2f", *temperature);

	char sprinkler_on_str[2];
	sprintf(sprinkler_on_str, "%d", *sprinkler_on);

	char heater_on_str[2];
	sprintf(heater_on_str, "%d", *heater_on);

	char lamp_on_str[2];
	sprintf(lamp_on_str, "%d", *lamp_on);

	send_message("/info/crop/attrib/humidity", humidity_str);
	send_message("/info/crop/attrib/soil-moisture", soil_moisture_str);
	send_message("/info/crop/attrib/temp", temperature_str);
	send_message("/info/devices/sprinkler", sprinkler_on_str);
	send_message("/info/devices/heater", heater_on_str);
	send_message("/info/devices/lamp", lamp_on_str);
}

void send_message(char *topic_snd_part, char *payload)
{
	int mid_sent = 0;
	char topic[60];
	strcpy(topic, "/polytunnels/");
	strcat(topic, poly_id);
	strcat(topic, topic_snd_part);
	mosquitto_publish(mosq,&mid_sent, topic, strlen(payload), payload, 0, 0);
}

void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	struct callback_params *params = (struct callback_params *)obj;

	printf("----------------------------------------------------------------\n");
	printf("Message arrived!\n");
	printf("----------------------------------------------------------------\n");

    if (message->payloadlen == 0)
    {
		if (strstr(message->topic, "/intervene/devices/sprinkler") != NULL)
		{
			printf("switching sprinkler\n");
			*params->sprinkler_on = !*params->sprinkler_on;
		}
		else if (strstr(message->topic, "/intervene/devices/heater") != NULL)
		{
			printf("switching heater\n");
			*params->heater_on = !*params->heater_on;
		}
		else if (strstr(message->topic, "/intervene/devices/lamp") != NULL)
		{
			printf("switching lamp\n");
			*params->lamp_on = !*params->lamp_on;
		}
		else
		{
			printf("Unknown message. Topic: %s, payload: %s\n", (char*)message->topic, (char*)message->payload);
		}
    }
    else if (strcmp(message->topic, "/env/info/temp") == 0)
    {
		printf("halohalo\n");
		printf("%s\n", (char*)message->payload);
		printf("float: %6.3f\n", strtof((char*)message->payload, NULL));

		printf("curr out_temp: %10.5f\n", *params->outer_temperature);
		*params->outer_temperature = strtof((char*)message->payload, NULL);
    }
    fflush(stdout);
}

void connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
    int i;
    if (!result)
    {
        /* Subscribe to broker information topics on successful connect. */
		subscribe_to_topic("/intervene/#", false);
		subscribe_to_topic("/env/info/temp", true);
    }
    else
    {
        printf("Connect failed\n");
    }
}

void subscribe_to_topic(char *topic_part, bool full_url)
{
	char topic[60];
	if (full_url)
	{
		strcpy(topic, topic_part);
	} 
	else 
	{
		strcpy(topic, "/polytunnels/");
		strcat(topic, poly_id);
		strcat(topic, topic_part);
	}
    mosquitto_subscribe(mosq, NULL, topic, 2);
}

void subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
    int i;

    printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
    for (i = 1; i < qos_count; i++)
    {
        printf(", %d", granted_qos[i]);
    }
    printf("\n");
}

void take_measurements(float *humidity, float *temperature, float* outer_temperature, float *soil_moisture, const bool *sprinkler_on, 
							const bool *heater_on, const bool *lamp_on, const float elapsed_seconds) 
{
	printf("-------------------------------------------------------------------------------------------------\n");
	printf("measuring\n");

	measure_humidity(humidity, soil_moisture, temperature, sprinkler_on, elapsed_seconds);
	measure_soil_moisture(soil_moisture, temperature, sprinkler_on, elapsed_seconds);
	measure_temperature(temperature, outer_temperature, heater_on, lamp_on, elapsed_seconds);

	printf("humidity: %5.2f\n", *humidity);
	printf("soil_moisture: %5.2f\n", *soil_moisture);
	printf("temperature: %5.2f\n", *temperature);
	printf("measuring ended\n");
}

void measure_humidity(float *humidity, const float *soil_moisture, const float *temperature, const bool *sprinkler_on, const float elapsed_seconds)
{
	if (*sprinkler_on && *humidity <= MAX_HUMIDITY && *temperature >= 30)
	{
		*humidity += 0.5 * elapsed_seconds;
	}
	else if (*sprinkler_on && *humidity <= MAX_HUMIDITY)
	{
		*humidity += 0.4 * elapsed_seconds;
	}
	else if (!*sprinkler_on && *soil_moisture > 50 && *humidity >= MIN_HUMIDITY)
	{
		*humidity -= 0.05 * elapsed_seconds;
	}
	else if (!*sprinkler_on && *soil_moisture <= 50 && *humidity >= MIN_HUMIDITY)
	{
		*humidity -= 0.1 * elapsed_seconds;
	}
}

void measure_soil_moisture(float *soil_moisture, const float *temperature, const bool *sprinkler_on, const float elapsed_seconds)
{
	if (*sprinkler_on && *soil_moisture <= MAX_SOIL_MOISTURE) 
	{
		*soil_moisture += 1 * elapsed_seconds;
	}
	else if (!*sprinkler_on && *temperature >= 30 && *soil_moisture >= MIN_SOIL_MOISTURE)
	{
		*soil_moisture -= 0.15 * elapsed_seconds;
	}
	else if (!*sprinkler_on && *soil_moisture >= MIN_SOIL_MOISTURE)
	{
		*soil_moisture -= 0.1 * elapsed_seconds;
	}
}

void measure_temperature(float *temperature, const float *outer_temperature, const bool *heater_on, const bool *lamp_on, const float elapsed_seconds)
{
	if (*heater_on && *temperature <= MAX_TEMPERATURE && *lamp_on)
	{
		*temperature += 0.6 * elapsed_seconds;
	}
	else if (*heater_on && *temperature <= MAX_TEMPERATURE)
	{
		*temperature += 0.5 * elapsed_seconds;
	}
	else if (!*heater_on && *temperature > *outer_temperature)
	{
		*temperature -= 0.05 * elapsed_seconds;
	}
}