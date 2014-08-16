void setup() {
  // put your setup code here, to run once:
   sloSetup(true);
}

void loop() {
  // put your main code here, to run repeatedly:
   sloLogL("Up since", millis());
   delay(1000);
}

/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
/*                                                                    */
/*        Module SLO - A simple logger                                */
/*                                                                    */
/*  Version 1.0, 11.08.2014                                           */
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
  if(slo.serialSwitch)
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
