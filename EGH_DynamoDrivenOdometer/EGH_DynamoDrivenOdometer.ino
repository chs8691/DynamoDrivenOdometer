/*
   Bike Distance Counter
   Version 1.2.1, 2015/01/13

   Features:
   - Needs no battery, just bike dynamo and power support
     of the buffered front light
   - Read the actual wheel rotation and store it on EEPROM
   - Communicate the value via Bluetooth 4.0 to the Android App


   Change Log

   Version 1.2.1
   - Change circumference to 1300 mm

   Version 1.2
   - Store distance with 5 cm precision
   - BT send real distance values in mm
   - BT send interval every second

   Version 1.1
   - Demo release with all features but only sends ticks.


 */

// Needed by BLT
#include <RFduinoBLE.h>

// Needed by MEM
#include <Wire.h>

////////////////////////////////////////////////////////
// GPIO 0 ..6 definition
////////////////////////////////////////////////////////
const int LED = 3;
const int REED = 2;

////////////////////////////////////////////////////////
// Variable declarations
////////////////////////////////////////////////////////

// True, if a hard error occured. If device falls into error==true,
// the device has to be restarted by interrupting power supply.
boolean error;

// Temp return value of an method call.
int ret = 0;

//Last error Message
String errorMessage;

// In error case count for next sending
boolean errorSend;

//

/******************************************************
  Setup
/******************************************************/
void setup()
{
  unsigned long value = 0;
  byte pairNr = 0;

  // Open serial communications for debug and pc controlling
  sloSetup(false); //Disable serial logging with false

  error = false;
  errorMessage = "-";
  errorSend = false;

  // Initialize the modules
  lecSetup(LED);
  rstSetup(REED);
  memSetup();
  bltSetup("UP");

  errorMessage = "IDX: " + String(memGetIndexValue()) + " " + String(memGetIndexPairNr());
  sloLogS(errorMessage);
  bltSendMessage(errorMessage);

}
/******************************************************
  Loop
/******************************************************/
void loop(void) {

  // Count wheels rotations and send to app
  if (rstRead())
  {
    memTick();
    lecFlash();
    sloLogUL("Counter=", memGetCounter());
  }

  // Send actual data once a second
  bltSendData(memGetCounter());

  // Let MEM make its stuff
  memControl();

  // Let LEC make its stuff
  lecControl();



}
/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
/*                                                                    */
/*      Module MEM - Error Tolerance Persisting for 24AA256           */
/*                                                                    */
/*  Version 1.1, 2014/12/01                                           */
/*  Write an read one unsigned long value to the external memory.     */
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
/*   - Max. distance value is 200,000 km with precision of 5 cm       */
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

  // actual value of the counter in mm
  unsigned long counter;

  // Last read value from memory in mm
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

  //Wheel circumference in mm
  unsigned int circumferenceMM;


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
const unsigned int MEM_CONFIGURATION_BANKS = 100;

// Offset for first page of room part (in pages)
const unsigned int MEM_ROOM_BANK_OFFSET = MEM_UNUSED_BANKS + MEM_CONFIGURATION_BANKS;

// A data bank may only be used for 1 millon writes per pair. To be on the safe side,
// a bank should be used only 1 million times.
const unsigned long MEM_MAX_BANK_WRITINGS = 1000000L;

// Needed for filter this values (EEPROM's bytes are initialized with 0xFF)
const unsigned long MEM_MAX_UNSIGNED_LONG = 4294967295;

// to store up to 200.000 km into an ULong as fine as possible, the value
// has to reduced before writing to memory
// In a later release this could be avoided by splitting the distance into
// to ULongs (e.g. m and mm)
const unsigned int MEM_DISTANCE_STORAGE_FACTOR = 50;


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
struct MemValidValue memReadIndex() {

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
     Returns valid payload data by reading from memory and transorming
     into the destination format.
     Internal use only.
     Precondition: memReadIndex() called before.
     Return
       MemValidValue transformed data.
************************************************************************/
struct MemValidValue memReadData(unsigned long index) {

  // Read memory
  MemValidValue data = memReadBank(mem.roomAddress + index * MEM_BANK_SIZE);

  // Transform stored cm into mm
  data.value *= MEM_DISTANCE_STORAGE_FACTOR;

