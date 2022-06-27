#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <TeensyThreads.h>
#include <Keypad.h>
#include <Chrono.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <OneWire.h>
#include <DFRobot_LedDisplayModule.h>
#include <SD.h>
#include <SPI.h>

// Custome header files
#include <Menuclass.h>
#include <Pins.h>
#include <Settings.h>


elapsedMillis timerOne;

// Create a stuct to store the settings
	struct Settings
	{
		float setPoint = 23.0;
		int fanSetting = 0;
		int coolerSetting = 0;
		bool update = false;

		void updateSettings(Settings newSettings)
		{
			setPoint = newSettings.setPoint;
			fanSetting = newSettings.fanSetting;
			coolerSetting = newSettings.coolerSetting;
		}
	};

	// Settings localSetting;
	// Settings netSetting;
	Settings currentSetting;

// Global variables
	float Global_TempCurrent = 20.00;
	int LastUpdate = 0;

// Keypad setup
	#define COL_NUM 4
	#define ROW_NUM 4
	char keys[COL_NUM][ROW_NUM]{
		{'1','2','3','A'},
		{'4','5','6','B'},
		{'7','8','9','C'},
		{'*','0','#','D'}
	};
	byte pinRow[ROW_NUM] = {11,10,9,8};
	byte pinCol[COL_NUM] = {7,6,5,4};
	Keypad keyInput = Keypad(makeKeymap(keys), pinRow, pinCol, COL_NUM, ROW_NUM);

// Heartbeat LED setup
	void HeartbeatLed(int _TimeDelay = 500); 

// LCD setup
#ifdef TESTBED
	//DFrobot
	LiquidCrystal_I2C lcd(0x20,16,2);
#else
	//raspi
	LiquidCrystal_I2C lcd(0x27,16,2);
#endif

	void LcdPrint();
	//Metro lcdUpdate = Metro(UPDATE_DELAY);
	Chrono lcdUpdate;
	unsigned long lastUpdateLCD;
	char tempFrameOne[16]{"               "};
	char tempFrameTwo[16]{"               "};


// Backlight
	void BacklightSet();
	Chrono backlightTimer;

// Temp get
	void UpdateCurrentTemp();
	Chrono tempDelay;
	#if defined(TESTBED)
	#elif defined(DS18B20)
		OneWire tempWire(TEMPSENSOR_PIN);
		DallasTemperature Temp1Sensor(&tempWire);
	#elif defined(SENS_DHT11)
		DHT tempSensor(TEMPSENSOR_PIN, DHT11);
	#endif

// Temp controller
	void TempController();
	// Delay before checking if the temperature is under set point
	Chrono tempCheckTimer;
	// Delay after cooler has turned off to prevent the cooler from rapidly turning on and off
	Chrono coolerOffTimer;
	bool Delay_reset = false;


// Net update
	// void NetUpdate_TempStat();
	void NetGetUpdate(Settings sendSetting, float* currentTemp);
	Chrono netTempDelay;

// Power controls
	void PowerController(int _FanSet, int _CoolSet);
	int tempCool = 0;
	int tempFan = 0;

// Segment display
	DFRobot_LedDisplayModule SegmentDisplay = DFRobot_LedDisplayModule(Wire, 0x48);
	void UpdateSegment();
	Chrono segmentDelay;

// Temp char arrays
	char charTempSet[5];
	char charTempCurrent[5];
	char charFanSetting[1];
	char charCoolSetting[1];
	void InteruptDelay();

// Char copy
	void CharCpy(char* _charOneInput, char* _charTwoInput[], int offset);
	void CharCpy(char* _charOneInput, char _charTwoInput[], int offset);
	void CharCpy(char* _charOneInput, const char _charTwoInput[], int offset);

// SD Setup
	#define CHIP_SELECT 254
	//File log;
	void SDLog(char data[]);
	bool SDAvailable = false;
	int temp_fanSet;
	int temp_coolSet;
	int temp_coolerSetting;

/********************************************************/

