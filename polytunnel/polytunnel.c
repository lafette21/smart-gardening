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

// Mosquitto
#define MQTT_HOSTNAME "localhost"
#define MQTT_PORT 1884
#define MQTT_CLIENTID "erts_sub"
#define MQTT_TOPIC "hello"

#define TIME_BETWEEN_MEASUREMENTS 2 // sec
#define ID_CHARACTER_LENGTH 36 // without terminating null

#define TEST_MODE false

#if TEST_MODE
	#include "./measure_simulation_test.h"
#else
	#include "./measure_simulation.h"
#endif

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
void take_measurements(float *humidity, float *temperature, float outer_temperature, float *soil_moisture, const bool sprinkler_on, 
							const bool heater_on, const bool lamp_on, const float elapsed_seconds);
void measure_humidity(float *humidity, const float soil_moisture, const float temperature, const bool sprinkler_on, const float elapsed_seconds);
void measure_soil_moisture(float *soil_moisture, const float temperature, const bool sprinkler_on, const float elapsed_seconds);
void measure_temperature(float *temperature, const float outer_temperature, const bool heater_on, const bool lamp_on, const float elapsed_seconds);
void adjust_humidity_sprinkler_is_on(float *humidity, const float temperature, const float elapsed_seconds);
void adjust_humidity_sprinkler_is_off(float *humidity, const float soil_moisture, const float elapsed_seconds);
void adjust_soil_moisture_sprinkler_on(float *soil_moisture, const float elapsed_seconds);
void adjust_soil_moisture_sprinkler_off(float *soil_moisture, const float temperature, const float elapsed_seconds);
void adjust_temperature_heater_on(float *temperature, const bool lamp_on, const float elapsed_seconds);
void adjust_temperature_heater_off(float *temperature, const bool lamp_on, const float outer_temperature, const float elapsed_seconds);

bool in_interval(const float base, const float value, const float threshold, const float elapsed_seconds, const bool max);

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
				take_measurements(humidity, temperature, *outer_temperature, soil_moisture, *sprinkler_on, *heater_on, *lamp_on, elapsed_seconds);
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
	char topic[13 + ID_CHARACTER_LENGTH + strlen(topic_snd_part) + 1];
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
	const int topic_length = full_url ? strlen(topic_part) : 13 + ID_CHARACTER_LENGTH + strlen(topic_part);
	char topic[topic_length];

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

void take_measurements(float *humidity, float *temperature, const float outer_temperature, float *soil_moisture, const bool sprinkler_on, 
							const bool heater_on, const bool lamp_on, const float elapsed_seconds) 
{
	printf("-------------------------------------------------------------------------------------------------\n");
	printf("measuring\n");

	measure_humidity(humidity, *soil_moisture, *temperature, sprinkler_on, elapsed_seconds);
	measure_soil_moisture(soil_moisture, *temperature, sprinkler_on, elapsed_seconds);
	measure_temperature(temperature, outer_temperature, heater_on, lamp_on, elapsed_seconds);

	printf("humidity: %5.2f\n", *humidity);
	printf("soil_moisture: %5.2f\n", *soil_moisture);
	printf("temperature: %5.2f\n", *temperature);
	printf("measuring ended\n");
}

void measure_humidity(float *humidity, const float soil_moisture, const float temperature, const bool sprinkler_on, const float elapsed_seconds)
{
	if (sprinkler_on) 
	{
		adjust_humidity_sprinkler_is_on(humidity, temperature, elapsed_seconds);
	}
	else
	{
		adjust_humidity_sprinkler_is_off(humidity, soil_moisture, elapsed_seconds);
	}
}

void adjust_humidity_sprinkler_is_on(float *humidity, const float temperature, const float elapsed_seconds)
{
	if (temperature >= 30 && in_interval(*humidity, SPRINKLER_ON_AND_TEMP_GTE_30, MAX_HUMIDITY, elapsed_seconds, true))
	{
		*humidity += SPRINKLER_ON_AND_TEMP_GTE_30 * elapsed_seconds;	
	}
	else if (in_interval(*humidity, SPRINKLER_ON_AND_TEMP_LT_30, MAX_HUMIDITY, elapsed_seconds, true))
	{
		*humidity += SPRINKLER_ON_AND_TEMP_LT_30 * elapsed_seconds;
	}
	else 
	{
		*humidity = MAX_HUMIDITY;
	}
}

