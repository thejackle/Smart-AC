// #include <arduino_secrets.h>
#include "thingProperties.h"
// #include <ArduinoIoTCloud.h>
#include <Chrono.h>
#include <Arduino.h>
#include <SerialCommands.h>

// This file is used to create the code for the arduino iot device for remote access
// This code is copied and compiled on the arduino iot website

char inputData[10];
char outputData[6];
bool dataRecived = false;

int fanSetting = 0;
bool coolerSetting = false;
float setPoint = 23.00;
float currentTempurature = 23.00;


/*****************************************************************************/

void setup() {
  
  // Start serial coms
  Serial.begin(9600);
  Serial1.begin(115200);
  delay(1500);

  // init iot properties
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection); 
  
  /*
     The following function allows you to obtain more information
     related to the state of network and IoT Cloud connection and errors
     the higher number the more granular information you’ll get.
     The default is 0 (only errors).
     Maximum is 4
 */
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();
  
  // Create settings defalut, set to default
  fanSetting = 0;
  coolerSetting = false;
  setPoint = 23.00;
  currentTempurature = 23.00;


  // Setup pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(2, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(2, LOW);

}

/*****************************************************************************/

void loop() 
{
  // Update iot connection
  ArduinoCloud.update();

  if (ArduinoCloud.connected())
  {
    // Set the LED to solid, to indicate if its connected
    digitalWrite(LED_BUILTIN, 1);
    digitalWrite(2, 1);
    Serial.println("Connected");
    delay(5000);
    
    // Read the input serial data
    while (Serial1.available())
    {
      Serial1.readBytes(inputData, 10);
      dataRecived = true;
      //Serial.println("serial loop");
    }
    
    // Process the input data
    if(dataRecived == true)
    {

      // Update current temperature
      memcpy(&inputData[0], &currentTempurature, sizeof currentTempurature);
      // Update set temperature
      memcpy(&inputData[4], &setPoint, sizeof setPoint);

      // Update fan setting
      switch (inputData[8])
      {
      case SER_FAN0:
        fanSetting = 0;
        break;
      
      case SER_FAN1:
        fanSetting = 1;
        break;
        
      case SER_FAN2:
        fanSetting = 2;
        break;
        
      case SER_FAN3:
        fanSetting = 3;
        break;

      default:
        break;
      }

      // Update cooler setting
      switch (inputData[9])
      {
      case SER_COOL_OFF:
        coolerSetting = false;
        break;
      
      case SER_COOL_AUTO:
        coolerSetting = true;
        break;
        
      case SER_COOL_ON:
        break;
        
      default:
        break;
      }

      dataRecived = false;
    }
    
  }
  else
  {
    // Blink the LED if it can't connect
    digitalWrite(LED_BUILTIN, 1);
    delay(100);
    digitalWrite(LED_BUILTIN, 0);
    delay(100);
  }
 
}


/*****************************************************************************/

void onAcFanSettingChange() 
{
  updateReady = true;
}

void onAcSetTempChange() 
{
  updateReady = true;
}

void onAcAutoSettingChange()
{
  updateReady = true;
}

void sendUpdate()
{
  // Add the set point to the output data
  memcpy(&outputData[0], &setPoint, sizeof setPoint);

  // Add the fan setting to the output data
  switch (fanSetting)
  {
  case 0:
    outputData[4] = SER_FAN0;
    break;
  
  case 1:
    outputData[4] = SER_FAN1;
    break;
  
  case 2:
    outputData[4] = SER_FAN2;
    break;
  
  case 3:
    outputData[4] = SER_FAN3;
    break;

  default:
    break;
  }

  // Add the cooler setting to the output data
  if(coolerSetting == true)
  {
    outputData[5] = SER_COOL_AUTO;
  }
  else
  {
    outputData[5] = SER_COOL_OFF;
  }

  // Send the output data
  Serial1.write(outputData, 6);
}