// Menu Setup
	Menuclass menu[MENU_ITEMS + MENU_HIDDEN_ITEMS]{0,0,0};
	int menuIndex = 0;
/*
	Menu items
	0. Start menu - Hidden
	1. Main screen
	2. Current temp
*/

/* Changes for testing
	disable segment diplay (loop)
	comment test input temp
*/

/*************************************************************************************************************************/

/*
Bug's

modular approch - any IO can be changed/modifed without affecting main program
Define
	Global variables
	Keypad
	Heartbeat LED
	LCD
	Backlight
	Temp get
	Temp controller
	Net update
	Power contols
	Segment display
	Menu setup
	Temp char arrays
	Char copy
	SD Setup
	Menu Setup

Setup
	Serial init
	Segment display start
	Temp sensor start
	Set pin modes
	Add threads
	populate main menu
	Start LCD
	Logging
	Menu

Loop
	get key - update current
	loop menu
	update net settings
	update menu char arrays
	switch for main menu index
		update main menu based on index
	
	Log data
	update segment display
	update lcd temp
*/
/*************************************************************************************************************************/
// Program start
void setup(){

	// Serial init
	Serial.begin(115200);
	Serial1.begin(115200);
	Serial1.setTimeout(500);
	delay(500);

	SegmentDisplay.begin4();
	SegmentDisplay.setDisplayArea4(1,2,3,4);
	SegmentDisplay.print4("C","O","O","L");

	#if defined(TESTBED)
	#elif defined(DS18B20)
		Temp1Sensor.begin();
	#elif defined(SENS_DHT11)
		tempSensor.begin();
	#endif

	// Set pins 
	pinMode(LED_BUILTIN, OUTPUT);
	pinMode(TEMPSENSOR_PIN, INPUT);

	pinMode(FAN_LOW_PIN, OUTPUT);
	pinMode(FAN_MEDIUM_PIN, OUTPUT);
	pinMode(FAN_HIGH_PIN, OUTPUT);
	pinMode(COOLER_PIN, OUTPUT);
	
	// Init threads
	threads.addThread(HeartbeatLed,500);
	// threads.addThread(TempController);
	threads.addThread(BacklightSet);
	//threads.addThread(UpdateCurrentTemp);

	// Fill in the menu
	
	strcpy(menu[0].Lineone, SW_Version);
	strcpy(menu[0].Linetwo, "Conditioner     ");

	strcpy(menu[1].Lineone, "Set Temp        ");
	strcpy(menu[1].Linetwo, "Off     Off     ");
	
	strcpy(menu[2].Lineone, "Set Temp        ");
	strcpy(menu[2].Linetwo, "Current         ");
	
	// Start the LCD
	lcd.init();
	lcd.backlight();

	// Setup SD logging
	if (!SD.begin(BUILTIN_SDCARD))
	{
		Serial.println("Could not initialise SD card");
	}
	else
	{
		SDAvailable = true;
		Serial.println("SD card initialised");
		SDLog("****************************************");
		SDLog("Starting new Log");
	}
	
	// Show the start menu
	LcdPrint();
	delay(START_MENU);
	menuIndex = 1;
}

