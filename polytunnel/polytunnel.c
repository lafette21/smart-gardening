#include <stdio.h>
#include <stdbool.h>
// #include "mosquitto.h"
#include <signal.h>
#include <unistd.h>
#include <sys/shm.h>
// #include <sys/types.h>
// #include <sys/ipc.h>
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

struct callback_params {
	float *humidity;
	float *soil_moisture;
	float *temperature;

	bool *sprinkler_on;
	bool *heater_on;
	bool *lamp_on;
};


// Mosquitto
// void message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message);
// void connect_callback(struct mosquitto *mosq, void *userdata, int result);
// void subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_ops);

// measurements
void send_measurements();
void take_measurements(float *humidity, float *temperature, float *soil_moisture, const bool *sprinler_on, 
							const bool *heater_on, const bool *lamp_on, const float elapsed_seconds);
void measure_humidity(float *humidity, const float *soil_moisture, const float *temperature, const bool *sprinkler_on, const float elapsed_seconds);
void measure_soil_moisture(float *soil_moisture, const float *temperature, const bool *sprinkler_on, const float elapsed_seconds);
void measure_temperature(float *temperature, const bool *heater_on, const bool *lamp_on, const float elapsed_seconds);

int main()
{
	// create shared memory ids
	unsigned int humidity_shared_mem_id = shmget(1000, sizeof(float), IPC_CREAT | S_IRUSR | S_IWUSR);
	unsigned int temperature_shared_mem_id = shmget(2000, sizeof(float), IPC_CREAT | S_IRUSR | S_IWUSR);
	unsigned int soil_moisture_shared_mem_id = shmget(3000, sizeof(float), IPC_CREAT | S_IRUSR | S_IWUSR);
	unsigned int sprinkler_on_shared_mem_id = shmget(4000, sizeof(bool), IPC_CREAT | S_IRUSR | S_IWUSR);
	unsigned int heater_on_shared_mem_id = shmget(5000, sizeof(bool), IPC_CREAT | S_IRUSR | S_IWUSR);
	unsigned int lamp_on_shared_mem_id = shmget(6000, sizeof(bool), IPC_CREAT | S_IRUSR | S_IWUSR);

	pid_t child = fork();

	if (child > 0) // Parent
	{
		// signal
		struct sigaction sigact;
		sigact.sa_handler = send_measurements;
		sigemptyset(&sigact.sa_mask);
		sigact.sa_flags = 0;

		sigaction(SIGALRM, &sigact, NULL);

		// measurement variables
		float outer_temperature = 20;

		
		// shared memory
		float *humidity = shmat(humidity_shared_mem_id, NULL, 0);
		float *temperature = shmat(temperature_shared_mem_id, NULL, 0);
		float *soil_moisture = shmat(soil_moisture_shared_mem_id, NULL, 0);
		printf("eljut\n");
		*humidity = DEFAULT_HUMIDITY;
		*temperature = DEFAULT_TEMPERATURE;
		*soil_moisture = DEFAULT_SOIL_MOISTURE;

		bool *sprinkler_on = shmat(sprinkler_on_shared_mem_id, NULL, 0);
		bool *heater_on = shmat(heater_on_shared_mem_id, NULL, 0);
		bool *lamp_on = shmat(lamp_on_shared_mem_id, NULL, 0);

		*sprinkler_on = false;
		*heater_on = false;
		*lamp_on = false;

		// timer
		const clock_t start = clock();
		clock_t last_update = start;
		clock_t elapsed_time = 0; // elapsed time from last update

		while (true)
		{
			elapsed_time = clock() - last_update;
			float elapsed_seconds = ((double)elapsed_time / CLOCKS_PER_SEC);

			if (elapsed_seconds >= TIME_BETWEEN_MEASUREMENTS)
			{
				take_measurements(humidity, temperature, soil_moisture, sprinkler_on, heater_on, lamp_on, elapsed_seconds);
				last_update = clock();
			}

		}

		exit(0);
	}
	else // Child
	{
		// float *humidity = shmat(humidity_shared_mem_id, NULL, SHM_RDONLY);
		// float *soil_moisture = shmat(soil_moisture_shared_mem_id, NULL, SHM_RDONLY);
		// float *temperature = shmat(temperature_shared_mem_id, NULL, SHM_RDONLY);
		// bool *sprinkler_on = shmat(sprinkler_on_shared_mem_id, NULL, 0);
		// bool *heater_on = shmat(heater_on_shared_mem_id, NULL, 0);
		// bool *lamp_on = shmat(lamp_on_shared_mem_id, NULL, 0);
		
		// struct mosquitto *mosq = NULL;
		// struct callback_params params = {humidity, soil_moisture, temperature, sprinkler_on, heater_on, lamp_on};
		// mosquitto_lib_init();

		// mosq = mosquitto_new(NULL, true, &params);
		// if (!mosq)
		// {
		// 	fprintf(stderr, "Out of memory.\n");
		// 	exit(1);
		// }
		// mosquitto_connect_callback_set(mosq, connect_callback);
    	// mosquitto_message_callback_set(mosq, message_callback);
    	// mosquitto_subscribe_callback_set(mosq, subscribe_callback);

		// if (mosquitto_connect(mosq, MQTT_HOSTNAME, MQTT_PORT, 60)) // keepalive=60
    	// {
    	//     printf("Unable to connect.\n");
    	//     exit(1);
    	// }

		printf("child loop\n");
		// mosquitto_loop_forever(mosq, -1, 1);
		printf("child endloop\n");
		// mosquitto_destroy(mosq);
		// mosquitto_lib_cleanup();
		exit(0);
	}

	exit(0);
}

