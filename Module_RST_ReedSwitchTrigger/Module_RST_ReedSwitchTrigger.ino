/*
  Reed
  Controls the LED toggled by the reed switch.

  Proparation:
  - Reed switch on 3V+ and GPIO 0, Pull down resistor with 320 OHM
  - LED on GPIO 1
 */

//GPIOs
const int LED = 2;
const int REED = 0;


// the setup routine runs once when you press reset:
void setup() {

  //Serial.begin(9600);
  rstSetup(REED);
  lecSetup(LED);

}

// the loop routine runs over and over again forever:
void loop() {

  if (rstRead())
    lecSwitchLed();
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
/**********************************************************************/
typedef struct {
  int ledStatus;
  int gpio;
} Lec;
Lec lec;

void lecSwitchLed() {
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
  pinMode(lec.gpio, OUTPUT);
}