void adjust_humidity_sprinkler_is_off(float *humidity, const float soil_moisture, const float elapsed_seconds)
{
	if (soil_moisture >= 50 && in_interval(*humidity, SPRINKLER_OFF_AND_SOIL_GTE_50, MAX_HUMIDITY, elapsed_seconds, true))
	{
		*humidity += SPRINKLER_OFF_AND_SOIL_GTE_50 * elapsed_seconds;	
	}
	else if (in_interval(*humidity, SPRINKLER_OFF_AND_SOIL_LT_50, MIN_HUMIDITY, elapsed_seconds, false))
	{
		*humidity -= SPRINKLER_OFF_AND_SOIL_LT_50 * elapsed_seconds;	
	}
	else
	{
		*humidity = MIN_HUMIDITY;
	}
}

void measure_soil_moisture(float *soil_moisture, const float temperature, const bool sprinkler_on, const float elapsed_seconds)
{
	if (sprinkler_on)
	{
		adjust_soil_moisture_sprinkler_on(soil_moisture, elapsed_seconds);
	}
	else
	{
		adjust_soil_moisture_sprinkler_off(soil_moisture, temperature, elapsed_seconds);
	}
}

void adjust_soil_moisture_sprinkler_on(float *soil_moisture, const float elapsed_seconds)
{
	if (in_interval(*soil_moisture, SPRINKLER_ON, MAX_SOIL_MOISTURE, elapsed_seconds, true)) 
	{
		*soil_moisture += SPRINKLER_ON * elapsed_seconds;
	}
	else
	{
		*soil_moisture = MAX_SOIL_MOISTURE;
	}
}

void adjust_soil_moisture_sprinkler_off(float *soil_moisture, const float temperature, const float elapsed_seconds)
{
	if (temperature >= 30 && in_interval(*soil_moisture, SPRINKLER_OFF_AND_TEMP_GTE_30, MIN_SOIL_MOISTURE, elapsed_seconds, false))
	{
		*soil_moisture -= SPRINKLER_OFF_AND_TEMP_GTE_30 * elapsed_seconds;
	}
	else if (in_interval(*soil_moisture, SPRINKLER_OFF_AND_TEMP_LT_30, MIN_SOIL_MOISTURE, elapsed_seconds, false))
	{
		*soil_moisture -= SPRINKLER_OFF_AND_TEMP_LT_30 * elapsed_seconds;
	}
	else
	{
		*soil_moisture = MIN_SOIL_MOISTURE;
	}
}

void measure_temperature(float *temperature, const float outer_temperature, const bool heater_on, const bool lamp_on, const float elapsed_seconds)
{
	if (heater_on)
	{
		adjust_temperature_heater_on(temperature, lamp_on, elapsed_seconds);
	}
	else
	{
		adjust_temperature_heater_off(temperature, lamp_on, outer_temperature, elapsed_seconds);
	}
}

void adjust_temperature_heater_on(float *temperature, const bool lamp_on, const float elapsed_seconds)
{
	if (lamp_on && in_interval(*temperature, HEATER_ON_AND_LAMP_ON, MAX_TEMPERATURE, elapsed_seconds, true))
	{
		*temperature += HEATER_ON_AND_LAMP_ON * elapsed_seconds;
	}
	else if (in_interval(*temperature, HEATER_ON_AND_LAMP_OFF, MAX_TEMPERATURE, elapsed_seconds, true))
	{
		*temperature += HEATER_ON_AND_LAMP_OFF * elapsed_seconds;
	}
	else {
		*temperature = MAX_TEMPERATURE;
	}
}

void adjust_temperature_heater_off(float *temperature, const bool lamp_on, const float outer_temperature, const float elapsed_seconds)
{
	if (lamp_on && *temperature > outer_temperature && in_interval(*temperature, HEATER_OFF_AND_LAMP_ON, outer_temperature, elapsed_seconds, false))
	{
		*temperature -= HEATER_OFF_AND_LAMP_ON * elapsed_seconds;
	}
	else if (*temperature > outer_temperature && in_interval(*temperature, HEATER_OFF_AND_LAMP_OFF, outer_temperature, elapsed_seconds, false))
	{
		*temperature -= HEATER_OFF_AND_LAMP_OFF * elapsed_seconds;
	}
	else if (*temperature < outer_temperature && in_interval(*temperature, HEATER_OFF_AND_TEMP_LT_OUT_TEMP, MAX_TEMPERATURE, elapsed_seconds, true))
	{
		*temperature += HEATER_OFF_AND_TEMP_LT_OUT_TEMP * elapsed_seconds;
	}
	else if (*temperature < outer_temperature || *temperature > outer_temperature)
	{
		*temperature = outer_temperature;
	}
}

bool in_interval(const float base, const float value, const float threshold, const float elapsed_seconds, const bool max) 
{
	float scaled_value = value * elapsed_seconds;
	float modified_value = base + (max ? scaled_value : -scaled_value);

	return max ? (modified_value <= threshold) : (modified_value >= threshold);
}