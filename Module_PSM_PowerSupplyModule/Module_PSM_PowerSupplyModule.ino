/*

  Demo setting for module PSM. USB board may not bo connected (TX = GPIO1).

 The circuit:
 - LED an GPIO2
 - Analog signal < 3 V on GPIO1

 */

const int TRIGGER_PIN = 1;    // select the input pin for the potentiometer
const int SIGNAL_LED = 3;    // Dummy LED for doing something

int ledPin = 2;      // select the pin for the LED

long nextSerial = 0L;

long timer = 0L; // LED on timer
long signalTimer = 0L;
long signalEnd =  0L;
const long SIGNAL_FREQ = 250L;
const long SIGNAL_DURATION = 100L;

boolean on;

void setup() {
  // declare the ledPin as an OUTPUT:
  pinMode(ledPin, OUTPUT);
  pinMode(SIGNAL_LED, OUTPUT);

  // Open serial communications for debug and pc controlling
  //  Serial.begin(9600);

  digitalWrite(ledPin, LOW);
  //  Serial.println("Hallo Cord!!!");

  on = false;

  timer = millis() + SIGNAL_FREQ;


  //Initialize Power Supply Module
  psmSetup(TRIGGER_PIN);

  //  digitalWrite(ledPin, HIGH);
  //  delay(250);
  //  digitalWrite(ledPin, LOW);

}

void loop() {

  boolean check = psmCheck();
  
  if(millis() > timer){
     if(check){
       digitalWrite(SIGNAL_LED, HIGH);
       on = true;
       signalEnd = millis() + SIGNAL_DURATION;
     }
     timer = millis() + SIGNAL_FREQ;  
  }
  
  
  if(on && millis() > signalEnd){
       digitalWrite(SIGNAL_LED, LOW);
       on = false;
  }
  
  if (check)
    digitalWrite(ledPin, HIGH);
  else
    digitalWrite(ledPin, LOW);

}
/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
/*                      Power Supply Module                           */
/*                                                                    */
/*  Version 2.0, 21.10.2014                                           */
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

// Maximum long value
const long MAX_LONG = 2147483647L;

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
const int PSM_THRESHOLD = 500;

// Latency time in milliseconds before powerCheck() will fire for the first
// time or for the next time after a powerCheck() returned true.
// Increase the value if very short distances should not be stored.
const int PSM_START_LATENCY_DURATION = 2000;

// Number of high voltage trigger signals that have to be reached before
// powerCheck() can fire.
// Increase the value, if the system has to be up for a longer time before
// next fireing.
const int PSM_UP_COUNT_LIMIT = 10;

// The trigger voltage must be above this value to increment the upCount.
// Decrease this, if the upCount doesn't reach the UP_COUNT_MIN.
// Increase this, if there is not enough voltage when the powerCheck() fires.
const int PSM_UP_THRESHOLD = PSM_THRESHOLD;


/**
  Returns true, if trigger signal is a) long enough up and b) still there.
  While a latency time after a new started up period, false will be returned.
  Use this in the loop() to check if power is in stable a mode.
  Reads the analogInput of the trigger sginal.
*/
boolean psmCheck() {
  //  Serial.println("psmCheck()");

  psm.ret = false;

  // Read trigger input
  psm.triggerValue = analogRead(psm.triggerPin);
  // Serial.println(analogRead(1));

  ///// NEW

  // If trigger signal high, reset timer and switch
  if (psm.triggerValue >= PSM_THRESHOLD) {
    //Very first trigger signal: Restart start latency time
    if (psm.startTime == MAX_LONG)
      psm.startTime = millis() + PSM_START_LATENCY_DURATION;

    //Set time frame for next high value will be expected in
    psm.timer = millis() + PSM_LATENCY;

  }

  // Check if trigger is low for latency time
  if (millis() < psm.timer) {
    //Normal mode starts after 1 second up time
    if (millis() > psm.startTime)
      psm.ret = true;
  }
  // Trigger failed
  else {
    psm.startTime = MAX_LONG;
  }

  return psm.ret;

  /////////// OLD /////////////////////////
  /*   // Check up criteria
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
  */
}

/**
  Can be used to reset after psmCheck() has fired.
*/
/*
void psmReset() {
  psmSetup(psm.triggerPin);
}
*/
/**
  Initialize power supply module. Must be called once in startup().
  triggerPin: GPIO with the voltage trigger signal
*/
void psmSetup(int triggerPin) {

  // Initialize start time with 'infinite'
  psm.startTime = MAX_LONG;

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


