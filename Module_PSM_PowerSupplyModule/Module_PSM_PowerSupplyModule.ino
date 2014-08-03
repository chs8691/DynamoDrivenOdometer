/*
 Secondary voltage signal on analog input for controlling power fade out

 There is a bycicle dynamo who powers the B&M LED front light- Lumotec IQ. The B&M has
 two outputs: One is the LED power, a DC voltage limited to ~ 3.5 V and with
 a big capicator for stand light support. This power will be used for powering
 the RFDuino. The second output of the B&M is for the taillight, it is the AC voltage
 of the dynamo and can be >> than 3.5 V. For the RFDuino this signal will be used for
 triggering the end of power. Be careful, the maximum input voltage of RFDuino is 3.6 V AC.
 It has to be limited and rectified.

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
  powerSetup(TRIGGER_PIN);
}

void loop() {

  if (powerCheck()) {
    digitalWrite(ledPin, HIGH);
    timer = millis() + 1000;
  }

  if (millis() > timer)
    digitalWrite(ledPin, LOW);

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

  // Local variable for powerCheck()
  boolean ret;

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
boolean powerCheck() {
//  Serial.println("powerCheck()");

  power.ret = false;

  // Read trigger input
  power.triggerValue = analogRead(power.triggerPin);
// Serial.println(analogRead(1));
  
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
         power.ret = true;
         power.fired = true;
         power.startTime = millis() + POWER_START_LATENCY_DURATION;
         power.upCount = 0;
       }
     }
  
  return power.ret;

}
/**
  Initialize power supply module. Call this once in startup().
  triggerPin: GPIO with the voltage trigger signal
*/
void powerSetup(int triggerPin) {

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
//  pinMode(power.triggerPin, INPUT);
//  digitalWrite(power.triggerPin, LOW);

}


