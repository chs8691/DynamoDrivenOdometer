/*

 The circuit:
 - LED an GPIO2
 - Analog signal < 3 V on GPIO1

 */

const int TRIGGER_PIN = 1;    // select the input pin for the potentiometer

int ledPin = 2;      // select the pin for the LED

long nextSerial = 0;

long timer = 0; // LED on timer

void setup() {
  // declare the ledPin as an OUTPUT:
  pinMode(ledPin, OUTPUT);

  // Open serial communications for debug and pc controlling
//  Serial.begin(9600);

  digitalWrite(ledPin, LOW);
//  Serial.println("Hallo Cord!!!");

  timer = 0;

  //Initialize Power Supply Module
  psmSetup(TRIGGER_PIN);
}

void loop() {

  if (psmCheck()) {
    digitalWrite(ledPin, HIGH);
    timer = millis() + 1000;
  }

  if (millis() > timer)
    digitalWrite(ledPin, LOW);

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
     if(psm.triggerValue >= PSM_UP_THRESHOLD && psm.upCount < PSM_UP_COUNT_LIMIT){
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


