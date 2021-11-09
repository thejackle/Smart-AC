#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <TeensyThreads.h>
#include <Keypad.h>
#include <Menuclass.h>
#include <Metro.h>
#include <Chrono.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <DFRobot_LedDisplayModule.h>


// Global variables
    double Global_TempCurrent = 20.00;

    #define LOCAL_UPDATE 1
    #define NET_UPDATE 2
    #define NO_UPDATE 0
    int LastUpdate = 0;
    
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
    //raspi
    LiquidCrystal_I2C lcd(0x27,16,2);
    //DFrobot
    // LiquidCrystal_I2C lcd(0x20,16,2);
    void lcdPrint();

    #define UPDATE_DELAY 1
    Metro lcdUpdate = Metro(UPDATE_DELAY);
    unsigned long lastUpdateLCD;
    char tempFrameOne[16]{"               "};
    char tempFrameTwo[16]{"               "};


// Backlight
    void BacklightSet();
    #define BACKLIGHT_TIME 50000
    // #define BACKLIGHT_TIME 99999999 // Test time
    Chrono backlightTimer;

// Temp get
    void UpdateTemp();
    #define TEMPSENSOR_PIN 34
    OneWire tempWire(TEMPSENSOR_PIN);
    DallasTemperature Temp1Sensor(&tempWire);
    Metro tempDelay = Metro(1000);

// Temp control
    void TempController();
    #define CONTROLLER_TIME 1*1000
    #define COOLER_DELAY_TIME 5*60000
    // #define COOLER_DELAY_TIME 100 // Test time
    Metro tempControllerMetro = Metro(CONTROLLER_TIME);
    Metro coolerOffTimeMetro = Metro(100); //Short time for startup, removes delay
    bool Delay_reset = false;

// Net update
    void NetUpdate_TempStat();
    void NetSendUpdate();
    Metro netTempDelay = Metro(3000);
    #define NETCONNECT 2

// Power controls
    void PowerController(int _FanSet, int _CoolSet);
    #define FAN_LOW 25
    #define FAN_MEDIUM 26
    #define FAN_HIGH 27
    #define COOLER_PIN 28

// Segment display
    DFRobot_LedDisplayModule SegmentDisplay = DFRobot_LedDisplayModule(Wire, 0x48);
    void UpdateSegment();
    Metro segmentDelay = Metro(1000);

// Temp
    char charTempSet[5];
    char charTempCurrent[5];
    char charFanSetting[1];
    char charCoolSetting[1];
    void InteruptDelay();
    

/********************************************************/

// Menu Setup
    #define START_MENU 1000
    #define MENU_ITEMS 2
    #define MENU_HIDDEN_ITEMS 2
    #define MENU_TOTAL_ITEMS MENU_HIDDEN_ITEMS + MENU_ITEMS
    Menuclass menu[MENU_ITEMS + MENU_HIDDEN_ITEMS + 1]{0,0,0,0,3};
    int menuIndex = 1;
/*
    Menu items
    0. Blank for offset
    1. Start menu - index 0 - Hidden
    2. pop-up - index 0 - Hidden
    3. Main screen - index 0
    4. Current temp

*/

/* Changes for testing
    lcd screen
    disable segment diplay (loop)
    change BACKLIGHT_TIME
    change COOLER_DELAY_TIME
    comment test input temp
*/

/*************************************************************************************************************************/

