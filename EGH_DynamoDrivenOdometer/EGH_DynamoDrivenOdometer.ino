/*
   Bike Distance Counter
   Features:
   - Needs no battery, just bike dynamo and power support
     of the buffered front light
   - Read the actual distance via Bluetooth 4.0 with the Android App
 */

// include the SD library:
#include <SPI.h>
#include <SD.h>


////////////////////////////////////////////////////////
// GPIO 0 ..6 definition
////////////////////////////////////////////////////////
const int LED = 2;
const int REED = 0;
const int TRIGGER_PIN = 1;    // select the input pin for the potentiometer


////////////////////////////////////////////////////////
// Variable declarations
////////////////////////////////////////////////////////
// Actual Distance value (TODO: switch from ticks to KM)
int counter;
int lastSavedCounter;


/******************************************************
  Setup
/******************************************************/
void setup()
{

  // Open serial communications for debug and pc controlling
  Serial.begin(9600);

  // Initialize the modules
  rstSetup(REED);
  lecSetup(LED);
  powerSetup(TRIGGER_PIN);

  int ret = 0;
  ret = etpSetup();
  if (ret > 0) {
    Serial.print(ret);
    Serial.println(": Error in etpSetup().Cancel program.");
  }
  counter = etpGetOldDistance();
  lastSavedCounter = counter;
  

  Serial.println("End of setup()");
}
/******************************************************
  Loop
/******************************************************/
void loop(void) {

  //Count wheels rotations
  if (rstRead()) {
    counter++;
    lecSwitchLed();
  }
  
  //Save to card when power breaks down
  if(powerCheck() && counter != lastSavedCounter){
//    etpWrite(counter);
}


}

/**********************************************************************/
/*        Module ETP - Error Tolerance Persisting to SD Card          */
/**********************************************************************/
typedef struct {
  long oldDistance;
  String nextFilename;
  char cNextFilename[8];
} ETP;
ETP etp;

// RFDuino: Chip select is GPIO 6
const int ETP_GPIO_CHIP_SELECT = 6;

// Card error occured
const int ETP_ERROR_CARD = 1;
const int ETP_ERROR_WRITE = 2;

const long ETP_NULL = -1;

/**
  Writes actual distance to file. Return 0 if successfull or otherwise
  error code.
*/
int etpWrite(long value) {
  File myFile;
  int ret = 0;
  String strValue = String(value);
  byte bytes = 0;

  Serial.print("etpWrite: ");
  Serial.println(value);

  Serial.println("Value as String: '" + strValue + "'");

  Serial.print("Filename = '");
  Serial.print(etp.cNextFilename);
  Serial.println("'");

  //Remove file with old content

  if (SD.exists(etp.cNextFilename))
  {
    Serial.print("File exists, will remove it... ");
    SD.remove(etp.cNextFilename);
    if (SD.exists(etp.cNextFilename)) {
      Serial.println(" Failed! Returning ETP_ERROR_WRITE");
      return ETP_ERROR_WRITE;
    } else
      Serial.println("Done!");
  } else {
    Serial.println("File doesn't exist, nothing to delete. ");
  }

  myFile = SD.open(etp.cNextFilename, FILE_WRITE);
  if (myFile) {
    Serial.print("No. of written bytes = " );
    //Cursor to begin
    //myFile.seek(0);
    bytes = myFile.println(strValue);
      myFile.close();
    Serial.println(bytes);
  }
  else {
    Serial.println("Could not open file for writing. Returning ETP_ERROR_WRITE");
    ret = ETP_ERROR_WRITE;
  }



}
/**
Initialize the ETP module. Determines the old distance value and
defines the file name for the new distance. Must be called in
setup() and as the first method of this module.
Return int 0, if setup was succesful, otherwise a ETP_ERROR* code
*/
int etpSetup() {
  Serial.println("etpSetup()");
  int ret = 0;
  // Find two valid files
  if (!SD.begin(ETP_GPIO_CHIP_SELECT)) {
    Serial.println("SD.begin() error. Returning ETP_ERROR_CARD");
    return ETP_ERROR_CARD;
  }

  //Initialize etp-variables
  etp.nextFilename = "";

  //Search for two files in the root directory
  boolean goOn;
  File next;
  int fNr;
  String fName;
  char cfName[8];
  File f;

  // Help vars
  long value1 = ETP_NULL;
  long value2 = ETP_NULL;
  String fName1;
  String fName2;

  goOn = true;
  fNr = 0;

  ///////////////////////////////////////////////////////////////
  // Loop directory and try to find two files with a valid value.
  // Results are: both distances (old and act) and file name
  // for next storing.
  ///////////////////////////////////////////////////////////////
  Serial.println("Entering while ");
  while (goOn) {

    // File to search("1", "2", "3" etc.)
    fNr++;
    fName = String(fNr);
    fName.toCharArray(cfName, 8);
    Serial.print("Searching for file = '" + fName + "'. As character array:'");
    Serial.print(cfName );
    Serial.println("'");

    // File exist
    if (SD.exists(cfName)) {
      Serial.println("File Found. Try to open it.");

      // Try to open file
      fName.toCharArray(cfName, 8);
      f = SD.open(cfName);

      // File ok, can be opend / is valid
      if (f) {
        Serial.println("Opened.");
        // We found the first value
        if (value1 == ETP_NULL) {
          Serial.println("Take it for value1. Read from file...");
          value1 = etpReadFile(f);
          Serial.println(value1);
          if (value1 != ETP_NULL) {
            fName1 = fName;
          }

        }
        // We found the second value
        else {
          Serial.println("Take it for value2. Read from file...");
          value2 = etpReadFile(f);
          Serial.println(value2);
          if (value2 != ETP_NULL) {
            fName2 = fName;
            // End of while
            goOn = false;
          }
        }
        f.close();
      }
    } else  {
      Serial.println("File doesn't exist. Exit while.");
      goOn = false;
    }
  }

  Serial.println("Determine the valid distance value");
  //--- Determine the valid distance value ---
  // At least one file was found
  if (value1 != ETP_NULL || value2 != ETP_NULL) {
    etp.oldDistance = max(value1, value2);
    Serial.print("Set etp.oldDistance = ");
    Serial.println(etp.oldDistance);

    if (value2 == ETP_NULL) {
      etp.nextFilename = fName;
    } else
      // Reuse older file as next fle
      if (value1 < value2)
        etp.nextFilename = fName1;
      else
        etp.nextFilename = fName2;
  }
  else {
    Serial.println("Set etp.oldDistance = 0");
    etp.oldDistance = 0;
    etp.nextFilename = fName;
  }
  Serial.println("Set etp.nextFilename = " + etp.nextFilename);

  //SD needs filename as char array not as String
  if (etp.nextFilename.length() > 0 ) {
    etp.nextFilename.toCharArray(etp.cNextFilename, 8);
    Serial.print("Set etp.cNextFilename = ");
    Serial.println(etp.cNextFilename);
  }


  Serial.println("End of etpSetup()");

  return ret;
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

/******************************************************
Converts a String to long. Non-digit characates
will be ignored.
/******************************************************/
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
int etpGetOldDistance() {
  return etp.oldDistance;
}
/**********************************************************************/
/*        Module RST - Reed Switch Trigger                            */
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
/**********************************************************************/
/*        Module LEC - LED Control                                    */
/*  Supports one LED with different light programs. A program can     */
/*  only be started once and there can only run on program at once.   */
/*  There is priority order between the programs:                     */
/*  Prio 1: Interval on/off: flashing slowly                          */
/*  Prio 2: Quick blinking for 5 times.                               */
/*  Prio 3: Single flash.                                             */
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
/************************************************************/
/*              Power Supply Module                         */
/************************************************************/
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
  // powerCheck(). Otherwise false;
  boolean fired;
  
  // Counts number of trigger voltage above threshold. The 
  // powerCheck() only fires, if system is long enough 'up'
  // since last fire. This avoids fireing at low voltage.
  int upCount;

} Power;
Power power;

