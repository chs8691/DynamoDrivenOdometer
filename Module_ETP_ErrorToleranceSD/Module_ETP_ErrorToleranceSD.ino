/*
Lesen: Ermitteln der gespeicherten Entfernung.
Es gibt immer eine Datei, in der der aktulle Wert zu finden ist. Der Datename ist eine Zahl,
sie hat die Endung ".ok" ( Fehlt die Datei beim ersten Ausführen, so wird wird sie später angelegt).
Es wird berücksichtigt, dass der Schreibvorgang unvollständig bleibt (Spannungsquelle
bricht weg). Die Datei kann dann nicht mehr bearbeitet werden, evtl. ist der Inhalt auf nicht
korrekt. Deshalb erfolgt das Schreiben zweistufig: Zuerst wird die Datei geschrieben, dannach
umbenannt, z.b.: "1" -> "1.ok".
In der Initialisirung wird die letzte ok-Datei gesucht (größter Datename, Z.B. "1.ok")
und der Inhalt ausgelesen. Beim Schreiben wird eine neue Datei mit dem Wert beschrieben ("2"),
und anschließend mit ok versehen ("2.ok"). Im letzten Schritt wird die alte Datei gelöscht ("1.ok").
Da der Prozess in jedem dieser Schritte abbrechen kann, gibt es folgende Endstellungen:
(Es werden immer Dateiname in "" Inhalt in () angegeben und Schreibbarkeit angegeben(W beschreibbar)
X nicht beschreibbar, * Beschreibbarkeit pielt keine Rolle)
Ausgangstellung:
1.ok (4711) W - Datei 1.ok hat Inhalt 4711 und ist beschreibbar

Endstellungen:
a) 1.ok (4711) W - Schreibvorgang für neue Datei konnte nicht gestartet werden
b) 1.ok (4711) W , 2 (4712) * - Neue Datei angelegt aber nicht bestätigt
c) 1.ok (4711) W, 2.ok (4712) X - Neue Datei ok, aber nicht schreibbar
d) 1.ok (4711) W, 2.ok (4712) W - Neue Datei sauber erstellt, alte Date nicht gelöscht
e) 2.ok (4712) W - Neue Datei sauber erstellt, alte Date nicht gelöscht

Setup: Leere SD Karte, FAT-formatiert.
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
boolean valueRead;
boolean demoEnd;
long counter;
long counterStart;
boolean error;
/******************************************************
  Setup
/******************************************************/
void setup()
{

  // Open serial communications for debug and pc controlling
  sloSetup(true);

  pinMode(LED, OUTPUT);

  int ret = 0;
  etpSetup();
  valueRead = false;
  demoEnd = false;
  error = false;
  counter = 0;
  counterStart = counter;
  //LOG Serial.println("End of setup()");
  digitalWrite(LED, LOW);
  //--------- Just TEST --------------

}
/******************************************************
  Loop
/******************************************************/
void loop(void) {

  if (!error && !demoEnd) {
    // Number of retries to much: Cancel reading and
    // switch to error mode
    if (etpCheckReadRetryOverflow() > 0) {
      error = true;
      sloLogB("etpCheckReadRetryOverflow: error. Set error", error);
      return;
    }

    // Card reading not finished
    if (!valueRead)
      // Next try to read from cord
      if (etpReadValue()) {
        sloLogL("Reading succesful, found value", etpGetDistance());
        valueRead = true;
        //Done. Add stored value to actual counter.
        counter += etpGetDistance();
        counterStart = counter;
      }
      else
        sloLogS("Reading not successful");

    // Simulate reed
    counter++;


    //Simulate End of session / power down
    if (counter > counterStart && valueRead) {
      sloLogL("Calling etpWrite with", counter );
      etpWrite(counter);
      demoEnd = true;
    }
  }
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
  if (slo.serialSwitch)
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

  sloLogL("etpWrite() with value", value);


  goOn = true;
  fNr = 0;

  //Search for free filename. Remove existing but not actual files.
  while (goOn) {

    //avoid overflow
    if (fNr >= ETP_MAX_FILE_NR)
    {
      sloLogS("Cancel with ETP_ERROR_MAX_FILE_NR_REACHED");
      return ETP_ERROR_MAX_FILE_NR_REACHED;
    }

    // File to search("1", "2", "3" etc.)
    fNr++;

    //Skip file of etp.oldDistance
    if (fNr == etp.lastFilenumber) {
      fNr++;
      sloLogL("Skip old file nr", fNr);
    }

    sloLogL("Search file nr", fNr);

    //First try: nok-File
    fName = String(fNr);
    fName.toCharArray(cfName, 11);

    if (etpPrepareFileForCreating(cfName))
    {
      sloLogS("free file place " + fName);
      //Let's try ok-File
      fName = String(fNr) + ".ok";
      fName.toCharArray(cfName, 11);
      if (etpPrepareFileForCreating(cfName)) {
        sloLogS("free file place, end while: " + fName);
        goOn = false;
      } else
        sloLogS("file exists: " + fName);
    } else
      sloLogS("file exists: " + fName);

  }

  //Now we have found a not existing filename
  myFile = SD.open(cfName, FILE_WRITE);
  if (myFile) {
    sloLogS("File opend for writing: " + fName);
    sloLogS("Write value: " + strValue);
    bytes = myFile.println(strValue);
    myFile.close();
    sloLogL("Wrote byte number", bytes);
  }
  else {
    sloLogS("Could not open file for writing. Returning ETP_ERROR_WRITE: " + fName);
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

  sloLogS("etpReadValue()");
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

  sloLogL("Nr of retries", etp.nrReadRetries);

  //Card initializtion must be done
  if (!etp.sdBegin) {
    if (!SD.begin(ETP_GPIO_CHIP_SELECT)) {
      sloLogS("SD.begin() false, leave etpReadValue()");
      return false;
    }
    else {
      etp.sdBegin = true;
      sloLogS("SD.begin() true");
    }
  }

  ///////////////////////////////////////////////////////////////
  // Loop directory and try to find last ok file and read its value.
  // Results are: stored value and file name
  // for next storing.
  ///////////////////////////////////////////////////////////////
  sloLogS("Entering while");
  while (goOn) {

    // File to search("1", "2", "3" etc.)
    fNr++;
    //Filename to search for, "1.ok", "2.ok" etc.
    fName = String(fNr) + ".ok";
    fName.toCharArray(cfName, 11);
    // sloLogS("Searching for file: " + fName);

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
          // sloLogL("Set etp.oldDistance: ", value1);
          etp.oldDistance = value1;
        }
      }
      f.close();
    }

    // File doesn't exist
    else  {
      //   sloLogL("File doesn't exist, finish searching. fNr", fNr);
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

  sloLogL("Set etp.LastFilenumber", fNr);
  sloLogL("Set etp.oldDistance", value1);

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