/*************************************************************************************************************************/
void loop()
{
	UpdateCurrentTemp();

	// Get local inputs from the keypad
	char keyinput = keyInput.getKey();
	if (keyinput)
	{
		Serial.println(keyinput);
		switch (keyinput)
		{
			case '2':
				menuIndex++;
				break;

			case '5':
				menuIndex--;
				break;

			case '6':
				currentSetting.setPoint += 0.5;
				break;

			case '4':
				currentSetting.setPoint -= 0.5;
				break;

			case '7':
				// Fan setting down
				if (currentSetting.fanSetting <= 0){currentSetting.fanSetting = 0;}
				else{currentSetting.fanSetting--;}
				break;

			case '8':
				// Fan setting up
				if (currentSetting.fanSetting >= 3){currentSetting.fanSetting = 3;}
				else{currentSetting.fanSetting++;}
				break;

			case 'C':
				// Turn off fan and cooler
				currentSetting.fanSetting = 0;
				currentSetting.coolerSetting = 0;
				break;

			case '*':
				// Cooler setting down
				if (currentSetting.coolerSetting <= 0){currentSetting.coolerSetting = 0;}
				else{currentSetting.coolerSetting--;}
				break;
			
			case '0':
				// Cooler setting up
				if (currentSetting.coolerSetting >= 3){currentSetting.coolerSetting = 3;}
				else{currentSetting.coolerSetting++;}
				break;

			case 'D':
				// Turn off cooler
				currentSetting.coolerSetting = 0;
				break;
			
			default:
				break;
		}
		backlightTimer.restart();
	}

	// If the index is greater than the items, reset it to the lowest menu item
	if (menuIndex >= MENU_TOTAL_ITEMS)
	{
		menuIndex = MENU_HIDDEN_ITEMS;
	}
	// If the index is less than or equal to the number of hidden menus, reset it to the highest value
	else if (menuIndex < MENU_HIDDEN_ITEMS)
	{
		menuIndex = MENU_TOTAL_ITEMS - 1;
	}
	
	if(netTempDelay.hasPassed(NET_UPDATE_DELAY)){
		NetGetUpdate(currentSetting, &Global_TempCurrent);
		netTempDelay.restart();
	}

	// Convert temp to char arrays
	dtostrf(Global_TempCurrent,4,2,charTempCurrent);
	dtostrf(currentSetting.setPoint,4,2,charTempSet);

	// Convert setting to char array
	sprintf(charFanSetting, "%d", currentSetting.fanSetting);
	sprintf(charCoolSetting, "%d", currentSetting.coolerSetting);

	// Update set temp - LCD
	CharCpy(&menu[1].Lineone[11], &charTempSet[0], 0);
	CharCpy(&menu[2].Lineone[11], &charTempSet[0], 0);

	switch (menuIndex)
	{        
		case 1:
			if (currentSetting.fanSetting == 0)
			{
				CharCpy(&menu[1].Linetwo[0], "Off  ", 0);
			}
			else
			{
				CharCpy(&menu[1].Linetwo[0], "Fan ", 0);
				menu[1].Linetwo[4] = charFanSetting[0];
			}

			if (currentSetting.coolerSetting == 0)
			{
				CharCpy(&menu[1].Linetwo[8], "Off  ", 0);
			}
			else if (currentSetting.coolerSetting == 1)            
			{
				CharCpy(&menu[1].Linetwo[8], "Auto ", 0);
			}
			else
			{
				CharCpy(&menu[1].Linetwo[8], "Cool ", 0);
			}
			
		break;
		
		case 2:
			// Update current temp - LCD
			CharCpy(&menu[2].Linetwo[11], &charTempCurrent[0], 0);
		break;

		default:
		break;
	}

	// Check the temperature
	TempController();

	// Update current temp - 7 segment display
	SegmentDisplay.print4(Global_TempCurrent);

	// Print LCD screen (no flashing)
	LcdPrint();
}

/*************************************************************************************************************************/

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

void LcdPrint()
{
	char _lineOneIn[16];
	char _lineTwoIn[16];
	strcpy(_lineOneIn, menu[menuIndex].Lineone);
	strcpy(_lineTwoIn, menu[menuIndex].Linetwo);

	if(lcdUpdate.hasPassed(LCD_UPDATE_DELAY) == 1){

		lcdUpdate.restart(); 

		for (unsigned int i = 0; i < sizeof(tempFrameOne) ; i++)
		{
			if (tempFrameOne[i] != _lineOneIn[i])
			{
				tempFrameOne[i] = _lineOneIn[i];
				lcd.setCursor(i,0);
				lcd.print(tempFrameOne[i]);
			}
		}

		for (unsigned int i = 0; i < sizeof(tempFrameTwo) ; i++)
		{
			if (tempFrameTwo[i] != _lineTwoIn[i])
			{
				tempFrameTwo[i] = _lineTwoIn[i];
				lcd.setCursor(i,1);
				lcd.print(tempFrameTwo[i]);
			}
		}
	}
}

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


