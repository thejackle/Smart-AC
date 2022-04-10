#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <TeensyThreads.h>
#include <Keypad.h>
#include <Chrono.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <DFRobot_LedDisplayModule.h>
#include <SD.h>

// Custome header files
#include <Menuclass.h>
#include <SerialCommands.h>
#include <Pins.h>
#include <Settings.h>

#define TESTBED

// Test comment

// Create a stuct to store the settings
    struct Settings
    {
        double setPoint = 23.0;
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

    Settings localSetting;
    Settings netSetting;
    Settings currentSetting;

// Global variables
    double Global_TempCurrent = 20.00;
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
    OneWire tempWire(TEMPSENSOR_PIN);
    DallasTemperature Temp1Sensor(&tempWire);
    Chrono tempDelay;

// Temp controller
    void TempController();
    // Delay before checking if the temperature is under set point
    Chrono tempCheckTimer;
    // Delay after cooler has turned off to prevent the cooler from rapidly turning on and off
    Chrono coolerOffTimer;
    bool Delay_reset = false;


// Net update
    // void NetUpdate_TempStat();
    void NetGetUpdate(double* _setTemp, int* _fanSetting, int* _coolerSetting);
    void NetSendUpdate();
    Chrono netTempDelay;

// Power controls
    void PowerController(int _FanSet, int _CoolSet);

// Segment display
    DFRobot_LedDisplayModule SegmentDisplay = DFRobot_LedDisplayModule(Wire, 0x48);
    void UpdateSegment();
    Chrono segmentDelay;

// Temp
    char charTempSet[5];
    char charTempCurrent[5];
    char charFanSetting[1];
    char charCoolSetting[1];
    void InteruptDelay();

// Char copy
    void CharCpy(char* _charOneInput, char* _charTwoInput[], int offset);
    void CharCpy(char* _charOneInput, char _charTwoInput[], int offset);
    void CharCpy(char* _charOneInput, const char _charTwoInput[], int offset);

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

// Program version
#define SW_Version "Smart Air   V2.X"
/*
Bug's

modular approch - any IO can be changed/modifed without affecting main program
Define
    Global variables
    Keypad
    LCD
    popup - not currently used
    Heartbeat LED
    Backlight
    Temp get
    Temp controller
    Net update
    Power contols
    Segment display
    Menu setup

Setup
    Serial init
    Segment display start
    Temp sensor start
    Set pin modes
    Add threads
    populate main menu
    Start LCD
    Reset timers

    Show start menu screen
        Wait START_MENU seconds
    
    Show menu 1 

Loop
    get key
        update local
        local update true

    Flip flop for net/local update
        if local update and last update is not local
            update global
            update net
            send update
            reset local update
        else if net update and last update is not net
            update global
            update local
            reset net update
        else
            reset last update
    
    switch for main menu index
        update main menu based on index
    
    update current temp - send to net
    update segment display
    update lcd temp
    print lcd

*/
/*************************************************************************************************************************/
// Program start
void setup(){

    // Serial init
    Serial.begin(115200);
    Serial1.begin(115200);
    delay(500);

    SegmentDisplay.begin4();
    SegmentDisplay.setDisplayArea4(1,2,3,4);
    // SegmentDisplay.print4("C","O","O","L");

    Temp1Sensor.begin();

    // Set pins 
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(TEMPSENSOR_PIN, INPUT);
    pinMode(NETCONNECT, INPUT);
    pinMode(33, INPUT);

    pinMode(FAN_LOW_PIN, OUTPUT);
    pinMode(FAN_MEDIUM_PIN, OUTPUT);
    pinMode(FAN_HIGH_PIN, OUTPUT);
    pinMode(COOLER_PIN, OUTPUT);
    
    // Init threads
    threads.addThread(HeartbeatLed,500);
    threads.addThread(TempController);
    threads.addThread(BacklightSet);
    threads.addThread(UpdateCurrentTemp);

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

    // Show the start menu
    LcdPrint();
    delay(START_MENU);
    menuIndex = 1;
}

/*************************************************************************************************************************/
void loop()
{
    // Get local inputs from the keypad
    char keyinput = keyInput.getKey();
    if (keyinput)
    {
        // Serial.println(keyinput);
        switch (keyinput)
        {
            case '2':
                menuIndex++;
                break;

            case '5':
                menuIndex--;
                break;

            case '6':
                localSetting.setPoint += 0.5;
                break;

            case '4':
                localSetting.setPoint -= 0.5;
                break;

            case '7':
                // Fan setting down
                if (localSetting.fanSetting <= 0){localSetting.fanSetting = 0;}
                else{localSetting.fanSetting--;}
                break;

            case '8':
                // Fan setting up
                if (localSetting.fanSetting >= 3){localSetting.fanSetting = 3;}
                else{localSetting.fanSetting++;}
                break;

            case 'C':
                // Turn off fan and cooler
                localSetting.fanSetting = 0;
                localSetting.coolerSetting = 0;
                break;

            case '*':
                // Cooler setting down
                if (localSetting.coolerSetting <= 0){localSetting.coolerSetting = 0;}
                else{localSetting.coolerSetting--;}
                break;
            
            case '0':
                // Cooler setting up
                if (localSetting.coolerSetting >= 3){localSetting.coolerSetting = 3;}
                else{localSetting.coolerSetting++;}
                break;

            case 'D':
                // Turn off cooler
                localSetting.coolerSetting = 0;
                break;

            default:
                break;
        }

        localSetting.update = true;
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
    
    /*
    Flip flop for net/local update
        if local update and last update is not local
            send update
            set update logic local
        else if net update and last update is not net
            get update
            set update logic net
        else
            reset update
    */


    // if (localSetting.update && LastUpdate == !LOCAL_UPDATE)
    // {
    //     // Serial.println("local");
    //     localSetting.update = false;
    //     LastUpdate = LOCAL_UPDATE;

    //     currentSetting.updateSettings(localSetting);
    //     netSetting.updateSettings(localSetting);
    //     NetSendUpdate();
    // }
    // else if (netSetting.update && LastUpdate == !NET_UPDATE)
    // {
    //     // Serial.println("Net");
    //     netSetting.update = false;
    //     LastUpdate = NET_UPDATE;

    //     currentSetting.updateSettings(netSetting);
    //     localSetting.updateSettings(netSetting);
    // }
    // else
    // {
    //     LastUpdate = NO_UPDATE;
    // }
    
    if (digitalRead(NETCONNECT) == 1)
    {
        
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

    // delay(100);

    // Update current temp - net
    NetUpdate_TempStat();

    // Update current temp - 7 segment display
    // SegmentDisplay.print4(Global_TempCurrent);
    // UpdateSegment();

    // Print LCD screen (no flashing)
    LcdPrint();
}

/*************************************************************************************************************************/

void HeartbeatLed(int _TimeDelay)
{
    while (1)
    {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(_TimeDelay);
        digitalWrite(LED_BUILTIN, LOW);
        delay(_TimeDelay);        
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
    delay(1000);
    while (1)
    {
        if (tempDelay.hasPassed(TEMP_DELAY) == 1 && Temp1Sensor.getDeviceCount() > 0)
        {
            Temp1Sensor.requestTemperatures();
            delay(100);
            Global_TempCurrent = Temp1Sensor.getTempCByIndex(0);
            // Global_TempCurrent = 27;
            tempDelay.restart();
        }
        else
        {
            #ifdef TESTBED
                Global_TempCurrent = map(analogRead(TEST_TEMPIN),0,1023,-35,90);
            #endif
        }
    }
}

void TempController()
{
    int _Cset = 0;
    while (1)
    {
        // Auto temperature control loop
        if (tempCheckTimer.hasPassed(CONTROLLER_TIME) == 1 && currentSetting.coolerSetting == COOLER_AUTO)
        {
            if (Global_TempCurrent > currentSetting.setPoint && coolerOffTimer.hasPassed(COOLER_DELAY_TIME) == 1)
            {
                // Turn on
                PowerController(currentSetting.fanSetting,COOLER_AUTO);
                _Cset = COOLER_AUTO;
                Delay_reset = false;
                coolerOffTimer.stop();
            }
            else if (Global_TempCurrent < currentSetting.setPoint && Delay_reset == false)
            {
                // Turn off
                PowerController(currentSetting.fanSetting,DEVICE_OFF);
                coolerOffTimer.restart();
                _Cset = DEVICE_OFF;
                Delay_reset = true;
            }
            else
            {
                // Serial.println("else");
                PowerController(currentSetting.fanSetting,_Cset);
            }
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
    } 
}

void PowerController(int _fanSet, int _coolSet)
{
    // Controlls fan and cooler
    if (_coolSet > 0)
    {
        if (_fanSet < 1)
        {
            _fanSet = 1;
            currentSetting.fanSetting = 1;
            localSetting.fanSetting = currentSetting.fanSetting;
        }
        digitalWrite(COOLER_PIN, 1);
    }
    else
    {
        digitalWrite(COOLER_PIN, 0);
    }
    
    switch (_fanSet)
    {
    case DEVICE_OFF:
        digitalWrite(FAN_LOW_PIN, 0);
        digitalWrite(FAN_MEDIUM_PIN, 0);
        digitalWrite(FAN_HIGH_PIN, 0);
        digitalWrite(COOLER_PIN, 0);
        break;

    case FAN_LOW:
        digitalWrite(FAN_LOW_PIN, 1);
        digitalWrite(FAN_MEDIUM_PIN, 0);
        digitalWrite(FAN_HIGH_PIN, 0);
        break;
    
    case FAN_MEDIUM:
        digitalWrite(FAN_LOW_PIN, 0);
        digitalWrite(FAN_MEDIUM_PIN, 1);
        digitalWrite(FAN_HIGH_PIN, 0);
        break;
        
    case FAN_HIGH:
        digitalWrite(FAN_LOW_PIN, 0);
        digitalWrite(FAN_MEDIUM_PIN, 0);
        digitalWrite(FAN_HIGH_PIN, 1);
        break;

    default:
        break;
    }
    
    
}

/*********************************** Coms *******************************************/

/*
    Get
        Master - request variable
        Secondary - send varable
        Master - acknowledge
    
    Set
        Master - send update command
        Secondary - acknowledge
        Master - send variable
        Secondary - acknowledge

*/

void NetGetUpdate(double* _setTemp, int* _fanSetting, int* _coolerSetting)
{
    Serial1.write(SER_GETTEMP);
    int t = Serial1.read();
}

void NetSendUpdate()
{

}

void NetUpdate_TempStat()
{

    if (digitalRead(NETCONNECT) && netTempDelay.hasPassed(NET_UPDATE_DELAY) == 1)
    {
        char _NetSend[16] = "set tempc ";

        strcat(_NetSend, charTempCurrent);
        strcat(_NetSend, " ");
        Serial1.println(_NetSend);
        Serial.println(_NetSend);
        netTempDelay.restart();
    }
}

void NetSendUpdate()
{
    char _NetSend[16];
    // Serial.println("Send update");

    if (digitalRead(NETCONNECT) == 1)
    {
        // Serial.println("temps");
        netTempDelay.restart();

        // Update set temp
        dtostrf(currentSetting.setPoint,4,2,charTempSet);
        strcpy(_NetSend, "set temps ");
        strcat(_NetSend, charTempSet);
        Serial1.println(_NetSend);

        delay(100);

        // Update cooler setting
        strcpy(_NetSend, "set auto ");
        sprintf(charCoolSetting, "%d", currentSetting.coolerSetting);
        strcat(_NetSend, charCoolSetting);
        // _NetSend = "set auto ";
        Serial1.println(_NetSend);

        delay(100);

        // Update fan setting
        strcpy(_NetSend, "set fan ");
        sprintf(charFanSetting, "%d", currentSetting.fanSetting);
        strcat(_NetSend, charFanSetting);
        Serial1.println(_NetSend);
    }
}

void serialEvent1()
{
    // delay(2000);
    // Serial.println("Start");
    if (digitalRead(NETCONNECT))
    {
        char _rawCharIn[32];
        int _testInt = Serial1.available();
        for (int i = 0; i < _testInt; i++)
        {
            _rawCharIn[i] = Serial1.read();
        }

        char Action[5];
        strncpy(Action, _rawCharIn, 4);
        Action[sizeof(Action) - 1] = '\0';

        // Serial.print("raw=");
        // Serial.println(_rawCharIn);
        // Serial.print("action=");
        // Serial.println(Action);
        
        if (strcmp(Action, "temp") == 0)
        {
            char _charInt[6];
            _charInt[0] = _rawCharIn[4];
            _charInt[1] = _rawCharIn[5];
            _charInt[2] = _rawCharIn[6];
            _charInt[3] = _rawCharIn[7];
            _charInt[4] = _rawCharIn[8];

            netSetting.setPoint = atof(_charInt);
            Serial.print("temp=");
            Serial.println(netSetting.setPoint);
        }
        else if (strcmp(Action, "auto") == 0)
        {
            netSetting.coolerSetting = _rawCharIn[4] - '0';
            if (netSetting.coolerSetting == 1 && currentSetting.fanSetting == 0)
            {
                currentSetting.fanSetting = 1;
                netSetting.fanSetting = 1;
                NetSendUpdate();
            }
            
        }
        else if (strcmp(Action, "fans") == 0)
        {
            netSetting.fanSetting = _rawCharIn[4] - '0';
            if (netSetting.fanSetting == 0)
            {
                // Serial.println("test");
                currentSetting.coolerSetting = 0;
                netSetting.coolerSetting = 0;
                NetSendUpdate();
            }
            
        }
        else{}
        // Serial.println(netSetting.fanSetting);
        netSetting.update = true;
    }
    else
    {
        while (Serial1.available())
        {
            Serial1.read();
        }
        
    }
}


