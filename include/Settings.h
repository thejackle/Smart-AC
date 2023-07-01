// Define for bench testing
// #define TESTBED

// Program version
#define SW_Version "Smart Air  V3.0"

// Keypad setup
    #define KEYPAD_1X4

    #ifdef KEYPAD_1X4
        #include <Keypad.h>
    #endif

// Heartbeat LED setup

// Display setup
    // #define LCD_DISPLAY
    #define OLED_DISPLAY

    #ifdef OLED_DISPLAY
        #include <Adafruit_SSD1306.h>
    #endif

// Temp get
    #define TEMP_DELAY 1000
    #define MCP9808
    // #define SENS_DHT11
    // #define DS18B20
    // #define CONST_TEMP

    #ifdef MCP9808
        #include <Adafruit_MCP9808.h>
    #elif defined(SENS_DHT11)
        #include <DHT.h>
    #elif defined(DS18B20)
        #include <DallasTemperature.h>
    #endif

// Temp controller
    #ifdef TESTBED
        #define COOLER_DELAY_TIME 5000 // Test time
    #else
        #define COOLER_DELAY_TIME 5*60000
    #endif
    // Delay before cooler can turn on after turning off
    #define CONTROLLER_TIME 1*1000


// IR Remote
    #define IR_REMOTE

    #ifdef IR_REMOTE
        #include <IRremote.h>
        #include <IR Commands.h>
    #endif

// Power controls
    #define FAN_LOW 1
    #define FAN_MEDIUM 2
    #define FAN_HIGH 3
    
    #define COOLER_ON 2
    #define COOLER_AUTO 1
    #define DEVICE_OFF 0

// Menu Setup

