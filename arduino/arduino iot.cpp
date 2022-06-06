// #include <arduino_secrets.h>
#include "thingProperties.h"
// #include <ArduinoIoTCloud.h>
#include <Chrono.h>

// Request and acknowledge
#define SER_REQ 0x00
#define SER_ACK 0x01

// Update required
#define SER_UPDATE_STAT 0x02
#define SER_UPDATE_REQ 0x03


// This file is used to create the code for the arduino iot device for remote access
// This code is copied and compiled on the arduino iot website

char inputData[10];
char outputData[6];
bool dataRecived = false;
bool updateReady = false;


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

    // Read the input serial data
    while (Serial1.available())
    {
      Serial1.readBytes(inputData, 10);
      dataRecived = true;
      //Serial.println("serial loop");
    }

    // Process the input data
    if (dataRecived == true)
    {
      Serial.println("Processing data");

      // Update current temperature
      memcpy(&currentTempurature, &inputData[0], sizeof currentTempurature);
      // Update set temperature
      memcpy(&setPoint, &inputData[4], sizeof setPoint);

      Serial.print("currentTempurature = ");
      Serial.println(currentTempurature);
      Serial.print("setPoint = ");
      Serial.println(setPoint);

      fanSetting = inputData[8] - '0';
      Serial.print("Fan = ");
      Serial.println(fanSetting);

      // Update cooler setting
      switch (inputData[9] - '0')
      {
        case 0:
          coolerSetting = false;
          Serial.println("Cooler = off");
          break;

        case 1:
          coolerSetting = true;
          Serial.println("Cooler = auto");
          break;

        case 2:
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

void onFanSettingChange()
{
  sendUpdate();
  Serial.println("Send update");
}

void onSetPointChange()
{
  sendUpdate();
  Serial.println("Send update");
}

void onCoolerSettingChange()
{
  sendUpdate();
  Serial.println("Send update");
}

void sendUpdate()
{
  // Add the set point to the output data
  memcpy(&outputData[0], &setPoint, sizeof setPoint);

  // Add the fan setting to the output data
  outputData[4] = '0' + fanSetting;

  // Add the cooler setting to the output data
  if (coolerSetting == true)
  {
    outputData[5] = '1';
  }
  else
  {
    outputData[5] = '0';
  }

  // Send the output data
  Serial1.write(outputData, 6);
}

