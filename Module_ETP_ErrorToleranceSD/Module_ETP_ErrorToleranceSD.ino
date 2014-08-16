
/*
Lesen: Ermitteln der gespeicherten Entfernung.
Es gibt immer zwei gültige Dateien (am Anfang zu erstellen).
Es können weitere Dateien bestehen, die nicht geöffnet werden können
(korrupte Dateien, die beim Schreiben erzeugt wurden).
Das Programm geht alle Dateien der Reihe nach durch, bis er zwei Datei
zum Lesen öffnen konnte.
Die Datei ist gültig, wenn sie geöffnet werden kann. Die Datei
mit dem größeren Wert ist der letzte Entfernungsstand.Diese Datei
wird nicht verändert. Die andere Datei wird nach dem Auslesen gelöscht.

Schreiben: Speichern des Wertes in einer zweiten Datei.
Es werden die Anzahl der Dateien ermittelt, der Dateiname der neuen Datei
ist 'Dateianzahl + 1'. Die neue Datei wird mit dem neuen Entfernungswert
gespeichert.
Kann die Datei nicht gespeichert werden, geht der neue Entferungswert verloren.

 */

// include the SD library:
#include <SPI.h>
#include <SD.h>


////////////////////////////////////////////////////////
// GPIO 0 ..6 definition
////////////////////////////////////////////////////////
// change this to match your SD shield or module;
const int LED = 2;

////////////////////////////////////////////////////////
// Variable declarations
////////////////////////////////////////////////////////
long counter;
boolean done;

/******************************************************
  Setup
/******************************************************/
void setup()
{

  // Open serial communications for debug and pc controlling
  //Serial.begin(9600);

  pinMode(LED, OUTPUT);

  int ret = 0;
  ret = etpSetup();
  if (ret > 0) {
    //LOG Serial.print(ret);
    //LOG Serial.println(": Error in etpSetup().Cancel program.");
  }
  counter = etpGetOldDistance();
  done = false;
  //LOG Serial.println("End of setup()");
   digitalWrite(LED, LOW);
  //--------- Just TEST --------------

}
/******************************************************
  Loop
/******************************************************/
void loop(void) {

  //only one time
  if (!done) {
    //Test: Write every distance change to file
 
    for (int i = 1; i <= counter; i++) {
      digitalWrite(LED, HIGH);
      delay(100);
      digitalWrite(LED, LOW);
      delay(100);
    }

    counter++;
    etpWrite(counter);
    done = true;
  }
  /*
    counter++;
    //LOG Serial.print("Act distance is=");
    //LOG Serial.println(counter);

    etpWrite(counter);

    delay(1);
  */
}

/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
/*                                                                    */
/*        Module ETP - Error Tolerance Persisting to SD Card          */
/*                                                                    */
/*  Version 1.0, 11.08.2014                                           */
/*  There are two files: one with the actual and one with the         */
/*  previous value. Only the new one will be touched. If an error     */   
/*  occurs, there is always the oder one as fallback.                 */  
/**********************************************************************/
/**********************************************************************/
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

String etpErrorToString(int error){
  if(error == ETP_ERROR_CARD)
    return "ETP_ERROR_CARD";

  if(error == ETP_ERROR_WRITE)
    return "ETP_ERROR_WRITE";
  
  return "ERROR_UNKNOWN";
    
}


/**
  Writes actual distance to file. Return 0 if successfull or otherwise
  error code.
*/
int etpWrite(long value) {
  File myFile;
  int ret = 0;
  String strValue = String(value);
  byte bytes = 0;

  //LOG Serial.print("etpWrite: ");
  //LOG Serial.println(value);

  //LOG Serial.println("Value as String: '" + strValue + "'");

  //LOG Serial.print("Filename = '");
  //LOG Serial.print(etp.cNextFilename);
  //LOG Serial.println("'");

  //Remove file with old content

  if (SD.exists(etp.cNextFilename))
  {
    //LOG Serial.print("File exists, will remove it... ");
    SD.remove(etp.cNextFilename);
    if (SD.exists(etp.cNextFilename)) {
      //LOG Serial.println(" Failed! Returning ETP_ERROR_WRITE");
      return ETP_ERROR_WRITE;
    } else
      //LOG Serial.println("Done!");
      ;
  } else {
    //LOG Serial.println("File doesn't exist, nothing to delete. ");
    ;
  }

  myFile = SD.open(etp.cNextFilename, FILE_WRITE);
  if (myFile) {
    //LOG Serial.print("No. of written bytes = " );
    //Cursor to begin
    //myFile.seek(0);
   bytes = myFile.println(strValue);
    myFile.close();
    //LOG Serial.println(bytes);
  }
  else {
    //LOG Serial.println("Could not open file for writing. Returning ETP_ERROR_WRITE");
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
  //LOG Serial.println("etpSetup()");
  int ret = 0;
  // Find two valid files
  if (!SD.begin(ETP_GPIO_CHIP_SELECT)) {
    //LOG Serial.println("SD.begin() error. Returning ETP_ERROR_CARD");
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
  //LOG Serial.println("Entering while ");
  while (goOn) {

    // File to search("1", "2", "3" etc.)
    fNr++;
    fName = String(fNr);
    fName.toCharArray(cfName, 8);
    //LOG Serial.print("Searching for file = '" + fName + "'. As character array:'");
    //LOG Serial.print(cfName );
    //LOG Serial.println("'");

    // File exist
    if (SD.exists(cfName)) {
      //LOG Serial.println("File Found. Try to open it.");

      // Try to open file
      fName.toCharArray(cfName, 8);
      f = SD.open(cfName);

      // File ok, can be opend / is valid
      if (f) {
        //LOG Serial.println("Opened.");
        // We found the first value
        if (value1 == ETP_NULL) {
          //LOG Serial.println("Take it for value1. Read from file...");
          value1 = etpReadFile(f);
          //LOG Serial.println(value1);
          if (value1 != ETP_NULL) {
            fName1 = fName;
          }

        }
        // We found the second value
        else {
          //LOG Serial.println("Take it for value2. Read from file...");
          value2 = etpReadFile(f);
          //LOG Serial.println(value2);
          if (value2 != ETP_NULL) {
            fName2 = fName;
            // End of while
            goOn = false;
          }
        }
        f.close();
      }
    } else  {
      //LOG Serial.println("File doesn't exist. Exit while.");
      goOn = false;
    }
  }

  //LOG Serial.println("Determine the valid distance value");
  //--- Determine the valid distance value ---
  // At least one file was found
  if (value1 != ETP_NULL || value2 != ETP_NULL) {
    etp.oldDistance = max(value1, value2);
    //LOG Serial.print("Set etp.oldDistance = ");
    //LOG Serial.println(etp.oldDistance);

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
    //LOG Serial.println("Set etp.oldDistance = 0");
    etp.oldDistance = 0;
    etp.nextFilename = fName;
  }
  //LOG Serial.println("Set etp.nextFilename = " + etp.nextFilename);

  //SD needs filename as char array not as String
  if (etp.nextFilename.length() > 0 ) {
    etp.nextFilename.toCharArray(etp.cNextFilename, 8);
    //LOG Serial.print("Set etp.cNextFilename = ");
    //LOG Serial.println(etp.cNextFilename);
  }


  //LOG Serial.println("End of etpSetup()");

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
long etpGetOldDistance() {
  return etp.oldDistance;
}

