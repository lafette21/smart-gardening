
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
#define SPRINKLER_ON_AND_TEMP_GTE_30 0.2
#define SPRINKLER_ON_AND_TEMP_LT_30 0.1
#define SPRINKLER_OFF_AND_SOIL_GTE_50 0.08

// decrease
#define SPRINKLER_OFF_AND_SOIL_LT_50 0.1

// ------------Soil Moisture------------
// increase
#define SPRINKLER_ON 1

// decrease
#define SPRINKLER_OFF_AND_TEMP_GTE_30 0.2
#define SPRINKLER_OFF_AND_TEMP_LT_30 0.15

// ------------Temperature------------
#define STRONG_LIGHT_LOWER_LIMIT 0.5
// increase
#define HEATER_ON_AND_LAMP_ON 0.6
#define HEATER_ON_AND_LAMP_OFF 0.5
#define HEATER_OFF_AND_TEMP_LT_OUT_TEMP_AND_STRONG_LIGHT 0.1
#define HEATER_OFF_AND_TEMP_LT_OUT_TEMP_AND_WEAK_LIGHT 0.7

// decrease
#define HEATER_OFF_AND_LAMP_ON_AND_STRONG_LIGHT 0.03
#define HEATER_OFF_AND_LAMP_ON_AND_WEAK_LIGHT 0.06
#define HEATER_OFF_AND_LAMP_OFF_AND_STRONG_LIGHT 0.05
#define HEATER_OFF_AND_LAMP_OFF_AND_WEAK_LIGHT 0.09