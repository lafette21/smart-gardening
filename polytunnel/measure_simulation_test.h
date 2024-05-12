
// Sensor parameters
#define DEFAULT_HUMIDITY 40
#define MAX_HUMIDITY 100
#define MIN_HUMIDITY 0

#define DEFAULT_SOIL_MOISTURE 30
#define MAX_SOIL_MOISTURE 100
#define MIN_SOIL_MOISTURE 0

#define DEFAULT_TEMPERATURE 30
#define MAX_TEMPERATURE 100
#define MIN_TEMPERATURE -100

#define DEFAULT_OUTER_TEMPERATURE 20 // will be overwritten after first outer temperature value is arrived through MQTT

// Measurement simulation parameters

// ------------Humidity------------
// increase
#define SPRINKLER_ON_AND_TEMP_GTE_30 3.2
#define SPRINKLER_ON_AND_TEMP_LT_30 3.1
#define SPRINKLER_OFF_AND_SOIL_GTE_50 3.08

// decrease
#define SPRINKLER_OFF_AND_SOIL_LT_50 3.1

// ------------Soil Moisture------------
// increase
#define SPRINKLER_ON 4

// decrease
#define SPRINKLER_OFF_AND_TEMP_GTE_30 3.2
#define SPRINKLER_OFF_AND_TEMP_LT_30 3.15

// ------------Temperature------------
// increase
#define HEATER_ON_AND_LAMP_ON 3.6
#define HEATER_ON_AND_LAMP_OFF 3.5
#define HEATER_OFF_AND_TEMP_LT_OUT_TEMP 3.1

// decrease
#define HEATER_OFF_AND_LAMP_ON 3.08
#define HEATER_OFF_AND_LAMP_OFF 3.05