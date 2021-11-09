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
    double Global_TempSet = 23.00;
    int Global_FanSetting = 0;
    int Global_CoolerSetting = 0;

    double Local_TempSet = Global_TempSet;
    int Local_FanSetting = Global_FanSetting;
    int Local_CoolerSetting = Global_CoolerSetting;
    bool Local_Update = false;

    double Net_TempSet = Global_TempSet;
    int Net_FanSetting = Global_FanSetting;
    int Net_CoolerSetting = Global_CoolerSetting;
    bool Net_Update = false;

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
    byte pin_row[ROW_NUM] = {11,10,9,8};
    byte pin_col[COL_NUM] = {7,6,5,4};
    Keypad Key_Input = Keypad(makeKeymap(keys), pin_row, pin_col, COL_NUM, ROW_NUM);

// Heartbeat LED setup
    void HeartbeatLed(int _TimeDelay = 500); 

// LCD setup
    //raspi
    LiquidCrystal_I2C lcd(0x27,16,2);
    //DFrobot
    // LiquidCrystal_I2C lcd(0x20,16,2);
    void LCD_print();

    #define updateDelay 1
    Metro lcdUpdate = Metro(updateDelay);
    unsigned long lastUpdate_LCD;
    char tempFrameOne[16]{"               "};
    char tempFrameTwo[16]{"               "};


// Backlight
    void Backlight_set();
    #define BacklightTime 50000
    // #define BacklightTime 99999999 // Test time
    Chrono BacklightTimer;

// Temp get
    void UpdateTemp();
    #define temp_pin 34
    OneWire TempWire(temp_pin);
    DallasTemperature Temp1_Sensor(&TempWire);
    Metro TempDelay = Metro(1000);

// Temp control
    void TempController();
    #define Controller_Time 1*1000
    #define CoolerDelayTime 5*60000
    // #define CoolerDelayTime 100 // Test time
    Metro TempController_Metro = Metro(Controller_Time);
    Metro CoolerOffTime_Metro = Metro(100); //Short time for startup, removes delay
    bool Delay_reset = false;

// Net update
    void NetUpdate_TempStat();
    void NetSendUpdate();
    Metro NetTempDelay = Metro(3000);
    #define NetConnect 2

// Power controls
    void PowerController(int _FanSet, int _CoolSet);
    #define fan_low 25
    #define fan_med 26
    #define fan_high 27
    #define cooler_pin 28

// Segment display
    DFRobot_LedDisplayModule SegmentDisplay = DFRobot_LedDisplayModule(Wire, 0x48);
    void UpdateSegment();
    Metro segmentDelay = Metro(1000);

// Temp
    bool test = false;
    char chartempset[5];
    char chartempcurrent[5];
    char charfanset[1];
    char charcoolset[1];
    void interuptdelay();
    

/********************************************************/

// Menu Setup
    #define Start_menu 1000
    #define menu_items 2
    #define menu_hiddenItems 2
    #define menu_totalItems menu_hiddenItems + menu_items
    Menuclass MainMenu[menu_items + menu_hiddenItems + 1]{0,0,0,0,3};
    int mainmenu_index = 1;
    bool enter = false;
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
    change BacklightTime
    change CoolerDelayTime
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
        Wait Start_menu seconds
    
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

    Temp1_Sensor.begin();

    // Set pins 
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(temp_pin, INPUT);
    pinMode(NetConnect, 0);
    pinMode(33, INPUT);

    pinMode(fan_low, 1);
    pinMode(fan_med, 1);
    pinMode(fan_high, 1);
    pinMode(cooler_pin, 1);
    
    // Init threads
    threads.addThread(HeartbeatLed,500);
    threads.addThread(TempController);
    threads.addThread(Backlight_set);
    threads.addThread(UpdateTemp);

    // Fill in the menu
    strcpy(MainMenu[0].Lineone, "Offset menu     ");
    strcpy(MainMenu[0].Linetwo, "Offset menu     ");

    strcpy(MainMenu[1].Lineone, SW_Version);
    strcpy(MainMenu[1].Linetwo, "Conditioner     ");

    strcpy(MainMenu[2].Lineone, "Hidden line one ");
    strcpy(MainMenu[2].Linetwo, "Hidden line one ");

    strcpy(MainMenu[3].Lineone, "Set Temp        ");
    strcpy(MainMenu[3].Linetwo, "Off     Off     ");
    
    strcpy(MainMenu[4].Lineone, "Set Temp        ");
    strcpy(MainMenu[4].Linetwo, "Current         ");
    
    // Start the LCD
    lcd.init();
    lcd.backlight();

    TempDelay.reset();
    BacklightTimer.restart();
    NetTempDelay.reset();
    CoolerOffTime_Metro.reset();
    segmentDelay.reset();

    // attachInterrupt(digitalPinToInterrupt(NetConnect),interuptdelay, HIGH);

    // Show the start menu
    LCD_print();
    delay(Start_menu);
    mainmenu_index = 3;
}

