/*
  Module MEM with a demo sketch. The demo sketch reads and writes a value the the I2C-EEPROM.
  Setup for the demo:
  - A green LED on GPIO 2
  - A red LED in GPIO 3
  - 24AA256 EEPORM
  - I2C in standard pins (SCL on GPIO 5 and SDA on GPIO 6)
*/
// include the wire-library:
#include <Wire.h>

////////////////////////////////////////////////////////
// GPIO 0 ..6 definition
////////////////////////////////////////////////////////
// change this to match your SD shield or module;
const int LED_GREEN = 2;
const int LED_RED = 3;

////////////////////////////////////////////////////////
// Variable declarations
////////////////////////////////////////////////////////
boolean indexChanged;
boolean writing;
boolean demoEnd;
unsigned long counter;
unsigned long value;
unsigned long timer;
unsigned long timer2;
unsigned long timer3;
boolean error;
/******************************************************
  Setup
/******************************************************/
void setup()
{
  sloSetup(true);
  error = false;

  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_RED, LOW);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_GREEN, LOW);

  timer = millis() + 5000;
  timer2 = 4294967295;

  memSetup();
  sloLogUL("memGetCounter() = ", memGetCounter());

  sloLogUL("mem.RoomAddress=", memGetRoomAddress());
  //      readEEPROMOLD(24);
  //  readEEPROM(6);
  //clearEEPROM(0,4);

  readEEPROM(0, 10);
  Serial.println("--");

  indexChanged = false;
  writing = false;
  memTick();

}
/******************************************************
  Loop
/******************************************************/
void loop(void) {
  // delay(1000000);
  /*
    //Simulate reed tick
    if (millis() > timer) {
      memTick();
      timer = millis() + 500;
      timer2 = millis() + 100;

      // sloLogUL("memGetCounter() = ", memGetCounter());
    }
    if (millis() > timer2) {
  //    readEEPROM(6);
      timer2 = 4294967295;
    }
  */
  if (memGetCounter() < 1000100L) {
    switch (memControl()) {
      case 1:
        digitalWrite(LED_GREEN, HIGH);
        writing = true;
        break;
      case 2:
        digitalWrite(LED_RED, HIGH);
        indexChanged = true;
        break;
      default:
        digitalWrite(LED_RED, LOW);
        digitalWrite(LED_GREEN, LOW);
        if (writing) {
          writing = false;
          memTick();
          if (!(memGetCounter() % 10000L)) {
            Serial.print(millis());
            Serial.print("ms : ");
            Serial.println(memGetCounter());
          }
        }
        if (indexChanged) {
          //    readEEPROM(0, 20);
          indexChanged = false;
        }
        break;

    }
  }
  else {
    if (!error) {
      sloLogUL("END counter", memGetCounter());
      error = true;
      readEEPROM(0, 20);
    }
  }
}

//Initialize the first number bytes with the unsigned long max.
void clearEEPROM(unsigned int offset, int bank) {
  unsigned int address;
  unsigned long counter = 4294967295;

  for (int i = 0; i < bank * 4; i++) {
    address = offset + (64 * i);

    Wire.beginTransmission(0x50);
    Wire.write((address >> 8) & 0xFF); //MSB of the address
    Wire.write((address & 0xFF)); //LSB of the address
    Wire.write((byte) (counter >> 24));
    Wire.write((byte) (counter >> 16));
    Wire.write((byte) (counter >> 8));
    Wire.write((byte) counter);
    Wire.endTransmission();
    delay(6);
  }

}
void readEEPROM(unsigned int offset, int bank) {
  unsigned int address;
  unsigned long rdata = 0;

  Serial.println("ReadEEPROM");
  for (int k = 0; k < bank; k++) {
    Serial.print(k);
    Serial.print("   ");
    for (int j = 0; j < 4; j++) {
      address = offset + ((k * 64 * 4) + (j * 64));
      byte data[4];
      byte i = 0;

      Wire.beginTransmission(0x50);
      Wire.write((address >> 8) & 0xFF); //MSB of the address
      Wire.write((address & 0xFF)); //LSB of the address
      Wire.endTransmission();     // stop transmitting

      // delay(5);
      //  Wire.beginTransmission(MEM_SLAVE_ADDRESS);
      Wire.requestFrom(0x50, 4);

      while (Wire.available() > 0) data[i++] = Wire.read();

      rdata = ((long)data[0] << 24) + ((long)data[1] << 16) + ((long)data[2] << 8) + data[3];

      Serial.print(rdata);
      Serial.print(" ");
    }
    Serial.println("");
  }
}