// Update the seven segment display
void UpdateSegment()
{
	if(segmentDelay.hasPassed(SEGMENT_DELAY) == 1)
	{
		SegmentDisplay.print4(Global_TempCurrent);
		segmentDelay.restart();
	}
}

// Turn the backlight off when the timer has expired
void BacklightSet()
{
	while (1)
	{
		if (backlightTimer.hasPassed(BACKLIGHT_TIME))
		{
			lcd.setBacklight(0);
		}
		else
		{
			lcd.setBacklight(1);
		}
	}
}

// Read the current temperature
void UpdateCurrentTemp()
{
	// delay(1000);
	// while (1)
	// {
		if (tempDelay.hasPassed(TEMP_DELAY))
		{
			#if defined(TESTBED)
				Global_TempCurrent = map(analogRead(TEST_TEMPIN),0,1023,0,90);
			#elif defined(DS18B20)
				if (Temp1Sensor.getDeviceCount() > 0)
				{
					Temp1Sensor.requestTemperatures();
					delay(100);
					Global_TempCurrent = Temp1Sensor.getTempCByIndex(0);
				}
			#elif defined(SENS_DHT11)
				Global_TempCurrent = tempSensor.readTemperature();
			#elif defined(CONST_TEMP)
				Global_TempCurrent = 25.00;
			#endif
			tempDelay.restart();
		}
		else
		{
			delay(500);
		}
	// }
}

void TempController()
{
	// int _Cset = 0;
	// while (1)
	// {
		// if (temp_coolerSetting != currentSetting.coolerSetting)
		// {
			// temp_coolerSetting = currentSetting.coolerSetting;
			// char temp[] = "Cooler setting = A";
			// temp[17] = currentSetting.coolerSetting + '0';
			// Serial.println(temp);
			// SDLog(temp);
		// }
		



		// digitalWrite(LED_BUILTIN, HIGH);
		// delay(500);
		// digitalWrite(LED_BUILTIN, LOW);
		// delay(500);    
		// Auto temperature control loop
		if (currentSetting.coolerSetting == COOLER_AUTO)
		{
			if(currentSetting.fanSetting < 1)
			{
				currentSetting.fanSetting = 1;
			}

			if (Global_TempCurrent > currentSetting.setPoint && coolerOffTimer.hasPassed(COOLER_DELAY_TIME) == 1)
			{
				// Turn on
				PowerController(currentSetting.fanSetting,COOLER_AUTO);
				// _Cset = COOLER_AUTO;
				Delay_reset = false;
				coolerOffTimer.stop();
			}
			else if (Global_TempCurrent < currentSetting.setPoint && Delay_reset == false)
			{
				// If the fan is off turn the fan to low
				if (currentSetting.fanSetting < FAN_LOW){currentSetting.fanSetting = FAN_LOW;}
				
				// Turn off
				PowerController(currentSetting.fanSetting,DEVICE_OFF);
				coolerOffTimer.restart();
				// _Cset = DEVICE_OFF;
				Delay_reset = true;
			}
			// else
			// {
			// 	// Serial.println("else");
			// 	PowerController(currentSetting.fanSetting,_Cset);
			// }
			tempCheckTimer.restart();
		}
		// On cooler control loop
		else if (currentSetting.coolerSetting == COOLER_ON)
		{
			PowerController(currentSetting.fanSetting, 1);
		}
		// Turn off devices
		else if (currentSetting.coolerSetting == DEVICE_OFF)
		{
			PowerController(currentSetting.fanSetting, DEVICE_OFF);
		}
	// } 
}