  return data;

}
/***********************************************************************
     Returns actual index pairNr.
     Precondition: memSetup() must be called.
************************************************************************/
byte memGetIndexPairNr() {
  return mem.index.pairNr;
}
/***********************************************************************
     Returns actual index value.
     Precondition: memSetup() must be called.
************************************************************************/
unsigned long memGetIndexValue() {
  return mem.index.value;
}

/***********************************************************************
     Returns actual value pairNr.
     Precondition: memSetup() must be called.
************************************************************************/
byte memGetDataPairNr() {
  return mem.data.pairNr;
}

/***********************************************************************
     Returns actual data value. Precondition: memSetup() must be called.
************************************************************************/
unsigned long memGetDataValue() {
  return mem.data.value;
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
  mem.counter += mem.circumferenceMM;

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

  //--- Set configuration data ---//
  // My original Brompton with 6.8 bar air pressure has 1303 mm circumference
  mem.circumferenceMM = 1300;

  //--- Set transaction data ---//
  // Index adress is at the first place inside a room
  mem.index = memReadIndex();

  // Data bank address need an offset 1 because of the index bank
  mem.data = memReadData(mem.index.value);

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
            // Check for max write cycles into the bank and switch to next bank
            // Number of ticks per Bank > 1E6
            if ((mem.data.value / mem.circumferenceMM) > (mem.index.value * MEM_MAX_BANK_WRITINGS)) {
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

        mem.writtenValue = memReadData(mem.writeIndexValue);
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
        // Counter value has to be transform before writing
        writeTaskDoing(true, mem.writePairNr, mem.writeIndexValue, mem.writeCounter / MEM_DISTANCE_STORAGE_FACTOR);
        break;

      case MEM_TASK_WRITE_DATA:
        // Counter value has to be transform before writing
        writeTaskDoing(false, mem.writePairNr, mem.writeIndexValue, mem.writeCounter / MEM_DISTANCE_STORAGE_FACTOR);
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
/*  Prio 2: One blinking for 5 long time.                             */
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

//const int LEC_BLINK_MS = 50;
const int LEC_BLINK_MS = 750;
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
  Light up the LED for 750 ms. Priority 2. lecControl() must be
  called within the loop().
/*********************************************************************/
void lecBlink() {

  //Only start blink if not interval is running
  if (lec.interval || lec.blnk)
    return;

  //Stop flash program
  lec.flash = false;


  //Start blink
  lec.counter = 1;
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
  lec.endMs = millis() + 50;
  sloLogL("time", millis());
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


/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
/*        Module RST - Reed Switch Trigger                            */
/*                                                                    */
/*  Version 1.0, 11.08.2014                                           */
/*  Read the reed contact. Works contact bounce free.                 */
/**********************************************************************/
/**********************************************************************/
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
/**********************************************************************/
/**********************************************************************/
/*                                                                    */
/*        Module BLT - Bluetooth LE Trasmitter                        */
/*                                                                    */
/*  Version 1.1, 05.11.2014                                           */
/*  Sends a value via BT LE in the format char(28), closed with \0:   */
/*  D<long>: a long number, for instance "D2147483647"                */
/*  M<String>: a message,  for instance "MSD_READ_ERROR"              */
/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
typedef struct {

  //Temp. buffer with data
  char buf[28];

  // data string to send
  String data;

  // Time when next value can be read
  // to elimate switch bouncing
  unsigned long nextIntervalMs;

} Blt;
Blt blt;

// How often data will be send in milliseconds. Increase value to
// save energy
const int BLT_INTERVAL_MS = 1000;

// Name of the device, appears in the advertising. Keep it short.
const char* BLT_DEVICE_NAME = "DDO";

/**********************************************************************
   Initializes BLT module. Call this in setup().
/*********************************************************************/
void bltSetup(String welcomeMessage) {


  //Set interval von BLE exchange //TODO Neede?
  // RFduinoBLE.advertisementInterval = BLT_INTERVAL_MS;

  //set the BLE device name as it will appear when advertising
  RFduinoBLE.deviceName = BLT_DEVICE_NAME;

  // start the BLE stack
  RFduinoBLE.begin();

  //Initialize send data
  bltSendMessage(welcomeMessage);

  blt.nextIntervalMs = BLT_INTERVAL_MS;
}

/**********************************************************************
  Sends a long value as distance message, e.g. "D12345". Character
  'D' will be add as prefix. Only sends after latency time.
/*********************************************************************/
void bltSendData(unsigned long value) {

  if (millis() > blt.nextIntervalMs) {

    // Send actual distance
    blt.buf = {'\0', '\0', '\0', '\0', '\0', //
               '\0', '\0', '\0', '\0', '\0', //
               '\0', '\0', '\0', '\0', '\0', //
               '\0', '\0', '\0', '\0', '\0', //
               '\0', '\0', '\0', '\0', '\0', //
               '\0', '\0', '\0' //
              };

    blt.data = "D" + String(value) + String('\0');

    blt.data.toCharArray(blt.buf, 28);
    RFduinoBLE.send(blt.buf, 28);

    blt.nextIntervalMs = millis() + BLT_INTERVAL_MS;
      sloLogS("Send data " + blt.data);
  }
}

/**********************************************************************
  Sends message, e.g. "MERROR_SD_CARD_WRITE"
  String message Message text with < 26 characters, without lead 'M'.
/*********************************************************************/
void bltSendMessage(String message) {

  blt.buf = {'\0', '\0', '\0', '\0', '\0', //
             '\0', '\0', '\0', '\0', '\0', //
             '\0', '\0', '\0', '\0', '\0', //
             '\0', '\0', '\0', '\0', '\0', //
             '\0', '\0', '\0', '\0', '\0', //
             '\0', '\0', '\0' //
            };

  // Send actual distance
  blt.data = "M" + message + String('\0');

  blt.data.toCharArray(blt.buf, 28);
  RFduinoBLE.send(blt.buf, 28);

}
/**********************************************************************/
/**********************************************************************/
/**********************************************************************/
/*                                                                    */
/*        Module SLO - A simple logger                                */
/*                                                                    */
/*  Version 1.1, 05.11.2014                                           */
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
