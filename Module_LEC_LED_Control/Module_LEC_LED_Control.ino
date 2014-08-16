/*
  Module LED Control
  The demo program show the usage.

  Proparation:
  - LED on GPIO 1
 */

//GPIOs
const int LED = 2;

boolean demoStarted;
long endMs;

// the setup routine runs once when you press reset:
void setup() {
  Serial.begin(9600);

  demoStarted = false;
  lecSetup(LED);
  Serial.println("LEC Demo");
}

// the loop routine runs over and over again forever:
void loop() {

  // Toggle demo: let it toggle
  /*
      lecToggleLed();
      delay(250);
  */

  // Flash demo: Flash the LED for a defined length
//    if (!demoStarted) {
//      lecFlash();
//      demoStarted = true;
//    }
    

  // Blink demo: Let LED blink 5 times

 // if (!demoStarted) {
//    lecBlink();
//    demoStarted = true;
 // }

  // Interval demo: Let LED switch peridically
  
   if (!demoStarted) {
     lecIntervalStart();
     demoStarted = true;
     endMs = millis() + 5000;
   }
   if (millis() > endMs)
     lecIntervalStop();
  

  lecControl();

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