// Time between two measurement. Because the trigger signal is
// a AC signal the normal sinus wave has to be compensated
// The value depends of the dynamo's AC frequency per revolution
// and the mininum velocity that has to be recognized
// Minimum supported velocity: 3 km/h (0.83 m/s)
// Shimano: ~ 10 1/revolution -> 166 ms
const long POWER_LATENCY = 166;

// Defines the analog input value the voltage must fall below for
// fireing powerCheck() = true.
// Increase the value if trigger signal has a very low voltage value
const int POWER_THRESHOLD = 950;

// Latency time in milliseconds before powerCheck() will fire for the first 
// time or for the next time after a powerCheck() returned true.
// Increase the value if very short distances should not be stored.
const int POWER_START_LATENCY_DURATION = 2000;

// Number of high voltage trigger signals that have to be reached before 
// powerCheck() can fire.
// Increase the value, if the system has to be up for a longer time before
// next fireing.
const int POWER_UP_COUNT_LIMIT = 10000;

// The trigger voltage must be above this value to increment the upCount.
// Decrease this, if the upCount doesn't reach the UP_COUNT_MIN.
// Increase this, if there is not enough voltage when the powerCheck() fires. 
const int POWER_UP_THRESHOLD = 980;


/**
  Returns for one time true, if power is going down, otherwise false.
  For a latency time after setup and after fireing true 
  false will be returned.
  Use this in the loop() to check when power breaks down.
  Reads the analogInput of the trigger sginal.
*/
boolean powerCheck(){

  boolean ret = false;
  
  // Read trigger input
  power.triggerValue = analogRead(power.triggerPin);

  // Check up criteria
  if(power.triggerValue >= POWER_UP_THRESHOLD && power.upCount < POWER_UP_COUNT_LIMIT){
     power.upCount ++;
   }

  // If trigger signal high, reset timer and switch
  if (power.triggerValue >= POWER_THRESHOLD) {
    power.timer = millis() + POWER_LATENCY;
    power.fired = false;
  }
   
  //Normal mode starts after 1 second up time
  if (millis() > power.startTime) {
    // Check if trigger is low for latency time
    if (power.triggerValue < POWER_THRESHOLD //
    && millis() > power.timer  //
    && power.fired == false //
    && power.upCount >= POWER_UP_COUNT_LIMIT)
    {
      ret = true;
      power.fired = true;
      power.startTime = millis() + POWER_START_LATENCY_DURATION;
      power.upCount = 0;
    }
  }
  
  return ret;
  
} 
/**
  Initialize power supply module. Call this once in startup().
  triggerPin: GPIO with the voltage trigger signal
*/
void powerSetup(int triggerPin){
  
  // Let the Board time for start up
  power.startTime = millis() + POWER_START_LATENCY_DURATION;
  
  // Init value for a good feeling
  power.triggerValue = 0;
  
  // Reset temp. var
  power.timer = 0;
  
  // Not send yet
  power.fired = false;
  
  // Define GPIO with the trigger signal
  power.triggerPin = triggerPin;
  
  // Reset limit
  power.upCount = 0;
  
  // declare the analog in pin as input (default)
  pinMode(power.triggerPin, INPUT);
  digitalWrite(power.triggerPin, LOW);

}