void send_measurements()
{
	printf("Sending measurements.\n");
}

// void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
// {
// 	struct callback_params *params = (struct callback_params *)obj;
// 	printf("params->humidity: %6.3f\n", params->humidity);

//     if (message->payloadlen)
//     {
// 		printf("--------------------------\n");
//         printf("%s %s\n", message->topic, message->payload);
// 		printf("%5.3f", *humidity);
// 		printf("Sending alrm signal to parent.\n");
// 		kill(getppid(), SIGALRM);
// 		printf("--------------------------\n");
//     }
//     else
//     {
//         printf("%s (null)\n", message->topic);
//     }
//     fflush(stdout);
// }

// void connect_callback(struct mosquitto *mosq, void *userdata, int result)
// {
//     int i;
//     if (!result)
//     {
//         /* Subscribe to broker information topics on successful connect. */
//         mosquitto_subscribe(mosq, NULL, "hello", 2);
//     }
//     else
//     {
//         printf("Connect failed\n");
//     }
// }

// void subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
// {
//     int i;

//     printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
//     for (i = 1; i < qos_count; i++)
//     {
//         printf(", %d", granted_qos[i]);
//     }
//     printf("\n");
// }

void take_measurements(float *humidity, float *temperature, float *soil_moisture, const bool *sprinkler_on, 
							const bool *heater_on, const bool *lamp_on, const float elapsed_seconds) 
{
	printf("-------------------------------------------------------------------------------------------------\n");
	printf("%6.3f\n", elapsed_seconds);
	printf("measuring\n");

	measure_humidity(humidity, soil_moisture, temperature, sprinkler_on, elapsed_seconds);
	measure_soil_moisture(soil_moisture, temperature, sprinkler_on, elapsed_seconds);
	measure_temperature(temperature, heater_on, lamp_on, elapsed_seconds);

	printf("humidity: %5.2f\n", *humidity);
	printf("soil_moisture: %5.2f\n", *soil_moisture);
	printf("temperature: %5.2f\n", *temperature);
}

void measure_humidity(float *humidity, const float *soil_moisture, const float *temperature, const bool *sprinkler_on, const float elapsed_seconds)
{
	if (*sprinkler_on && *humidity <= (MAX_HUMIDITY - 0.5) && *temperature >= 30)
	{
		*humidity += 0.5 * elapsed_seconds;
	}
	else if (*sprinkler_on && *humidity <= (MAX_HUMIDITY - 0.4))
	{
		*humidity += 0.4 * elapsed_seconds;
	}
	else if (*sprinkler_on && *humidity < MAX_HUMIDITY)
	{
		*humidity = MAX_HUMIDITY;
	}
	else if (!*sprinkler_on && *soil_moisture <= 50 && *humidity >= (MIN_HUMIDITY + 0.1))
	{
		*humidity -= 0.1 * elapsed_seconds;
	}
	else if (!*sprinkler_on && *humidity > MIN_HUMIDITY)
	{
		*humidity = MIN_HUMIDITY;
	}
}

void measure_soil_moisture(float *soil_moisture, const float *temperature, const bool *sprinkler_on, const float elapsed_seconds)
{
	if (*sprinkler_on && *soil_moisture <= (MAX_SOIL_MOISTURE - 1)) 
	{
		*soil_moisture += 1 * elapsed_seconds;
	}
	else if (!*sprinkler_on && *temperature >= 30 && *soil_moisture >= (MIN_SOIL_MOISTURE + 0.3))
	{
		*soil_moisture -= 0.3 * elapsed_seconds;
	}
	else if (!*sprinkler_on && *soil_moisture >= (MIN_SOIL_MOISTURE + 0.2))
	{
		*soil_moisture -= 0.2 * elapsed_seconds;
	}
	else if (!*sprinkler_on && *soil_moisture > MIN_SOIL_MOISTURE) 
	{
		*soil_moisture = MIN_SOIL_MOISTURE;
	}
}

void measure_temperature(float *temperature, const bool *heater_on, const bool *lamp_on, const float elapsed_seconds)
{
	float outer_temperature = 25;

	if (*heater_on && *temperature <= (MAX_TEMPERATURE - 0.6) && *lamp_on)
	{
		*temperature += 0.6 * elapsed_seconds;
	}
	else if (*heater_on && *temperature <= (MAX_TEMPERATURE - 0.5))
	{
		*temperature += 0.5 * elapsed_seconds;
	}
	else if (*heater_on && *temperature < MAX_TEMPERATURE)
	{
		*temperature = MAX_TEMPERATURE;
	}
	else if (!*heater_on && *temperature > (outer_temperature + 0.05))
	{
		*temperature -= 0.05 * elapsed_seconds;
	}
}