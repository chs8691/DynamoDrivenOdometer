void setup() {
  // put your setup code here, to run once:
   sloSetup(true);
      sloLogUL("Unsigned long = ", 4294967295 );
      sloLogL("Unsigned long = ", 4294967295 ); //ERROR
      sloLogUI("Unsigned int = ", 4294967295 );
      sloLogI("Int = ", 4294967295 );
      sloLogUI("Unsigned Int = ", -2147483648 );
      sloLogI("Int = ", -2147483648 );
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

// For number types unsigned long and unsigned int
void sloLogUL(String text, unsigned long value) {
  if (slo.serialSwitch) {
    Serial.print(text + ": ");
    Serial.println(value);
  }
}

// For number types long and int
void sloLogL(String text, long value) {
  if (slo.serialSwitch) {
    Serial.print(text + ": ");
    Serial.println(value);
  }
}

//For boolean
void sloLogB(String text, boolean value) {
  if (slo.serialSwitch) {
    Serial.print(text + ": ");
    Serial.println(value);
  }
}