// Program version
#define SW_Version "Smart Air   V1.0"
/*
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
    pinMode(NETCONNECT, 0);
    pinMode(33, INPUT);

    pinMode(FAN_LOW, 1);
    pinMode(FAN_MEDIUM, 1);
    pinMode(FAN_HIGH, 1);
    pinMode(COOLER_PIN, 1);
    
    // Init threads
    threads.addThread(HeartbeatLed,500);
    threads.addThread(TempController);
    threads.addThread(BacklightSet);
    threads.addThread(UpdateTemp);

    // Fill in the menu
    strcpy(menu[0].Lineone, "Offset menu     ");
    strcpy(menu[0].Linetwo, "Offset menu     ");

    strcpy(menu[1].Lineone, SW_Version);
    strcpy(menu[1].Linetwo, "Conditioner     ");

    strcpy(menu[2].Lineone, "Hidden line one ");
    strcpy(menu[2].Linetwo, "Hidden line one ");

    strcpy(menu[3].Lineone, "Set Temp        ");
    strcpy(menu[3].Linetwo, "Off     Off     ");
    
    strcpy(menu[4].Lineone, "Set Temp        ");
    strcpy(menu[4].Linetwo, "Current         ");
    
    // Start the LCD
    lcd.init();
    lcd.backlight();

    tempDelay.reset();
    backlightTimer.restart();
    netTempDelay.reset();
    coolerOffTimeMetro.reset();
    segmentDelay.reset();

    // attachInterrupt(digitalPinToInterrupt(NETCONNECT),InteruptDelay, HIGH);

    // Show the start menu
    lcdPrint();
    delay(START_MENU);
    menuIndex = 3;
}

/*************************************************************************************************************************/
void loop()
{    
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
                if (localSetting.fanSetting <= 0)
                {
                    localSetting.fanSetting = 0;
                }
                else
                {
                    localSetting.fanSetting--;
                }
                break;

            case '8':
                // Fan setting up
                if (localSetting.fanSetting >= 3)
                {
                    localSetting.fanSetting = 3;
                }
                else
                {
                    localSetting.fanSetting++;
                }
                break;

            case 'C':
                // Turn off fan and cooler
                localSetting.fanSetting = 0;
                localSetting.coolerSetting = 0;
                break;

            case '*':
                // Cooler setting down
                if (localSetting.coolerSetting <= 0)
                {
                    localSetting.coolerSetting = 0;
                }
                else
                {
                    localSetting.coolerSetting--;
                }
                break;
            
            case '0':
                // Cooler setting up
                if (localSetting.coolerSetting >= 3)
                {
                    localSetting.coolerSetting = 3;
                }
                else
                {
                    localSetting.coolerSetting++;
                }
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
    if (menuIndex > MENU_TOTAL_ITEMS)
    {
        menuIndex = MENU_HIDDEN_ITEMS + 1;
    }
    // If the index is less than or equal to the number of hidden menus, reset it to the highest value
    else if (menuIndex <= MENU_HIDDEN_ITEMS)
    {
        menuIndex = MENU_TOTAL_ITEMS;
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


    if (localSetting.update && LastUpdate == !LOCAL_UPDATE)
    {
        // Serial.println("local");
        localSetting.update = false;
        LastUpdate = LOCAL_UPDATE;

        currentSetting.updateSettings(localSetting);
        netSetting.updateSettings(localSetting);
        NetSendUpdate();
    }
    else if (netSetting.update && LastUpdate == !NET_UPDATE)
    {
        // Serial.println("Net");
        netSetting.update = false;
        LastUpdate = NET_UPDATE;

        currentSetting.updateSettings(netSetting);
        localSetting.updateSettings(netSetting);
    }
    else
    {
        LastUpdate = NO_UPDATE;
    }
    
    //Convert temp to char arrays
    dtostrf(Global_TempCurrent,4,2,charTempCurrent);
    dtostrf(currentSetting.setPoint,4,2,charTempSet);
    sprintf(charFanSetting, "%d", currentSetting.fanSetting);
    sprintf(charCoolSetting, "%d", currentSetting.coolerSetting);

    //Update set temp - LCD
    menu[3].Lineone[11] = charTempSet[0];
    menu[3].Lineone[12] = charTempSet[1];
    menu[3].Lineone[13] = charTempSet[2];
    menu[3].Lineone[14] = charTempSet[3];
    menu[3].Lineone[15] = charTempSet[4];
    
    menu[4].Lineone[11] = charTempSet[0];
    menu[4].Lineone[12] = charTempSet[1];
    menu[4].Lineone[13] = charTempSet[2];
    menu[4].Lineone[14] = charTempSet[3];
    menu[4].Lineone[15] = charTempSet[4];


    switch (menuIndex)
    {        
        case 3:


            if (currentSetting.fanSetting == 0)
            {
                menu[3].Linetwo[0] = 'O';
                menu[3].Linetwo[1] = 'f';
                menu[3].Linetwo[2] = 'f';
                menu[3].Linetwo[3] = ' ';
            }
            else
            {
                menu[3].Linetwo[0] = 'F';
                menu[3].Linetwo[1] = 'a';
                menu[3].Linetwo[2] = 'n';
                menu[3].Linetwo[3] = charFanSetting[0];
            }

            if (currentSetting.coolerSetting == 0)
            {
                menu[3].Linetwo[8] = 'O';
                menu[3].Linetwo[9] = 'f';
                menu[3].Linetwo[10] = 'f';
                menu[3].Linetwo[11] = ' ';
            }
            else if (currentSetting.coolerSetting == 1)            
            {
                menu[3].Linetwo[8] = 'A';
                menu[3].Linetwo[9] = 'u';
                menu[3].Linetwo[10] = 't';
                menu[3].Linetwo[11] = 'o';
            }
            else
            {
                menu[3].Linetwo[8] = 'C';
                menu[3].Linetwo[9] = 'o';
                menu[3].Linetwo[10] = 'o';
                menu[3].Linetwo[11] = 'l';
            }
            
        break;
        
        case 4:
            // Update current temp - LCD
            menu[4].Linetwo[11] = charTempCurrent[0];
            menu[4].Linetwo[12] = charTempCurrent[1];
            menu[4].Linetwo[13] = charTempCurrent[2];
            menu[4].Linetwo[14] = charTempCurrent[3];
            menu[4].Linetwo[15] = charTempCurrent[4];
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
    lcdPrint();
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

void lcdPrint()
{
    char _lineOneIn[16];
    char _lineTwoIn[16];
    strcpy(_lineOneIn, menu[menuIndex].Lineone);
    strcpy(_lineTwoIn, menu[menuIndex].Linetwo);

    if(lcdUpdate.check() == 1){

        lcdUpdate.reset(); 

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

void UpdateTemp()
{
    delay(1000);
    while (1)
    {
        if (tempDelay.check() == 1 && Temp1Sensor.getDeviceCount() > 0)
        {
            Temp1Sensor.requestTemperatures();
            delay(100);
            Global_TempCurrent = Temp1Sensor.getTempCByIndex(0);
            // Global_TempCurrent = 27;
            tempDelay.reset();
        }
        else
        {
            // Global_TempCurrent = map(analogRead(23),0,1023,-35,90);
        }
    }
}

void TempController()
{
    int _Cset = 0;
    while (1)
    {
        if (tempControllerMetro.check() == 1 && currentSetting.coolerSetting == 1)
        {
            if (Global_TempCurrent > currentSetting.setPoint && coolerOffTimeMetro.check() == 1)
            {
                // Turn on
                PowerController(currentSetting.fanSetting,1);
                _Cset = 1;
                Delay_reset = false;
                coolerOffTimeMetro.interval(COOLER_DELAY_TIME);
            }
            else if (Global_TempCurrent < currentSetting.setPoint && Delay_reset == false)
            {
                // Turn off
                PowerController(currentSetting.fanSetting,0);
                coolerOffTimeMetro.reset();
                _Cset = 0;
                Delay_reset = true;
            }
            else
            {
                // Serial.println("else");
                PowerController(currentSetting.fanSetting,_Cset);
            }
            tempControllerMetro.reset();
        }
        else if (currentSetting.coolerSetting == 2)
        {
            PowerController(currentSetting.fanSetting, 1);
        }
        else if (currentSetting.coolerSetting == 0)
        {
            PowerController(currentSetting.fanSetting, 0);
        }
    } 
}

void NetUpdate_TempStat()
{

    if (digitalRead(NETCONNECT) && netTempDelay.check() == 1)
    {
        char _NetSend[16] = "set tempc ";

        strcat(_NetSend, charTempCurrent);
        strcat(_NetSend, " ");
        Serial1.println(_NetSend);
        Serial.println(_NetSend);
        netTempDelay.reset();
    }
}

void NetSendUpdate()
{
    char _NetSend[16];
    // Serial.println("Send update");

    if (digitalRead(NETCONNECT) == 1)
    {
        // Serial.println("temps");
        netTempDelay.reset();

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

// Is this needed?
/*
void InteruptDelay()
{
    delay(100);
    while (Serial1.available())
    {
        Serial1.read();
    }
    
}
*/
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

void PowerController(int _fanSet, int _coolSet)
{
    // Controlls fan and cooler
    if (_coolSet > 0)
    {
        digitalWrite(COOLER_PIN, 1);
        if (_fanSet < 1)
        {
            _fanSet = 1;
            currentSetting.fanSetting = 1;
            localSetting.fanSetting = currentSetting.fanSetting;
        }
    }
    else
    {
        digitalWrite(COOLER_PIN, 0);
    }
    
    switch (_fanSet)
    {
    case 0:
        digitalWrite(FAN_LOW, 0);
        digitalWrite(FAN_MEDIUM, 0);
        digitalWrite(FAN_HIGH, 0);
        digitalWrite(COOLER_PIN, 0);
        break;

    case 1:
        digitalWrite(FAN_LOW, 1);
        digitalWrite(FAN_MEDIUM, 0);
        digitalWrite(FAN_HIGH, 0);
        break;
    
    case 2:
        digitalWrite(FAN_LOW, 0);
        digitalWrite(FAN_MEDIUM, 1);
        digitalWrite(FAN_HIGH, 0);
        break;
        
    case 3:
        digitalWrite(FAN_LOW, 0);
        digitalWrite(FAN_MEDIUM, 0);
        digitalWrite(FAN_HIGH, 1);
        break;

    default:
        break;
    }
    
    
}

void UpdateSegment()
{
    if(segmentDelay.check() == 1)
    {
        SegmentDisplay.print4(Global_TempCurrent);
        segmentDelay.reset();
    }
}

