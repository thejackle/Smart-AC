// Define for bench testing
// #define TESTBED

// Program version
#define SW_Version "Smart Air  V3.0"

// Global variables
    #define LOCAL_UPDATE 1
    #define NET_UPDATE 2
    #define NO_UPDATE 0

// Keypad setup
    // #define FOURxFOUR
    #define ONExFOUR

// Heartbeat LED setup


// LCD setup
    // #define LCD_DISPLAY
    #define OLED_DISPLAY

    #ifdef LCD_DISPLAY
        #include <LiquidCrystal_I2C.h>
        #define LCD_UPDATE_DELAY 1
    #elif defined(OLED_DISPLAY)
        #include <Adafruit_SSD1306.h>
    #endif

// Backlight
    // Timeout setting
    #ifdef TESTBED
        #define BACKLIGHT_TIME 99999999
        // #define BACKLIGHT_TIME 1000
    #else
        #define BACKLIGHT_TIME 50*1000
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

    #define COOLER_ON 2
    #define COOLER_AUTO 1
    #define DEVICE_OFF 0

// Net update
    #define NET_UPDATE_DELAY 3000

// IR Remote
    #define IR_REMOTE

// Power controls
    #define FAN_LOW 1
    #define FAN_MEDIUM 2
    #define FAN_HIGH 3

// Segment display
    // #define SEGMENT_DISPLAY
    #define SEGMENT_DELAY 1000

// Menu Setup
    #define START_MENU 1000
    #define MENU_ITEMS 2
    #define MENU_HIDDEN_ITEMS 1
    #define MENU_TOTAL_ITEMS MENU_HIDDEN_ITEMS + MENU_ITEMS

