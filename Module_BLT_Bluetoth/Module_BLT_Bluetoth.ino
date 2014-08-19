/*
   Module BLT and a demo for usage demo.
 */

//Bluetooth LE
#include <RFduinoBLE.h>


////////////////////////////////////////////////////////
// GPIO 0 ..6 definition
////////////////////////////////////////////////////////
// change this to match your SD shield or module;
// Information LED
const int led = 2;

// actual value of the LED
boolean ledValue = false;

// Distance sample data
long distanceValue = 2000000000;


/******************************************************
  Setup
/******************************************************/
void setup()
{
  // Open serial communications for debug and pc controlling
  Serial.begin(9600);

  //Set LED pin to output
  pinMode(led, OUTPUT);

  bltSetup();

}


/******************************************************
  Loop
/******************************************************/
void loop(void) {

  // simulate reed contact
  RFduino_ULPDelay( SECONDS(1) );
  distanceValue++;

  //Send data over BLE
  bltSendData(distanceValue);
  
  //Send message 
// sendMessage("HELLO_RFDUINO");

  switchLed();

}

/******************************************************
  Switch LED
/******************************************************/
void switchLed() {
  if (ledValue) ledValue = false;
  else ledValue = true;
  digitalWrite(led, ledValue);
}



/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
/*                                                                    */
/*        Module BLT - Bluetooth LE Trasmitter                        */
/*                                                                    */
/*  Version 1.0, 11.08.2014                                           */
/*  Sends a value via BT LE in the format char(28), closed with \0:   */
/*  D<long>: a long number, for instance "D2147483647"                */
/*  M<String>: a message,  for instance "MSD_READ_ERROR"              */
/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
typedef struct {
  
  //Temp. buffer with data
  char buf[28];
  
  // data string to send
  String data;
} Blt;
Blt blt;

// How often data will be send in milliseconds. Increase value to
// save energy
const int BLT_INTERVAL_MS = 1000;

// Name of the device, appears in the advertising. Keep it short.
const char* BLT_DEVICE_NAME = "DDO";

/**********************************************************************
   Initializes BLT module. Call this in setup().
/*********************************************************************/
void bltSetup() {

  
  //Set interval von BLE exchange //TODO Neede?
 // RFduinoBLE.advertisementInterval = BLT_INTERVAL_MS; 

  //set the BLE device name as it will appear when advertising
  RFduinoBLE.deviceName = BLT_DEVICE_NAME;

  // start the BLE stack
  RFduinoBLE.begin();
}

/**********************************************************************
  Sends a long value as distance message, e.g. "D12345". Character
  'D' will be add as prefix.
/*********************************************************************/
void bltSendData(long value) {

  // Send actual distance
  blt.data = "D" + String(value) + String('\0');

  blt.data.toCharArray(blt.buf, 28);
  RFduinoBLE.send(blt.buf, 28);

}

/**********************************************************************
  Sends message, e.g. "MERROR_SD_CARD_WRITE"
  String message Message text with < 26 characters, without lead 'M'.
/*********************************************************************/
void bltSendMessage(String message) {

  // Send actual distance
  blt.data = "M" + message + String('\0');

  blt.data.toCharArray(blt.buf, 28);
  RFduinoBLE.send(blt.buf, 28);

}