// number: number of banks
void readEEPROMOLD(int number) {
  unsigned int address;
  for (int i = 0; i <= number; i++) {
    address = 64 * i;
    unsigned long rdata = 0;
    byte data[4];
    byte i = 0;

    Wire.beginTransmission(0x50);
    Wire.write((address >> 8) & 0xFF); //MSB of the address
    Wire.write((address & 0xFF)); //LSB of the address
    Wire.endTransmission();     // stop transmitting

    // delay(5);
    //  Wire.beginTransmission(MEM_SLAVE_ADDRESS);
    Wire.requestFrom(0x50, 4);

    while (Wire.available() > 0) data[i++] = Wire.read();

    rdata = ((long)data[0] << 24) + ((long)data[1] << 16) + ((long)data[2] << 8) + data[3];


    Serial.print(address);

    Serial.print(" ");
    Serial.println(rdata);

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
// For number types unsigned long and unsigned int
void sloLogUL(String text, unsigned long value) {
  if (slo.serialSwitch) {
    Serial.print(text + ": ");
    Serial.println(value);
  }
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
/*      Module MEM - Error Tolerance Persisting for 24AA256           */
/*                                                                    */
/*  Version 1.0, 22.10.2014                                           */
/*  Write an read one unsigned integer value to the external memory.  */
/*  Only increasing values are supported (new value to be written     */
/*  must be greate than the stored one.                               */
/*  The storage is protected against broken power supply while        */
/*  writing. There are 2 * 2 pages in use to store the value. One     */
/*  page pair holds the acutal value and the other page pair holds    */
/*  the previous one. While reading, both values of a pair must       */
/*  be equal to be valid. The pair with the greater value will be     */
/*  returned. While writing, the new value overwrites the older pair  */
/*  (the pair with the lower value). The value will be stored on both */
/*  value places of an pair.                                          */
/*  This mechanism protects against corrupt writing. A value must be  */
/*  written twice to be valid. Only if the second writing was done,   */
/*  the value is 'valid'. Not valid pairs will be handled as          */
/*  value == 0.                                                       */
/*  Medule restrictions:                                              */
/*   - Only for 24xx256 EEPROM from Microchip                         */
/*   - Only one chip supported, wired to A2 = A1 = A0 = GND           */
/*   - Only page writing supported                                    */
/*   - Only values from 0 .. UNSIGNED_LONG_MAX - 1 are supported.     */
/*   - memDo() must be called periodically in loop():                 */
/*      There is a latency time of 6 ms between every transaction,    */
/*      so transactions will be buffered and handled in cycles. No    */
/*      delay is used.                                                */
/*   - Check with memCheck() before using memWrite() memRead()        */

/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
// Values of a Bank

typedef byte MemProgram;
typedef byte MemTask;

typedef struct {
  unsigned long a1;
  unsigned long a2;
  unsigned long b1;
  unsigned long b2;
} MemBank;

// Valid value and its source
struct MemValidValue {
  unsigned long value;
  byte pairNr;   //0: Pair 0, 1: Pair 1, 2: No pair
};

typedef struct {

  // actual value of the counter
  unsigned long counter;

  // Last read value from memory
  unsigned long oldValue;

  // Absolute address of the actual data value (Page 1 of the pair)
  // Read from index in the room. Address must be inside the room.
  //(Must be a 2 Byte value with integer multiple of MEM_PAGE_SIZE)
  struct MemValidValue index;

  // Stored data value
  struct MemValidValue data;

  // to make shure, that te copy has the same value
  unsigned long writeCounter;

  // Index for writing data. Will be set in a program.
  unsigned long writeIndexValue;

  //PairNr to write to
  byte writePairNr;

  // Verifying: data value after writing. Temp. used in a program.
  struct MemValidValue writtenValue;

  // Delay after page write must be 6 ms and counter will be
  // updated ever second.
  unsigned long timer;

  //Next time in milliseconds to check counter
  unsigned long timerCounter;

  // Program to do
  MemProgram program;

  // Upcoming task
  MemTask task;

  //FALSE, if there is a task to do and the task is not done. Otherwise TRUE;
  boolean taskDone;

  //Address to to room (first byte)
  unsigned int roomAddress;

} Mem;
Mem mem;


// Should be an int for Wire.beginTransmission
const int MEM_SLAVE_ADDRESS = 0x50; //1010000 Address of the chip for writing

// Page size in Byte: 64 Bytes per page
const unsigned int MEM_PAGE_SIZE = 64;

// A pair has two pages
const unsigned int MEM_PAIR_SIZE = 2 * MEM_PAGE_SIZE; // 128

// A bank has two pairs
const unsigned int MEM_BANK_SIZE = 2 * MEM_PAIR_SIZE; // 256

// Number of data banks
const unsigned int MEM_MAX_INDEX_BANK_NR = 973;

// Size of unused part in banks
// Set to 0 for a production EEPROM. Set to 1..99 for developing.
const unsigned int MEM_UNUSED_BANKS = 0;

// Size of fixed part in pages for configuration values (further release)
// Set to 100 for further releases
const unsigned int MEM_CONFIGURATION_BANKS = 0;

// Offset for first page of room part (in pages)
const unsigned int MEM_ROOM_BANK_OFFSET = MEM_UNUSED_BANKS + MEM_CONFIGURATION_BANKS;

// A data bank may only be used for 1 millon writes per pair. To be on the safe side,
// a bank should be used only 1 million times.
const unsigned long MEM_MAX_BANK_WRITINGS = 1000000L;

// Needed for filter this values (EEPROM's bytes are initialized with 0xFF)
const unsigned long MEM_MAX_UNSIGNED_LONG = 4294967295;

// Latency time between two writes in milliseconds
const unsigned long MEM_TIMER_INTERVALL_MS = 0;

// pairNr can be 0 or 1. If no pair found, it will be set to NULL
const byte MEM_PAIR_NULL = 255;

//--- Programs: A program runs a set of task for writing data to the memory
// Dummy program
const MemProgram MEM_PROGRAM_NULL = 0;

// Normal data writing of the actual counter value
const MemProgram MEM_PROGRAM_WRITE_DATA = 1;

// After max. write cycles into a page, we have to increment the bank
const MemProgram MEM_PROGRAM_SWITCH_BANK = 2;

//--- Tasks: A task is a single step in a program
// Dummy-task
const MemTask MEM_TASK_NULL = 0;

// Write data value
const MemTask MEM_TASK_WRITE_DATA = 1;

// Wait after write (EEPROM needs time for it)
const MemTask MEM_TASK_WRITE_DATA_WAIT = 2;

// Write copy of the data value
const MemTask MEM_TASK_COPY_DATA = 3;

// Wait after write (EEPROM needs time for it)
const MemTask MEM_TASK_COPY_DATA_WAIT = 4;

// Read new written data value and check them
const MemTask MEM_TASK_VERIFY_WRITTEN_DATA = 5;

// Write data value
const MemTask MEM_TASK_WRITE_INDEX = 6;

// Wait after write (EEPROM needs time for it)
const MemTask MEM_TASK_WRITE_INDEX_WAIT = 7;

// Write copy of the data value
const MemTask MEM_TASK_COPY_INDEX = 8;

// Wait after write (EEPROM needs time for it)
const MemTask MEM_TASK_COPY_INDEX_WAIT = 9;

// Read new written data value and check them
const MemTask MEM_TASK_VERIFY_WRITTEN_INDEX = 10;

/***********************************************************************
     Returns valid index. Internal use only.
************************************************************************/
struct MemValidValue memGetIndex() {

  // Read memory
  MemValidValue index = memReadBank(mem.roomAddress);

  // in error case, init value
  if (index.value < 1 || index.value > MEM_MAX_INDEX_BANK_NR) {
    index.value = 1;
    index.pairNr = MEM_PAIR_NULL; //Indicates that no valid pair was found
  }

  return index;
}

/***********************************************************************
     Returns valid data. Internal use only.
     Precondition: memGetIndex() called before.
************************************************************************/
struct MemValidValue memGetData(unsigned long index) {

  // Read memory
  MemValidValue data = memReadBank(mem.roomAddress + index * MEM_BANK_SIZE);

  return data;

}

/***********************************************************************
     Returns value from memory or, if not found value == 0.
     Internal use only.
************************************************************************/
struct MemValidValue memReadBank(unsigned int address) {

  struct MemValidValue validValue;
  MemBank bank;
  boolean validA = false;
  boolean validB = false;

  // Read complete bank
  bank.a1 = memReadSingle(address);
  bank.a2 = memReadSingle(address + MEM_PAGE_SIZE);
  bank.b1 = memReadSingle(address + 2 * MEM_PAGE_SIZE);
  bank.b2 = memReadSingle(address + 3 * MEM_PAGE_SIZE);

  //check for valid values
  if (bank.a1 == bank.a2)
    validA = true;

  if (bank.b1 == bank.b2)
    validB = true;

  if (validA && validB) {
    if (bank.a1 >= bank.b1)
      validB = false;
    else
      validA = false;
  }

  if (validA) {
    validValue.value = bank.a1;
    validValue.pairNr = 0;
  }
  else if (validB) {
    validValue.value = bank.b1;
    validValue.pairNr = 1;
  }
  else {
    validValue.value = 0;
    validValue.pairNr = MEM_PAIR_NULL;  //Mark as 'Not valid'
  }

  return validValue;
}

/***********************************************************************
     Returns a specific value as unsigned long from a bank.
     MAX_UNSIGNED_LONG value will be returend as 0.
     Internal use only.
     Precondition: EEPROM not busy.
************************************************************************/
unsigned long memReadSingle(unsigned int address) {

  unsigned long rdata = 0;
  byte data[4];
  byte i = 0;

  Wire.beginTransmission(MEM_SLAVE_ADDRESS);
  Wire.write((address >> 8) & 0xFF); //MSB of the address
  Wire.write((address & 0xFF)); //LSB of the address
  Wire.endTransmission();     // stop transmitting

  // delay(5);
  //  Wire.beginTransmission(MEM_SLAVE_ADDRESS);
  Wire.requestFrom(MEM_SLAVE_ADDRESS, 4);

  while (Wire.available() > 0) data[i++] = Wire.read();

  rdata = ((long)data[0] << 24) + ((long)data[1] << 16) + ((long)data[2] << 8) + data[3];

  //Bytes in EEPROM is initialized with 0xFF, treat them as '0'
  if (rdata == MEM_MAX_UNSIGNED_LONG)
    rdata = 0L;

  return rdata;
}

/***********************************************************************
     Impulse for counter. Call this in the loop for increasing
     the counter value.
************************************************************************/
void memTick() {

  // increase counter
  mem.counter++;

}

/***********************************************************************
     Returns acual counter value.
************************************************************************/
unsigned long memGetCounter() {

  return mem.counter;

}



/***********************************************************************
     Initializes the module. Call this once in the setup() as first
     method of the module.
     Returns TRUE, if memory was successful initialized, otherwise false.
************************************************************************/
boolean memSetup() {

  // Just initialize the vars
  mem.timer = 0;
  mem.timerCounter = MEM_TIMER_INTERVALL_MS;
  mem.program = MEM_PROGRAM_NULL;
  mem.task = MEM_TASK_NULL;
  mem.taskDone = true;
  mem.counter = 0;
  mem.writeCounter = 0;
  mem.writeIndexValue = 0L;
  mem.writePairNr = MEM_PAIR_NULL;
  mem.writtenValue = {0, 0};

  // join i2c bus
  Wire.begin();

  // address to the room
  mem.roomAddress = MEM_ROOM_BANK_OFFSET * MEM_BANK_SIZE;

  // Index adress is at the first place inside a room
  mem.index = memGetIndex();


  // Data bank address need an offset 1 because of the index bank
  mem.data = memGetData(mem.index.value);

  // Initialize counter with stored value
  mem.counter = mem.data.value;

  // Inequality is a trigger for writing
  mem.writeCounter = mem.counter;

  sloLogUL("mem.index.value", mem.index.value);
  sloLogUL("mem.index.pairNr", mem.index.pairNr);
  sloLogUL("mem.data.value", mem.data.value);
  sloLogUL("mem.data.pairNr", mem.data.pairNr);

}
/***********************************************************************
     Writes counter data to the memory pair. If asCopy==TRUE, the copy
     will be written (Page 2), otherwise the data will be written to
     Page 1. To complete writing, the method has to be called twice.
     Precondition: EEPROM not busy.
************************************************************************/
void memWrite(boolean asCopy, byte pairNr, unsigned long index, //
              unsigned long value) {

  unsigned int address;

  address = mem.roomAddress // Room
            //            + (mem.index.value * MEM_BANK_SIZE) // Bank
            + (index * MEM_BANK_SIZE) // Bank
            +  (pairNr * MEM_PAIR_SIZE); //Pair

  //Decide in which page inside the pair to write
  if (asCopy) {
    address += MEM_PAGE_SIZE;
  }

  //sloLogUL("Adress=", address);
  //sloLogUL("Counter=", mem.writeCounter);

  Wire.beginTransmission(MEM_SLAVE_ADDRESS);
  Wire.write((address >> 8) & 0xFF); //MSB of the address
  Wire.write((address & 0xFF)); //LSB of the address
  Wire.write((byte) (value >> 24));
  Wire.write((byte) (value >> 16));
  Wire.write((byte) (value >> 8));
  Wire.write((byte) value);
  Wire.endTransmission();
}
/**********************************************************************
  Mechanical stuff to write the value to memory within a task.
  May only be called within an MEM_TASK_WRITE or MEM_TASK_COPY.
/*********************************************************************/
void writeTaskDoing(boolean asCopy, byte pairNr, unsigned long index, //
                    unsigned long value) {
  //  //Choose the pair to write, that holds not the actual value
  //  if (pairNr == 0)
  //    memWrite(asCopy, 1, index, value);
  //  else
  //    memWrite(asCopy, 0, index, value);
  memWrite(asCopy, pairNr, index, value);

  mem.timer = millis() + 6;
  mem.timerCounter = millis() + MEM_TIMER_INTERVALL_MS;
  mem.taskDone = true;
}


/**********************************************************************
  Only for developing.
/*********************************************************************/
unsigned int memGetRoomAddress() {
  return mem.roomAddress;
}

/**********************************************************************
  Must be called within loop() to give MEM a change to update the
  memory.
  The program number will be returned.
/*********************************************************************/
byte memControl() {


  // Controlling of programs an their tasks
  switch (mem.program) {

      // Normal write of data value to memory.
    case MEM_PROGRAM_WRITE_DATA:
      if (mem.taskDone) {
        switch (mem.task) {
          case MEM_TASK_WRITE_DATA:
            //sloLogS("Starting MEM_TASK_WRITE_DATA_WAIT");
            mem.task = MEM_TASK_WRITE_DATA_WAIT;
            mem.taskDone = false;
            break;
          case MEM_TASK_WRITE_DATA_WAIT:
            //sloLogS("Starting MEM_TASK_COPY_DATA");
            mem.task = MEM_TASK_COPY_DATA;
            mem.taskDone = false;
            break;
          case MEM_TASK_COPY_DATA:
            //sloLogS("Starting MEM_TASK_COPY_DATA_WAIT");
            mem.task = MEM_TASK_COPY_DATA_WAIT;
            mem.taskDone = false;
            break;
          case MEM_TASK_COPY_DATA_WAIT:
            //sloLogS("Starting MEM_TASK_COPY_DATA_WAIT");
            mem.task = MEM_TASK_VERIFY_WRITTEN_DATA;
            mem.taskDone = false;
            //sloLogUL("Duration=", millis()-timer3);
            break;
          case MEM_TASK_VERIFY_WRITTEN_DATA:
            //sloLogS("Program MEM_PROGRAM_WRITE_DATA finished.");
            mem.program = MEM_PROGRAM_NULL;
            mem.task = MEM_TASK_NULL;
            mem.taskDone = false;
            //digitalWrite(LED_GREEN, LOW)
            //sloLogUL("Duration=", millis()-timer3);

            //sloLogUL("mem.index.value=", mem.index.value);
            //sloLogUL("mem.data.value", mem.data.value);
            //Check for max write cycles into the bank and switch to next bank
            //            if (mem.index.value * mem.data.value > MEM_MAX_BANK_WRITINGS) {
            if (mem.data.value > (mem.index.value * MEM_MAX_BANK_WRITINGS)) {
              //sloLogS("Starting MEM_PROGRAM_SWITCH_BANK");
              mem.program = MEM_PROGRAM_SWITCH_BANK;
              mem.task = MEM_TASK_WRITE_DATA;
              mem.taskDone = false;
              //Important: Write data value to next bank
              mem.writeIndexValue++;

              //Always write into first pair.
              mem.writePairNr = 0;
            }

            break;
        }
      }
      break;

      // Program create a copy of the data value to the next bank.
      // After that, for switching to this new bank, the index will
      // be incremented.
      // Precondition: MEM_PROGRAM_WRITE_DATA successfull executed.
    case MEM_PROGRAM_SWITCH_BANK:
      if (mem.taskDone) {
        switch (mem.task) {
          case MEM_TASK_WRITE_DATA:
            //sloLogS("Starting MEM_TASK_WRITE_DATA_WAIT");
            mem.task = MEM_TASK_WRITE_DATA_WAIT;
            mem.taskDone = false;
            break;
          case MEM_TASK_WRITE_DATA_WAIT:
            //sloLogS("Starting MEM_TASK_COPY_DATA");
            mem.task = MEM_TASK_COPY_DATA;
            mem.taskDone = false;
            break;
          case MEM_TASK_COPY_DATA:
            //sloLogS("Starting MEM_TASK_COPY_DATA_WAIT");
            mem.task = MEM_TASK_COPY_DATA_WAIT;
            mem.taskDone = false;
            break;
          case MEM_TASK_COPY_DATA_WAIT:
            //sloLogS("Starting MEM_TASK_VERIFY_WRITTEN_DATA");
            mem.task = MEM_TASK_VERIFY_WRITTEN_DATA;
            mem.taskDone = false;
            //sloLogUL("Duration=", millis()-timer3);
            break;
          case MEM_TASK_VERIFY_WRITTEN_DATA:
            //sloLogS("Starting MEM_TASK_WRITE_INDEX");
            mem.task = MEM_TASK_WRITE_INDEX;
            mem.taskDone = false;

            //Toggle pair
            if (mem.index.pairNr == 0)
              mem.writePairNr = 1;
            else
              mem.writePairNr = 0;

            //digitalWrite(LED_GREEN, LOW)
            //sloLogUL("Duration=", millis()-timer3);
            break;
          case MEM_TASK_WRITE_INDEX:
            //sloLogS("Starting MEM_TASK_WRITE_INDEX_WAIT");
            mem.task = MEM_TASK_WRITE_INDEX_WAIT;
            mem.taskDone = false;
            break;
          case MEM_TASK_WRITE_INDEX_WAIT:
            //sloLogS("Starting MEM_TASK_COPY_INDEX");
            mem.task = MEM_TASK_COPY_INDEX;
            mem.taskDone = false;
            break;
          case MEM_TASK_COPY_INDEX:
            //sloLogS("Starting MEM_TASK_COPY_INDEX_WAIT");
            mem.task = MEM_TASK_COPY_INDEX_WAIT;
            mem.taskDone = false;
            break;
          case MEM_TASK_COPY_INDEX_WAIT:
            //sloLogS("Starting MEM_TASK_VERIFY_WRITTEN_INDEX");
            mem.task = MEM_TASK_VERIFY_WRITTEN_INDEX;
            mem.taskDone = false;
            //sloLogUL("Duration=", millis()-timer3);
            break;
          case MEM_TASK_VERIFY_WRITTEN_INDEX:
            //sloLogS("End of MEM_PROGRAM_SWITCH_BANK");
            mem.program = MEM_PROGRAM_NULL;
            mem.task = MEM_TASK_NULL;
            mem.taskDone = false;
            //digitalWrite(LED_GREEN, LOW)
            //sloLogUL("Duration=", millis()-timer3);
            break;
        }
      }
      break;

      //No program running. Start program with first task
    default:
      //Counter to write
      if ( millis() > mem.timerCounter //
           && mem.counter > mem.writeCounter) {
        //sloLogS("Starting MEM_PROGRAM_WRITE_DATA");
        mem.program = MEM_PROGRAM_WRITE_DATA;
        mem.task = MEM_TASK_WRITE_DATA;
        mem.taskDone = false;
        mem.writeIndexValue = mem.index.value;
        mem.writeCounter = mem.counter;

        //Toggle Pair in the bank
        if (mem.data.pairNr == 0)
          mem.writePairNr = 1;
        else
          mem.writePairNr = 0;

        //sloLogUL("mem.writeIndexValue=", mem.writeIndexValue);
        //sloLogUL("mem.writeCounter=", mem.writeCounter);
        //sloLogUL("mem.data.pairNr=", mem.data.pairNr);
      }
      break;
  }

  // Execution of task
  if (!mem.taskDone)
    switch (mem.task)
    {
      case MEM_TASK_WRITE_DATA_WAIT:
      case MEM_TASK_COPY_DATA_WAIT:
      case MEM_TASK_WRITE_INDEX_WAIT:
      case MEM_TASK_COPY_INDEX_WAIT:
        if (millis() > mem.timer)
          mem.taskDone = true;
        break;

        //--- Verify written data that the pair is valid ----
      case MEM_TASK_VERIFY_WRITTEN_DATA:
        //sloLogS("Executing MEM_TASK_VERIFY_WRITTEN_DATA");

        mem.writtenValue = memGetData(mem.writeIndexValue);
        //sloLogUL("mem.writeIndexValue=", mem.writeIndexValue);
        //sloLogUL("mem.writeCounter=", mem.writeCounter);
        //sloLogUL("mem.writePairNr=", mem.writePairNr);

        //sloLogUL("mem.writtenValue.pairNr=", mem.writtenValue.pairNr);
        //sloLogUL("mem.writtenValue.value=", mem.writtenValue.value);

        //When data like expected toggle pair for next writing
        if (mem.writtenValue.value == mem.writeCounter &&
            mem.writtenValue.pairNr == mem.writePairNr &&
            mem.writtenValue.pairNr != MEM_PAIR_NULL) {
          mem.data = mem.writtenValue;

          //Temp Var no longer used
          mem.writtenValue = {0, 0};
          //sloLogS("Writing was OK. Set mem.data.");
        }
        else {
          //digitalWrite(LED_RED, HIGH);
          //sloLogS("MEM_TASK_VERIFY_WRITTEN_DATA: Writing was NOK !!!!");
          mem.program = MEM_PROGRAM_NULL;
          mem.task = MEM_TASK_NULL;
          mem.taskDone = false;
        }
        mem.taskDone = true;
        break;

        //--- Create copy of counter value ----
      case MEM_TASK_COPY_DATA:
        writeTaskDoing(true, mem.writePairNr, mem.writeIndexValue, mem.writeCounter);
        break;

      case MEM_TASK_WRITE_DATA:
        writeTaskDoing(false, mem.writePairNr, mem.writeIndexValue, mem.writeCounter);
        break;

      case MEM_TASK_VERIFY_WRITTEN_INDEX:
        //sloLogS("Executing MEM_TASK_VERIFY_WRITTEN_INDEX");
        mem.writtenValue = memReadBank(mem.roomAddress);
        //sloLogUL("mem.writeIndexValue=", mem.writeIndexValue);
        //sloLogUL("mem.writePairNr=", mem.writePairNr);

        //sloLogUL("mem.writtenValue.pairNr=", mem.writtenValue.pairNr);
        //sloLogUL("mem.writtenValue.value=", mem.writtenValue.value);

        //When index like expected toggle pair for next writing
        if (mem.writtenValue.value == mem.writeIndexValue &&
            mem.writtenValue.pairNr == mem.writePairNr &&
            mem.writtenValue.pairNr != MEM_PAIR_NULL) {
          mem.index = mem.writtenValue;

          //Temp Var no longer used
          mem.writtenValue = {0, 0};
          //sloLogS("Writing was OK. Set mem.data.");
        }
        else {
          //digitalWrite(LED_RED, HIGH);
          //sloLogS("MEM_TASK_VERIFY_WRITTEN_INDEX: Writing was NOK !!!!");
          //Don't use this index, return to old one
          mem.writeIndexValue = mem.index.value;
        }
        mem.taskDone = true;

        break;
      case MEM_TASK_COPY_INDEX:
        writeTaskDoing(true, mem.writePairNr, 0, mem.writeIndexValue);
        break;
      case MEM_TASK_WRITE_INDEX:
        writeTaskDoing(false, mem.writePairNr, 0, mem.writeIndexValue);
        break;
    }

  return mem.program;
}
