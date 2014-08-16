/*
   Bike Distance Counter

   Features:
   - Needs no battery, just bike dynamo and power support
     of the buffered front light
   - Read the actual wheel rotation and store it on SD card
   - Communicate the value via Bluetooth 4.0 to the Android App



 */

// include the SD library:
#include <SPI.h>
#include <SD.h>
#include <RFduinoBLE.h>



////////////////////////////////////////////////////////
// GPIO 0 ..6 definition
////////////////////////////////////////////////////////
const int LED = 2;
const int REED = 0;
const int TRIGGER_PIN = 1;    // select the input pin for the potentiometer


////////////////////////////////////////////////////////
// Variable declarations
////////////////////////////////////////////////////////
// Actual Distance value as ticks (rotation counter)
long counter;
long lastSavedCounter;

// True, if a hard error occured
boolean error;

// Temp return value of an method call.
int ret = 0;

// TRUE, when SD card is online
boolean cardOn;
// Try periodically to open SD card
long cardNextBeginRetry;

String errorMessage;

// In error case count for next sending
boolean errorSend;

/******************************************************
  Setup
/******************************************************/
void setup()
{

  // Open serial communications for debug and pc controlling
  sloSetup(true);
  counter = 0;
  lastSavedCounter = 0;

  error = false;
  errorMessage = "-";
  errorSend = false;

  cardOn = false;

  // Initialize the modules
  lecSetup(LED);
  bltSetup("UP");
  rstSetup(REED);
  psmSetup(TRIGGER_PIN);

  cardNextBeginRetry = millis() + 1000;

  //  Serial.println("End of setup()");
}
/******************************************************
  Loop
/******************************************************/
void loop(void) {

  //Card module needs more time to come up
  //Retry until successful connected
  if (!cardOn && millis() > cardNextBeginRetry) {
    ret = etpSetup();
    if (ret == 0) {
      //Add stored value to actual turns
      counter += etpGetOldDistance();
      lastSavedCounter = etpGetOldDistance();
      cardOn = true;
      lecBlink();
    }
    else
      //Wait a second to try it again
      cardNextBeginRetry = millis() + 1000;
  }


  // Normal mode
  else {

    //Count wheels rotations and send to app
    if (rstRead()) {
      counter++;
      lecFlash();
      bltSendData(counter);
    }

    //Save to card when power breaks down
    if (psmCheck() && counter != lastSavedCounter) {
      if (cardOn) {
        ret = etpWrite(counter);
        if (ret == 0)
          lecBlink();
        else
        { lecIntervalStart();
          error = true;
          errorMessage = etpErrorToString(ret);
        }
      }
      else
      { lecIntervalStart();
        error = true;
        errorMessage = "ERROR_CARD_NOT_INIT";
      }
    }
  }
  //Give LED the control
  lecControl();
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
void bltSetup(String welcomeMessage) {


  //Set interval von BLE exchange //TODO Neede?
  // RFduinoBLE.advertisementInterval = BLT_INTERVAL_MS;

  //set the BLE device name as it will appear when advertising
  RFduinoBLE.deviceName = BLT_DEVICE_NAME;

  // start the BLE stack
  RFduinoBLE.begin();

  //Initialize send data
  bltSendMessage(welcomeMessage);
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
/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
/*                                                                    */
/*        Module LOG - A simple logger                                */
/*                                                                    */
/*  Version 1.6, 11.08.2014                                           */
/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
typedef struct {
  //True, if logging to serial out
  boolean serialSwitch;
} SLO;
SLO slo;
/***********************************************************************
     Initializes the module. Call this in the setup().
************************************************************************/

int sloSetup(boolean serialSwitch) {
  slo.serialSwitch = serialSwitch;
  if(slo.SerialSwitch()
    Serial.begin(9600);
}

void sloLogS(String text) {
  if (slo.serialSwitch)
    Serial.println(text);
}

void sloLogL(String text, long value) {
  if (slo.serialSwitch) {
    Serial.print(text + ": ");
    Serial.println(value);
  }
}
void sloLogB(String text, boolean value) {
  if (slo.serialSwitch) {
    Serial.print(text + ": ");
    Serial.println(value);
  }
}
/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
/*                                                                    */
/*        Module ETP - Error Tolerance Persisting to SD Card          */
/*                                                                    */
/*  Version 1.1, 11.08.2014                                           */
/*  There are two files: one with the actual and one with the         */
/*  previous value. Only the new one will be touched. If an error     */
/*  occurs, there is always the oder one as fallback.                 */
/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
typedef struct {
  //TRUE, when SD card connected with SD.begin()
  // (SD.begin() returns TRUE only one time)
  boolean sdBegin;
  long oldDistance;
  int lastFilenumber;
  int nrReadRetries;
} ETP;
ETP etp;

// RFDuino: Chip select is GPIO 6
const int ETP_GPIO_CHIP_SELECT = 6;

// Card error occured
const int ETP_ERROR_NR_READ_RETRIES = 1;
const int ETP_ERROR_WRITING = 2;
const int ETP_ERROR_MAX_FILE_NR_REACHED = 3;

//How often card reading can tried
const int ETP_MAX_NR_READ_RETRIES = 3;

//Biggest supported file number
const int ETP_MAX_FILE_NR = 10;

const long ETP_NULL = -1;

String etpErrorToString(int error) {
  if (error == ETP_ERROR_NR_READ_RETRIES)
    return "ETP_ERROR_NR_READ_RETRIES";

  if (error == ETP_ERROR_WRITING)
    return "ETP_ERROR_WRITING";

  if (error == ETP_ERROR_MAX_FILE_NR_REACHED)
    return "ETP_ERROR_MAX_FILE_NR_REACHED";

  return "ERROR_UNKNOWN";

}

/**********************************************************************
   Returns value from file or, if not read, 0.
***********************************************************************/
long etpGetDistance() {
  return etp.oldDistance;
}

/**********************************************************************
  Checks, if file exists and removes it. Availability of SD Card
  will not be checked.
  Returns true, if file doesn't exists (anymore), otherwise false;
***********************************************************************/
boolean etpPrepareFileForCreating(char * cfName) {

  // File exist
  if (SD.exists(cfName)) {
    SD.remove(cfName);
    if (!SD.exists(cfName)) {
      //Yeah! Let's take this filename
      return true;
    }
  }
  else {
    //Yeah! Let's take this filename
    return true;
  }
  // File exists and could not be removed.
  return false;
}

/**********************************************************************
  Writes actual distance to file. Return 0 if successfull or otherwise
  error code ETP_ERROR_WRITE.
***********************************************************************/
int etpWrite(long value) {

  boolean goOn;
  int fNr;
  String fName;
  char cfName[11];
  File myFile;
  String strValue = String(value);
  byte bytes = 0;

  logL("etpWrite() with value", value);


  goOn = true;
  fNr = 0;

  //Search for free filename. Remove existing but not actual files.
  while (goOn) {

    //avoid overflow
    if (fNr >= ETP_MAX_FILE_NR)
    {
      log("Cancel with ETP_ERROR_MAX_FILE_NR_REACHED");
      return ETP_ERROR_MAX_FILE_NR_REACHED;
    }

    // File to search("1", "2", "3" etc.)
    fNr++;

    //Skip file of etp.oldDistance
    if (fNr == etp.lastFilenumber) {
      fNr++;
      logL("Skip old file nr", fNr);
    }

    logL("Search file nr", fNr);

    //First try: nok-File
    fName = String(fNr);
    fName.toCharArray(cfName, 11);

    if (etpPrepareFileForCreating(cfName))
    {
      log("free file place " + fName);
      //Let's try ok-File
      fName = String(fNr) + ".ok";
      fName.toCharArray(cfName, 11);
      if (etpPrepareFileForCreating(cfName)) {
        log("free file place, end while: " + fName);
        goOn = false;
      } else
        log("file exists: " + fName);
    } else
      log("file exists: " + fName);

  }

  //Now we have found a not existing filename
  myFile = SD.open(cfName, FILE_WRITE);
  if (myFile) {
    log("File opend for writing: " + fName);
    log("Write value: " + strValue);
    bytes = myFile.println(strValue);
    myFile.close();
    logL("Wrote byte number", bytes);
  }
  else {
    log("Could not open file for writing. Returning ETP_ERROR_WRITE: " + fName);
    return ETP_ERROR_WRITING;
  }


}

/***********************************************************************
     Initializes the module. Call this in the setup().
************************************************************************/
int etpSetup() {
  //Just initialize the vars
  etp.oldDistance = 0;
  etp.lastFilenumber = 0;
  etp.nrReadRetries = 0;
  etp.sdBegin = false;
}

/***********************************************************************
     Checks if card read may retries. Returns 0 if, retry is possible
     otherwise FALSE. Call this in loop() to device if readValue() may
     be called once again.
************************************************************************/
int etpCheckReadRetryOverflow() {
  if (etp.nrReadRetries >= ETP_MAX_NR_READ_RETRIES)
    return ETP_ERROR_NR_READ_RETRIES;
  else return 0;
}

/***********************************************************************
    Try to read the value by reading all *.ok-files. The file with the
    'high score' wins. Must be called in loop() until TRUE is returned.
    Sets etp.oldDistance and etp.lastFilenumber. Increases read retry
    counter.
************************************************************************/
boolean etpReadValue() {

  log("etpReadValue()");
  int ret = 0;
  boolean goOn;
  File next;
  int fNr;
  String fName;
  char cfName[11];
  File f;

  // Help vars
  long value1 = ETP_NULL;
  String fName1;

  goOn = true;
  fNr = 0;

  //Increment retry counter (avoid overflow)
  if (etp.nrReadRetries < ETP_MAX_NR_READ_RETRIES)
    etp.nrReadRetries++;

  logL("Nr of retries", etp.nrReadRetries);

  //Card initializtion must be done
  if (!etp.sdBegin) {
    if (!SD.begin(ETP_GPIO_CHIP_SELECT)) {
      log("SD.begin() false, leave etpReadValue()");
      return false;
    }
    else {
      etp.sdBegin = true;
      log("SD.begin() true");
    }
  }

  ///////////////////////////////////////////////////////////////
  // Loop directory and try to find last ok file and read its value.
  // Results are: stored value and file name
  // for next storing.
  ///////////////////////////////////////////////////////////////
  log("Entering while");
  while (goOn) {

    // File to search("1", "2", "3" etc.)
    fNr++;
    //Filename to search for, "1.ok", "2.ok" etc.
    fName = String(fNr) + ".ok";
    fName.toCharArray(cfName, 11);
    // log("Searching for file: " + fName);

    // File exist
    if (SD.exists(cfName)) {
      //log("Found");

      // Try to open file
      fName.toCharArray(cfName, 11);
      f = SD.open(cfName);

      // File ok, can be opend
      if (f) {
        //log("Opened " + fName);
        // We found the value
        value1 = etpReadFile(f);
        if (value1 != ETP_NULL && value1 > etp.oldDistance) {
          //logL("Set etp.LastFilenumber: ", fNr);
          etp.lastFilenumber = fNr;
          // logL("Set etp.oldDistance: ", value1);
          etp.oldDistance = value1;
        }
      }
      f.close();
    }

    // File doesn't exist
    else  {
      //   logL("File doesn't exist, finish searching. fNr", fNr);
      //Found no file: Counter starts with '0'
      if (value1 == ETP_NULL) {
        value1 = 0;
        etp.oldDistance = value1;
        etp.lastFilenumber = 0;
      }
      // End searching
      goOn = false;
    }
  }

  logL("Set etp.LastFilenumber", fNr);
  logL("Set etp.oldDistance", value1);

  //Yeah, Success!
  return true;

}
/**
Read file content as long value. IF file could not be opend or
value is no long type, ETP_NULL will be returnd. File must
be opend and will not be closed.
*/
long etpReadFile(File myFile) {
  char fChar;
  String line = "0";
  long value = ETP_NULL;
  // read first line from the file, ignore the rest:
  while (myFile.available()) {
    fChar = myFile.read();

    //Exit after first line
    if (fChar == '\n')
      break;
    line.concat(fChar);
  }

  value = stringToLong(line);
  if (value >= 0)
    return value;
  else
    return ETP_NULL;
}

/**********************************************************************
Converts a String to long. Non-digit characates
will be ignored.
***********************************************************************/
long stringToLong(String digits) {
  char next = ' - ';
  int i = 0;
  int factor = 1;
  long ret = 0;


  for (int j = digits.length() - 1; j >= 0; j--)
  {
    next = digits.charAt(j);
    switch (next) {
      case '0':
        i = 0;
        break;
      case '1':
        i = 1;
        break;
      case '2':
        i = 2;
        break;
      case '3':
        i = 3;
        break;
      case '4':
        i = 4;
        break;
      case '5':
        i = 5;
        break;
      case '6':
        i = 6;
        break;
      case '7':
        i = 7;
        break;
      case '8':
        i = 8;
        break;
      case '9':
        i = 9;
        break;
      default:
        i = -1;
    }
    if (i >= 0) {
      ret = ret + (factor * i);
      factor = factor * 10;
    }
  }

  return ret;
}

/**
Returns the stored distance value read from file.
*/
long etpGetOldDistance() {
  return etp.oldDistance;
}

/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
/*        Module LEC - LED Control                                    */
/*                                                                    */
/*  Version 1.0, 11.08.2014                                           */
/*  Supports one LED with different light programs. A program can     */
/*  only be started once and there can only run on program at once.   */
/*  There is priority order between the programs:                     */
/*  Prio 1: Interval on/off: flashing slowly                          */
/*  Prio 2: Quick blinking for 5 times.                               */
/*  Prio 3: Single flash.                                             */
/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
typedef struct {
  int ledStatus;
  int gpio;
  long endMs;
  int counter;
  boolean blnk;
  boolean flash;
  boolean interval;
} Lec;
Lec lec;

const int LEC_BLINK_MS = 50;
const int LEC_INTERVAL_MS = 500;

/**********************************************************************
  Starts blinking the LED until lecIntervalStop() is called. Priority 1.
  lecControl() must be called within the loop().
/*********************************************************************/
void lecIntervalStart() {

  //Can only be started once
  if (lec.interval)
    return;

  //Stop all other programs
  lec.blnk = false;
  lec.flash = false;

  //Start interval
  lec.endMs = millis() + LEC_INTERVAL_MS;
  lec.ledStatus = 1;
  lec.interval = true;
  digitalWrite(lec.gpio, lec.ledStatus);


}

/**********************************************************************
  Stops a with lecIntervalStart() started interval immediatly.
/*********************************************************************/
void lecIntervalStop() {
  if (!lec.interval)
    return;

  lec.endMs = 0;
  lec.ledStatus = 0;
  lec.interval = false;
  digitalWrite(lec.gpio, lec.ledStatus);
}


/**********************************************************************
  Flashes the LED for five times. Priority 2. lecControl() must be
  called within the loop().
/*********************************************************************/
void lecBlink() {

  //Only start blink if not interval is running
  if (lec.interval || lec.blnk)
    return;

  //Stop flash program
  lec.flash = false;


  //Start blink
  lec.counter = 5;
  lec.endMs = millis() + LEC_BLINK_MS;
  lec.ledStatus = 1;
  lec.blnk = true;
  digitalWrite(lec.gpio, lec.ledStatus);
}

/**********************************************************************
  Flashes the LED for 100 milliseconds. Priority 3. lecControl() must be
  called within the loop().
/*********************************************************************/
void lecFlash() {

  //Only start flash program if nothing other is running
  if (lec.interval || lec.blnk || lec.flash)
    return;


  //Start flash
  lec.endMs = millis() + 100;
  Serial.println(millis());
  lec.ledStatus = 1;
  lec.flash = true;
  digitalWrite(lec.gpio, lec.ledStatus);
}


/**********************************************************************
  Must be called within loop() to give LEC a change to update the
  LED status.
/*********************************************************************/
void lecControl() {

  //cotrol blink
  if (lec.interval == true) {
    if (millis() > lec.endMs) {
      if (lec.ledStatus == 1)
        lec.ledStatus = 0;
      else
        lec.ledStatus = 1;
      lec.endMs = millis() + LEC_INTERVAL_MS;
      digitalWrite(lec.gpio, lec.ledStatus);
    }
  }

  //cotrol blink
  else if (lec.blnk == true) {
    if (millis() > lec.endMs) {
      if (lec.ledStatus == 1)
        lec.ledStatus = 0;
      else
        lec.ledStatus = 1;
      lec.counter--;
      if (lec.counter <= 0 && lec.ledStatus == 0)
        lec.blnk = false;
      else
        lec.endMs = millis() + LEC_BLINK_MS;
      digitalWrite(lec.gpio, lec.ledStatus);
    }
  }

  //End of flash
  else if (lec.flash == true) {
    if (millis() > lec.endMs) {
      lec.ledStatus = 0;
      lec.flash = false;
      digitalWrite(lec.gpio, lec.ledStatus);
    }
  }
}

/**********************************************************************
  Toggles the light between on and off.
/**********************************************************************/
void lecToggleLed() {
  if (lec.ledStatus == 0)
    lec.ledStatus = 1;
  else
    lec.ledStatus = 0;
  digitalWrite(lec.gpio, lec.ledStatus);
}
/**
   Initializes LEC module. Call this in setup().
   int gpioLed GPIO number of the LED
*/
void lecSetup(int gpioLed) {
  lec.ledStatus = 0;
  lec.gpio = gpioLed;
  lec.counter = 0;
  pinMode(lec.gpio, OUTPUT);
}
/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
/*                      Power Supply Module                           */
/*                                                                    */
/*  Version 1.0, 11.08.2014                                           */
/* Secondary voltage signal on analog input for controlling power     */
/* fade out. There is a bycicle dynamo who powers the B&M LED front   */
/* light- Lumotec IQ. The B&M has two outputs: One is the LED power,  */
/* a DC voltage limited to ~ 3.5 V and with a big capicator for stand */
/* light support. This power will be used for powering the RFDuino.   */
/* The second output of the B&M is for the taillight, it is the AC    */
/* voltage of the dynamo and can be >> than 3.5 V. For the RFDuino    */
/* this signal will be used for triggering the end of power. Be       */
/* careful, the maximum input voltage of RFDuino is 3.6 V AC. It has  */
/* to be limited and rectified.                                       */
/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
// Variables for Powermanagement all starting with 'power'
typedef struct {
  long startTime;

  // variable to store the value coming from the sensor
  int triggerValue;

  //GPIO of the voltage power in signal that is the trigger
  int triggerPin;

  // Temp var measures latency timer after power down for
  // smoothing the voltage oscillation of the trigger signal
  // (converted AC voltage)
  long timer;

  // Switch is true, if voltage drop has been communicated once
  // psmCheck(). Otherwise false;
  boolean fired;

  // Counts number of trigger voltage above threshold. The
  // psmCheck() only fires, if system is long enough 'up'
  // since last fire. This avoids fireing at low voltage.
  int upCount;

  // Local variable for psmCheck()
  boolean ret;

} Psm;
Psm psm;

// Time between two measurement. Because the trigger signal is
// a AC signal the normal sinus wave has to be compensated
// The value depends of the dynamo's AC frequency per revolution
// and the mininum velocity that has to be recognized
// Minimum supported velocity: 3 km/h (0.83 m/s)
// Shimano: ~ 10 1/revolution -> 166 ms
const long PSM_LATENCY = 166;

// Defines the analog input value the voltage must fall below for
// fireing powerCheck() = true.
// Increase the value if trigger signal has a very low voltage value
const int PSM_THRESHOLD = 950;

// Latency time in milliseconds before powerCheck() will fire for the first
// time or for the next time after a powerCheck() returned true.
// Increase the value if very short distances should not be stored.
const int PSM_START_LATENCY_DURATION = 2000;

// Number of high voltage trigger signals that have to be reached before
// powerCheck() can fire.
// Increase the value, if the system has to be up for a longer time before
// next fireing.
const int PSM_UP_COUNT_LIMIT = 10000;

// The trigger voltage must be above this value to increment the upCount.
// Decrease this, if the upCount doesn't reach the UP_COUNT_MIN.
// Increase this, if there is not enough voltage when the powerCheck() fires.
const int PSM_UP_THRESHOLD = 980;


/**
  Returns for one time true, if power is going down, otherwise false.
  For a latency time after setup and after fireing true
  false will be returned.
  Use this in the loop() to check when power breaks down.
  Reads the analogInput of the trigger sginal.
*/
boolean psmCheck() {
  //  Serial.println("psmCheck()");

  psm.ret = false;

  // Read trigger input
  psm.triggerValue = analogRead(psm.triggerPin);
  // Serial.println(analogRead(1));

  // Check up criteria
  if (psm.triggerValue >= PSM_UP_THRESHOLD && psm.upCount < PSM_UP_COUNT_LIMIT) {
    psm.upCount ++;
  }

  // If trigger signal high, reset timer and switch
  if (psm.triggerValue >= PSM_THRESHOLD) {
    psm.timer = millis() + PSM_LATENCY;
    psm.fired = false;
  }

  //Normal mode starts after 1 second up time
  if (millis() > psm.startTime) {
    // Check if trigger is low for latency time
    if (psm.triggerValue < PSM_THRESHOLD //
        && millis() > psm.timer  //
        && psm.fired == false //
        && psm.upCount >= PSM_UP_COUNT_LIMIT)
    {
      psm.ret = true;
      psm.fired = true;
      psm.startTime = millis() + PSM_START_LATENCY_DURATION;
      psm.upCount = 0;
    }
  }

  return psm.ret;

}
/**
  Initialize power supply module. Call this once in startup().
  triggerPin: GPIO with the voltage trigger signal
*/
void psmSetup(int triggerPin) {

  // Let the Board time for start up
  psm.startTime = millis() + PSM_START_LATENCY_DURATION;

  // Init value for a good feeling
  psm.triggerValue = 0;

  // Reset temp. var
  psm.timer = 0;

  // Not send yet
  psm.fired = false;

  // Define GPIO with the trigger signal
  psm.triggerPin = triggerPin;

  // Reset limit
  psm.upCount = 0;

  // declare the analog in pin as input (default)
  //  pinMode(psm.triggerPin, INPUT);
  //  digitalWrite(psm.triggerPin, LOW);

}


/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
/*        Module RST - Reed Switch Trigger                            */
/*                                                                    */
/*  Version 1.0, 11.08.2014                                           */
/*  Read the reed contact. Works contact bounce free.                 */
/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
typedef struct {
  // GPIO of the reed input
  int gpio;
  // Last reed value
  int last;

  // act value
  int act;

  // Time when next value can be read
  // to elimate switch bouncing
  long nextMs;

  boolean readReturn;
} Rst;
Rst rst;

// For filtering hardware bouncing
const long RST_BOUNCE_MS = 10;

/**
   Returns true, if reed switched from off to on. Use this in loop()
   to recognize wheel movement.
*/
boolean rstRead() {

  //Hardware bouncing time over
  if (millis() > rst.nextMs) {

    rst.act = digitalRead(REED);
    if (rst.act == 1 && rst.last == 0) {
      rst.nextMs = millis() + RST_BOUNCE_MS;
      rst.readReturn = true;
    }
    else
      rst.readReturn = false;

    rst.last = rst.act;
  }
  else
    rst.readReturn = false;

  return rst.readReturn;

}

/**
   Initializes RST module. Call this in setup().
   int gpioReed GPIO number of the reed input signal
*/
void rstSetup(int gpioReed) {
  rst.last = 0;
  rst.act = 0;
  rst.nextMs = 0;
  rst.readReturn = false;

  rst.gpio = gpioReed;
  pinMode(rst.gpio, INPUT);
}