void PowerController(int _fanSet, int _coolSet)
{

	if (_fanSet != tempFan || _coolSet != tempCool)
	{
		// tempFan = _fanSet;
		// tempCool = _coolSet;
		char temp[20] = "Fan = A ,Cooler = B";
		temp[6] = '0' + _fanSet;
		temp[18] = '0' + _coolSet;
		Serial.println(temp);
		SDLog(temp);
	}
	
	// Controlls fan and cooler
	if (_coolSet != tempCool)
	{
		tempCool = _coolSet;
		SDLog("cooler change");
		if (_coolSet > DEVICE_OFF)
		{
			if (_fanSet < FAN_LOW)
			{
				_fanSet = FAN_LOW;
				currentSetting.fanSetting = FAN_LOW;
			}
			digitalWrite(COOLER_PIN, HIGH);
			SDLog("Cooler on");
		}
		else
		{
			digitalWrite(COOLER_PIN, LOW);
			SDLog("Cooler off");
		}
	}
	
	
	if (_fanSet != tempFan)
	{
		tempFan = _fanSet;
		SDLog("fan change");
		switch (_fanSet)
		{
		case DEVICE_OFF:
			digitalWrite(FAN_LOW_PIN, LOW);
			digitalWrite(FAN_MEDIUM_PIN, LOW);
			digitalWrite(FAN_HIGH_PIN, LOW);
			digitalWrite(COOLER_PIN, LOW);
			SDLog("Fan off");
			break;

		case FAN_LOW:
			digitalWrite(FAN_LOW_PIN, HIGH);
			digitalWrite(FAN_MEDIUM_PIN, LOW);
			digitalWrite(FAN_HIGH_PIN, LOW);
			SDLog("Fan low");
			break;
		
		case FAN_MEDIUM:
			digitalWrite(FAN_LOW_PIN, LOW);
			digitalWrite(FAN_MEDIUM_PIN, HIGH);
			digitalWrite(FAN_HIGH_PIN, LOW);
			SDLog("Fan mid");
			break;
			
		case FAN_HIGH:
			digitalWrite(FAN_LOW_PIN, LOW);
			digitalWrite(FAN_MEDIUM_PIN, LOW);
			digitalWrite(FAN_HIGH_PIN, HIGH);
			SDLog("Fan high");
			break;

		default:
			break;
		}
	}
	
}

void SDLog(char data[])
{
	if (SDAvailable)
	{
		File log = SD.open("logFile.txt", FILE_WRITE);
		log.print("Time: ");
		log.print(millis());
		log.print(" - ");
		log.println(data);
		log.close();
	}
	
}

/*********************************** Coms *******************************************/

/*
	Update process
		Send update
			Create byte array to send to remote
			1.[0] Current temp
			2.[1] Current temp
			3.[2] Current temp
			4.[3] Current temp
			5.[4] Set temp
			6.[5] Set temp
			7.[6] Set temp
			8.[7] Set temp
			9.[8] Fan setting
			10.[9] Cooler setting
			Send byte array
			Read ACC

		Get update
			Serial event
			1.[0] Set temp
			2.[1] Set temp
			3.[2] Set temp
			4.[3] Set temp
			5.[4] Fan setting
			6.[6] Cooler setting

*/

void NetGetUpdate(Settings sendSetting, float* currentTemp)
{
	// Create update packet

	char outputData[10];
	memcpy(&outputData[0], currentTemp, 4);
	memcpy(&outputData[4], &sendSetting.setPoint, 4);
	outputData[8] = '0' + sendSetting.fanSetting;
	outputData[9] = '0' + sendSetting.coolerSetting;
	
	// Send packet to remote
	Serial1.write(outputData, sizeof(outputData));

	// if there is an error flush the buffer
	//Serial1.flush();
}

void serialEvent1(){

	// Get update
	Serial.println("Getting update");
	char inputData[6];
	if (Serial1.available() >= 6)
	{
		Serial1.readBytes(inputData, 6);
	}
	SDLog(inputData);
	memcpy(&currentSetting.setPoint, &inputData[0], 4);
	currentSetting.fanSetting = inputData[4] - '0';
	currentSetting.coolerSetting = inputData[5] - '0';

}