/*************************************************************************************************************************/
void loop()
{    
    char keyinput = Key_Input.getKey();
    if (keyinput)
    {
        // Serial.println(keyinput);
        if (keyinput == '2')
        {
            // up
            mainmenu_index++;
        }
        else if (keyinput == '5')
        {
            // Down
            mainmenu_index--;
        }
        else if (keyinput == '6')
        {
            // Increase
            Local_TempSet += 0.5;
        }
        else if (keyinput == '4')
        {
            // Decreas
            Local_TempSet -= 0.5;        
        }
        else if (keyinput == '7')
        {
            // Fan down
            if (Local_FanSetting <= 0)
            {
                Local_FanSetting = 0;
            }
            else
            {
                Local_FanSetting--;
            }
            
        }
        else if (keyinput == '8')
        {
            // Fan up
            if (Local_FanSetting >= 3)
            {
                Local_FanSetting = 3;
            }
            else
            {
                Local_FanSetting++;
            }
        }
        else if (keyinput == 'C')
        {
            // Fan off
            Local_FanSetting = 0;
            // Cooler off
            Local_CoolerSetting = 0;
        }
        else if (keyinput == '*')
        {
            // Cooler down
            if (Local_CoolerSetting <= 0)
            {
                Local_CoolerSetting = 0;
            }
            else
            {
                Local_CoolerSetting--;
            }
        }
        else if (keyinput == '0')
        {
            // Cooler up
            if (Local_CoolerSetting >= 2)
            {
                Local_CoolerSetting = 2;
            }
            else if(Local_CoolerSetting == 0)
            {
                Local_CoolerSetting++;
                if (Local_FanSetting == 0)
                {
                    Local_FanSetting++;
                }                
            }
            else
            {
                Local_CoolerSetting++;
            }
        }
        else if (keyinput == 'D')
        {
            // Cooler off
            Local_CoolerSetting = 0;
        }
        else{}

        Local_Update = true;
        BacklightTimer.restart();
    }

    // If the index is greater than the items, reset it to the lowest menu item
    if (mainmenu_index > menu_totalItems)
    {
        mainmenu_index = menu_hiddenItems + 1;
    }
    // If the index is less than or equal to the number of hidden menus, reset it to the highest value
    else if (mainmenu_index <= menu_hiddenItems)
    {
        mainmenu_index = menu_totalItems;
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


    if (Local_Update && LastUpdate == !1)
    {
        // Serial.println("local");
        Local_Update = false;
        LastUpdate = 1;
        
        Global_CoolerSetting = Local_CoolerSetting;
        Global_FanSetting = Local_FanSetting;
        Global_TempSet = Local_TempSet;
        
        Net_CoolerSetting = Local_CoolerSetting;
        Net_FanSetting = Local_FanSetting;
        Net_TempSet = Local_TempSet;

        NetSendUpdate();
    }
    else if (Net_Update && LastUpdate == !2)
    {
        // Serial.println("Net");
        Net_Update = false;
        LastUpdate = 2;

        Global_CoolerSetting = Net_CoolerSetting;
        Global_FanSetting = Net_FanSetting;
        Global_TempSet = Net_TempSet;

        Local_CoolerSetting = Net_CoolerSetting;
        Local_FanSetting = Net_FanSetting;
        Local_TempSet = Net_TempSet;
        
    }
    else
    {
        LastUpdate = 0;
    }
    
    //Convert temp to char arrays
    dtostrf(Global_TempCurrent,4,2,chartempcurrent);
    dtostrf(Global_TempSet,4,2,chartempset);
    sprintf(charfanset, "%d", Global_FanSetting);
    sprintf(charcoolset, "%d", Global_CoolerSetting);

    //Update set temp - LCD
    MainMenu[3].Lineone[11] = chartempset[0];
    MainMenu[3].Lineone[12] = chartempset[1];
    MainMenu[3].Lineone[13] = chartempset[2];
    MainMenu[3].Lineone[14] = chartempset[3];
    MainMenu[3].Lineone[15] = chartempset[4];
    
    MainMenu[4].Lineone[11] = chartempset[0];
    MainMenu[4].Lineone[12] = chartempset[1];
    MainMenu[4].Lineone[13] = chartempset[2];
    MainMenu[4].Lineone[14] = chartempset[3];
    MainMenu[4].Lineone[15] = chartempset[4];


    switch (mainmenu_index)
    {        
        case 3:


            if (Global_FanSetting == 0)
            {
                MainMenu[3].Linetwo[0] = 'O';
                MainMenu[3].Linetwo[1] = 'f';
                MainMenu[3].Linetwo[2] = 'f';
                MainMenu[3].Linetwo[3] = ' ';
            }
            else
            {
                MainMenu[3].Linetwo[0] = 'F';
                MainMenu[3].Linetwo[1] = 'a';
                MainMenu[3].Linetwo[2] = 'n';
                MainMenu[3].Linetwo[3] = charfanset[0];
            }

            if (Global_CoolerSetting == 0)
            {
                MainMenu[3].Linetwo[8] = 'O';
                MainMenu[3].Linetwo[9] = 'f';
                MainMenu[3].Linetwo[10] = 'f';
                MainMenu[3].Linetwo[11] = ' ';
            }
            else if (Global_CoolerSetting == 1)            
            {
                MainMenu[3].Linetwo[8] = 'A';
                MainMenu[3].Linetwo[9] = 'u';
                MainMenu[3].Linetwo[10] = 't';
                MainMenu[3].Linetwo[11] = 'o';
            }
            else
            {
                MainMenu[3].Linetwo[8] = 'C';
                MainMenu[3].Linetwo[9] = 'o';
                MainMenu[3].Linetwo[10] = 'o';
                MainMenu[3].Linetwo[11] = 'l';
            }
            
        break;
        
        case 4:
            // Update current temp - LCD
            MainMenu[4].Linetwo[11] = chartempcurrent[0];
            MainMenu[4].Linetwo[12] = chartempcurrent[1];
            MainMenu[4].Linetwo[13] = chartempcurrent[2];
            MainMenu[4].Linetwo[14] = chartempcurrent[3];
            MainMenu[4].Linetwo[15] = chartempcurrent[4];
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
    LCD_print();
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

void LCD_print()
{
    char lineonein[16];
    char linetwoin[16];
    strcpy(lineonein, MainMenu[mainmenu_index].Lineone);
    strcpy(linetwoin, MainMenu[mainmenu_index].Linetwo);

    if(lcdUpdate.check() == 1){

        lcdUpdate.reset(); 

        for (unsigned int i = 0; i < sizeof(tempFrameOne) ; i++)
        {
            if (tempFrameOne[i] != lineonein[i])
            {
                tempFrameOne[i] = lineonein[i];
                lcd.setCursor(i,0);
                lcd.print(tempFrameOne[i]);
            }
        }

        for (unsigned int i = 0; i < sizeof(tempFrameTwo) ; i++)
        {
            if (tempFrameTwo[i] != linetwoin[i])
            {
                tempFrameTwo[i] = linetwoin[i];
                lcd.setCursor(i,1);
                lcd.print(tempFrameTwo[i]);
            }
        }
    }
}


void Backlight_set()
{
    while (1)
    {
        if (BacklightTimer.hasPassed(BacklightTime))
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
        if (TempDelay.check() == 1 && Temp1_Sensor.getDeviceCount() > 0)
        {
            Temp1_Sensor.requestTemperatures();
            delay(100);
            Global_TempCurrent = Temp1_Sensor.getTempCByIndex(0);
            // Global_TempCurrent = 27;
            TempDelay.reset();
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
        if (TempController_Metro.check() == 1 && Global_CoolerSetting == 1)
        {
            if (Global_TempCurrent > Global_TempSet && CoolerOffTime_Metro.check() == 1)
            {
                // Turn on
                PowerController(Global_FanSetting,1);
                _Cset = 1;
                Delay_reset = false;
                CoolerOffTime_Metro.interval(CoolerDelayTime);
            }
            else if (Global_TempCurrent < Global_TempSet && Delay_reset == false)
            {
                // Turn off
                PowerController(Global_FanSetting,0);
                CoolerOffTime_Metro.reset();
                _Cset = 0;
                Delay_reset = true;
            }
            else
            {
                // Serial.println("else");
                PowerController(Global_FanSetting,_Cset);
            }
            TempController_Metro.reset();
        }
        else if (Global_CoolerSetting == 2)
        {
            PowerController(Global_FanSetting, 1);
        }
        else if (Global_CoolerSetting == 0)
        {
            PowerController(Global_FanSetting, 0);
        }
    } 
}

void NetUpdate_TempStat()
{

    if (digitalRead(NetConnect) && NetTempDelay.check() == 1)
    {
        char _NetSend[16] = "set tempc ";

        strcat(_NetSend, chartempcurrent);
        strcat(_NetSend, " ");
        Serial1.println(_NetSend);
        Serial.println(_NetSend);
        NetTempDelay.reset();
    }
}

void NetSendUpdate()
{
    char _NetSend[16];
    // Serial.println("Send update");

    if (digitalRead(NetConnect) == 1)
    {
        // Serial.println("temps");
        NetTempDelay.reset();

        // Update set temp
        dtostrf(Global_TempSet,4,2,chartempset);
        strcpy(_NetSend, "set temps ");
        strcat(_NetSend, chartempset);
        Serial1.println(_NetSend);

        delay(100);

        // Update cooler setting
        strcpy(_NetSend, "set auto ");
        sprintf(charcoolset, "%d", Global_CoolerSetting);
        strcat(_NetSend, charcoolset);
        // _NetSend = "set auto ";
        Serial1.println(_NetSend);

        delay(100);

        // Update fan setting
        strcpy(_NetSend, "set fan ");
        sprintf(charfanset, "%d", Global_FanSetting);
        strcat(_NetSend, charfanset);
        Serial1.println(_NetSend);
    }
}

void interuptdelay()
{
    delay(100);
    while (Serial1.available())
    {
        Serial1.read();
    }
    
}

void serialEvent1()
{
    // delay(2000);
    // Serial.println("Start");
    if (digitalRead(NetConnect))
    {
        char rawcharin[32];
        int testint = Serial1.available();
        for (int i = 0; i < testint; i++)
        {
            rawcharin[i] = Serial1.read();
        }

        char Action[5];
        strncpy(Action, rawcharin, 4);
        Action[sizeof(Action) - 1] = '\0';

        // Serial.print("raw=");
        // Serial.println(rawcharin);
        // Serial.print("action=");
        // Serial.println(Action);
        
        if (strcmp(Action, "temp") == 0)
        {
            char charint[6];
            charint[0] = rawcharin[4];
            charint[1] = rawcharin[5];
            charint[2] = rawcharin[6];
            charint[3] = rawcharin[7];
            charint[4] = rawcharin[8];

            Net_TempSet = atof(charint);
            Serial.print("temp=");
            Serial.println(Net_TempSet);
        }
        else if (strcmp(Action, "auto") == 0)
        {
            Net_CoolerSetting = rawcharin[4] - '0';
            if (Net_CoolerSetting == 1 && Global_FanSetting == 0)
            {
                Global_FanSetting = 1;
                Net_FanSetting = 1;
                NetSendUpdate();
            }
            
        }
        else if (strcmp(Action, "fans") == 0)
        {
            Net_FanSetting = rawcharin[4] - '0';
            if (Net_FanSetting == 0)
            {
                // Serial.println("test");
                Global_CoolerSetting = 0;
                Net_CoolerSetting = 0;
                NetSendUpdate();
            }
            
        }
        else{}
        // Serial.println(Net_FanSetting);
        Net_Update = true;
    }
    else
    {
        while (Serial1.available())
        {
            Serial1.read();
        }
        
    }
}

void PowerController(int _FanSet, int _CoolSet)
{
    // Controlls fan and cooler
    if (_CoolSet > 0)
    {
        digitalWrite(cooler_pin, 1);
        if (_FanSet < 1)
        {
            _FanSet = 1;
            Global_FanSetting = 1;
            Local_FanSetting = Global_FanSetting;
        }
    }
    else
    {
        digitalWrite(cooler_pin, 0);
    }
    
    switch (_FanSet)
    {
    case 0:
        digitalWrite(fan_low, 0);
        digitalWrite(fan_med, 0);
        digitalWrite(fan_high, 0);
        digitalWrite(cooler_pin, 0);
        break;

    case 1:
        digitalWrite(fan_low, 1);
        digitalWrite(fan_med, 0);
        digitalWrite(fan_high, 0);
        break;
    
    case 2:
        digitalWrite(fan_low, 0);
        digitalWrite(fan_med, 1);
        digitalWrite(fan_high, 0);
        break;
        
    case 3:
        digitalWrite(fan_low, 0);
        digitalWrite(fan_med, 0);
        digitalWrite(fan_high, 1);
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

