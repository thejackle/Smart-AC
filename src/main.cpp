#include <Arduino.h>
#include <TeensyThreads.h>
#include <Chrono.h>

// Custom header files
#include <Pins.h>
#include <Settings.h>

// Keypad setup
#ifdef KEYPAD_1X4
	#define ROW_NUM 1
	#define COL_NUM 4
	char keypadHex[ROW_NUM][COL_NUM]{
	{'1','2','3','4'}
	};
	byte rowPins[ROW_NUM] = {0};
	byte colPins[COL_NUM] = {2,1,4,3};

	Keypad keyInput = Keypad(makeKeymap(keypadHex), rowPins, colPins, ROW_NUM, COL_NUM);
#endif

// IR Remote setup
#ifdef IR_REMOTE
	void ProcessCommand(uint16_t cmd);
	uint16_t previousCommand;
	unsigned long lastDebounceTime = 0;
	unsigned long debounceDelay = 200;
#endif

// Heartbeat LED setup
	void HeartbeatLed(int _TimeDelay = 500); 

// Display setup
#ifdef OLED_DISPLAY
	#define SCREEN_WIDTH 128
	#define SCREEN_HEIGHT 64
	#define SCREEN_ADDRESS 0x3C
	#define SCREEN_ADDRESS_TWO 0x3D
	#define OLED_RESET -1

	Adafruit_SSD1306 tempDisplay(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
	Adafruit_SSD1306 controlDisplay(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

	void OLEDPrint();
#endif

// Temperature reading
	void UpdateCurrentTemp();
	Chrono tempDelay;
	#if defined(TESTBED)
	#elif defined(DS18B20)
		OneWire tempWire(TEMP_SENSOR_PIN);
		DallasTemperature Temp1Sensor(&tempWire);
	#elif defined(SENS_DHT11)
		DHT tempSensor(TEMP_SENSOR_PIN, DHT11);
	#elif defined(MCP9808)
		Adafruit_MCP9808 tempSensorAdafruit = Adafruit_MCP9808();
	#endif

// Temp controller
	void TempController();
	void AutoCooler();
	// Delay after cooler has turned off to prevent the cooler from rapidly turning on and off
	Chrono coolerOffTimer;
	bool delayReset = false;
	int previousCoolerSetting;

// Power controls
	void PowerController(int _FanSet, int _CoolSet);

// Char copy
	void CharCpy(char* _charOneInput, char* _charTwoInput[], int offset);
	void CharCpy(char* _charOneInput, char _charTwoInput[], int offset);
	void CharCpy(char* _charOneInput, const char _charTwoInput[], int offset);

/********************************************************/

// Menu Setup
	int menuIndex = 1;

	#define SETTING_LIST 4
	int settingsValues[SETTING_LIST] = {-2, 0, 0, 0};
	char coolerStrings[3][5] = {"Off", "Auto", "On"};
	#define OFFSET_SETTING 0
	#define FAN_SETTING 1
	#define COOL_SETTING 2
	#define SET_POINT 3

	char selectIcon[4][5] = {
	">   ",
	" >  ",
	"  > ",
	"   >"
	};

	void IncrementMenu();
	void DecrementMenu();
	
// Global variables
	float globalTemperature;
	float setTemperature = 22.00;
	#define UPPER_SET_LIMIT 35.00
	#define LOWER_SET_LIMIT 15.00

/*************************************************************************************************************************/
// Program start
void setup(){

	// Serial init
	Serial.begin(115200);
	Serial.println(SW_Version);
	Serial.println("Starting...");

	// IR Remote setup
	#ifdef IR_REMOTE
		IrReceiver.begin(IR_RECEIVER_PIN, true);
	#endif
	
	// Heartbeat LED setup
	pinMode(LED_BUILTIN, OUTPUT);

	// Display setup
	#ifdef OLED_DISPLAY
		tempDisplay.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
		controlDisplay.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS_TWO);
	#endif

	// Temperature reading
	#if defined(TESTBED)
		globalTemperature = -55.00;
	#elif defined(DS18B20)
		Temp1Sensor.begin();
	#elif defined(SENS_DHT11)
		tempSensor.begin();
	#elif defined(MCP9808)
		tempSensorAdafruit.begin();
		tempSensorAdafruit.setResolution(1);
	#endif

	#if defined(SENS_DHT11) || defined(DS18B20)
		pinMode(TEMPSENSOR_PIN, INPUT);
	#endif

	// Power controls
	pinMode(FAN_LOW_PIN, OUTPUT);
	pinMode(FAN_MEDIUM_PIN, OUTPUT);
	pinMode(FAN_HIGH_PIN, OUTPUT);
	pinMode(COOLER_PIN, OUTPUT);
	
	// Start threads
	threads.addThread(HeartbeatLed,500);

	Serial.println("Startup complete");

}

/*************************************************************************************************************************/
void loop()
{
#ifdef KEYPAD_1X4
	// Get inputs from the keypad
	char keyInputChar = keyInput.getKey();
	if (keyInputChar)
	{
		Serial.println(keyInputChar);
		switch (keyInputChar)
		{
			case '4':
				IncrementMenu();
				break;

			case '3':
				DecrementMenu();
				break;

			case '2':
				settingsValues[menuIndex]++;
				break;

			case '1':
				settingsValues[menuIndex]--;
				break;

			default:
				break;
		}
	}
#endif

#ifdef IR_REMOTE
	// Get the IR remote inputs
	if (IrReceiver.decode())
	{
		if (IrReceiver.decodedIRData.command != previousCommand || millis() - lastDebounceTime > debounceDelay)
		{
			ProcessCommand(IrReceiver.decodedIRData.command);
		}
		
		previousCommand = IrReceiver.decodedIRData.command;
		lastDebounceTime = millis();
		
		IrReceiver.resume();
	}
#endif
	
	// Limit the settings
	if (settingsValues[FAN_SETTING] > FAN_HIGH)
	{
		settingsValues[FAN_SETTING] = FAN_HIGH;
	}
	else if (settingsValues[FAN_SETTING] < DEVICE_OFF)
	{
		settingsValues[FAN_SETTING] = DEVICE_OFF;
	}
	
	if (settingsValues[COOL_SETTING] > COOLER_ON)
	{
		settingsValues[COOL_SETTING] = COOLER_ON;
	}
	else if (settingsValues[COOL_SETTING] < DEVICE_OFF)
	{
		settingsValues[COOL_SETTING] = DEVICE_OFF;
	}

	// Adjust the set point
	if (settingsValues[SET_POINT] > 0)
	{
		for (int i = settingsValues[SET_POINT]; i > 0; i--)
		{
			setTemperature += 0.5;
		}
		settingsValues[SET_POINT] = 0;
	}
	else if (settingsValues[SET_POINT] < 0)
	{
		for (int i = settingsValues[SET_POINT]; i < 0; i++)
		{
			setTemperature -= 0.5;
		}
		settingsValues[SET_POINT] = 0;
	}
	
	

	// Auto temperature control loop
	if (settingsValues[COOL_SETTING] == COOLER_AUTO)
	{
		AutoCooler();
	}
	// Turn the cooler on
	else if (settingsValues[COOL_SETTING] == COOLER_ON)
	{
		PowerController(settingsValues[FAN_SETTING], COOLER_ON);
	}
	// Turn the cooler off
	else if (settingsValues[COOL_SETTING] == DEVICE_OFF)
	{
		PowerController(settingsValues[FAN_SETTING], DEVICE_OFF);
	}
	previousCoolerSetting = settingsValues[COOL_SETTING];

	// Update the screens
#ifdef OLED_DISPLAY
	OLEDPrint();
#endif

	// Update the temperature
	UpdateCurrentTemp();

}

/*************************************************************************************************************************/

// IR Remote
#ifdef IR_REMOTE
	void ProcessCommand(uint16_t cmd){

		switch (cmd)
		{
			case BTN_SELECT:
				Serial.println(("Select button received"));
			break;
		
			case BTN_UP:
				IncrementMenu();
			break;
			
			case BTN_DOWN:
				DecrementMenu();
			break;
			
			case BTN_LEFT:
				settingsValues[menuIndex]--;
			break;
			
			case BTN_RIGHT:
				settingsValues[menuIndex]++;
			break;

			// Increase the set point
			case BTN_ZOOM_UP:
				setTemperature += 0.5;
				if (setTemperature > UPPER_SET_LIMIT)
				{
					setTemperature = UPPER_SET_LIMIT;
				}
				
			break;

			// Decrease the set point
			case BTN_ZOOM_DOWN:
				setTemperature -= 0.5;
				if (setTemperature < LOWER_SET_LIMIT)
				{
					setTemperature = LOWER_SET_LIMIT;
				}
			break;

			// Turn off everything
			case BTN_START_STOP:
				Serial.println(("Start/Stop button received"));
				Serial.printf("Resetting values\n");
				menuIndex = 0;
				settingsValues[FAN_SETTING] = DEVICE_OFF;
				settingsValues[COOL_SETTING] = DEVICE_OFF;
				settingsValues[OFFSET_SETTING] = -2;
			break;
			
			// Set to high
			case BTN_2_SEC:
				Serial.printf("Setting to high\n");
				settingsValues[FAN_SETTING] = FAN_HIGH;
				settingsValues[COOL_SETTING] = COOLER_ON;
			break;
			
			// Set to auto
			case BTN_SHUTTER:
				Serial.printf("Setting to auto\n");
				settingsValues[COOL_SETTING] = COOLER_AUTO;
			break;

			default:
				Serial.println((IrReceiver.decodedIRData.command));
			break;
		}
	}
#endif

// Heartbeat LED
void HeartbeatLed(int _timeDelay)
{
	while (1)
	{
		digitalWrite(LED_BUILTIN, HIGH);
		delay(_timeDelay);
		digitalWrite(LED_BUILTIN, LOW);
		delay(_timeDelay);        
	}    
}

// Display
#ifdef OLED_DISPLAY

	void OLEDPrint()
	{
		// Temperature display
		tempDisplay.clearDisplay();
		tempDisplay.setTextSize(2);
		tempDisplay.setTextColor(SSD1306_WHITE);
		tempDisplay.setCursor(0,0);

		tempDisplay.printf("  Temp C\n");
		tempDisplay.setTextSize(3);
		tempDisplay.printf("C:%.2f\n", globalTemperature);
		tempDisplay.printf("S:%.2f\n", setTemperature);

		tempDisplay.display();
		
		// Menu display
		controlDisplay.clearDisplay();
		controlDisplay.setTextSize(2);
		controlDisplay.setTextColor(SSD1306_WHITE);
		controlDisplay.setCursor(0,0);

		if (menuIndex == OFFSET_SETTING)
		{
			controlDisplay.printf("%cOffset:%i\n", selectIcon[menuIndex][OFFSET_SETTING], settingsValues[OFFSET_SETTING]);
		}
		else
		{
			controlDisplay.printf(" Settings\n");
		}

		controlDisplay.printf("%cFan: FAN%i\n", selectIcon[menuIndex][FAN_SETTING], settingsValues[FAN_SETTING]);
		controlDisplay.printf("%cCool:%s\n", selectIcon[menuIndex][COOL_SETTING], coolerStrings[settingsValues[COOL_SETTING]]);
		controlDisplay.printf("%cTemp -/+", selectIcon[menuIndex][SET_POINT]);

		// Invert the title for settings
		for (int y = 0; y <= 15; y++)
		{
			for (int x = 1; x <= SCREEN_WIDTH; x++)
			{
				controlDisplay.writePixel(x, y, SSD1306_INVERSE);
			}
		}
		controlDisplay.display();
	}

#endif

// Temperature reading
void UpdateCurrentTemp()
{
	if (tempDelay.hasPassed(TEMP_DELAY))
	{
		#if defined(TESTBED)
			// globalTemperature = map(analogRead(TEST_TEMPIN),0,1023,0,90);
			globalTemperature = 10.00;
		#elif defined(DS18B20)
			if (Temp1Sensor.getDeviceCount() > 0)
			{
				Temp1Sensor.requestTemperatures();
				delay(100);
				globalTemperature = Temp1Sensor.getTempCByIndex(0);
			}
		#elif defined(SENS_DHT11)
			globalTemperature = tempSensor.readTemperature();
		#elif defined(CONST_TEMP)
			globalTemperature = 25.00;
		#elif defined(MCP9808)
			globalTemperature = tempSensorAdafruit.readTempC() + settingsValues[OFFSET_SETTING];
		#endif
		tempDelay.restart();
	}
	else
	{
		delay(500);
	}
	
}

void AutoCooler()
{
	// If fan is off, turn fan on
	if(settingsValues[FAN_SETTING] < FAN_LOW)
	{
		settingsValues[FAN_SETTING] = FAN_LOW;
	}

	if (globalTemperature > setTemperature && (coolerOffTimer.hasPassed(COOLER_DELAY_TIME) || previousCoolerSetting != COOLER_AUTO))
	{
		// Turn on
		PowerController(settingsValues[FAN_SETTING], COOLER_AUTO);
		delayReset = false;
		coolerOffTimer.stop();
	}
	else if (globalTemperature < setTemperature && !delayReset)
	{
		// Turn off
		PowerController(settingsValues[FAN_SETTING], DEVICE_OFF);
		coolerOffTimer.restart();
		delayReset = true;
	}
}

// Power controls
void PowerController(int _fanSet, int _coolSet)
{
	// Controls fan and cooler

	// Ensure the fan is of if the cooler is turned on
	if (_coolSet > DEVICE_OFF)
	{
		if (_fanSet < FAN_LOW)
		{
			_fanSet = FAN_LOW;
			settingsValues[FAN_SETTING] = FAN_LOW;
		}
		digitalWrite(COOLER_PIN, HIGH);
	}
	else
	{
		digitalWrite(COOLER_PIN, LOW);
	}
	
	// Set the fan, if the fan is turned off shut off the cooler
	switch (_fanSet)
	{
	case DEVICE_OFF:
		digitalWrite(FAN_LOW_PIN, LOW);
		digitalWrite(FAN_MEDIUM_PIN, LOW);
		digitalWrite(FAN_HIGH_PIN, LOW);
		digitalWrite(COOLER_PIN, LOW);
		break;

	case FAN_LOW:
		digitalWrite(FAN_LOW_PIN, HIGH);
		digitalWrite(FAN_MEDIUM_PIN, LOW);
		digitalWrite(FAN_HIGH_PIN, LOW);
		break;
	
	case FAN_MEDIUM:
		digitalWrite(FAN_LOW_PIN, LOW);
		digitalWrite(FAN_MEDIUM_PIN, HIGH);
		digitalWrite(FAN_HIGH_PIN, LOW);
		break;
		
	case FAN_HIGH:
		digitalWrite(FAN_LOW_PIN, LOW);
		digitalWrite(FAN_MEDIUM_PIN, LOW);
		digitalWrite(FAN_HIGH_PIN, HIGH);
		break;

	default:
		break;
	}
	
}

// Char copy
// Insert a string into a char array
void CharCpy(char* _charOneInput, char* _charTwoInput[], int offset)
{
	for (uint i = 0; i < strlen(*_charTwoInput); i++)
	{
		_charOneInput[i + offset] = *_charTwoInput[i];
	}
}

void CharCpy(char* _charOneInput, char _charTwoInput[], int offset)
{
	for (uint i = 0; i < strlen(_charTwoInput); i++)
	{
		_charOneInput[i + offset] = _charTwoInput[i];
	}
}

void CharCpy(char* _charOneInput, const char _charTwoInput[], int offset)
{
	for (uint i = 0; i < strlen(_charTwoInput); i++)
	{
		_charOneInput[i + offset] = _charTwoInput[i];
	}
}

// Menu functions
void IncrementMenu(){
	
	menuIndex--;
	if (menuIndex < 0)
	{
		menuIndex = SETTING_LIST - 1;
	}
}

void DecrementMenu(){

	menuIndex++;
	if (menuIndex >= SETTING_LIST)
	{
		menuIndex = 1;
	}
}
