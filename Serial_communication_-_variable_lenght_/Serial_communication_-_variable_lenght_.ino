
/**
 * this program is to test a way of dynamically communicating over the serial bus. 
 * The main objectives are:
 * a fast way of receiving short commands, responding with low latency
 * also allowing longer messages to be sent, where latency is not a problem
 * Robust for messages being too short or too long, or invalid
 * ability to send error messages back
 * a Diagnostic on/off function, sending a lot more information back
 * 
 * Principle:
 * (1) Header Bytes and start reading from Serial
 * - the first Byte of any message is '255'
 * - the second byte is 'Mode' which is used further down in the program for 'cases'
 * - the third Byte is 'DataLength" and says how many bytes will follow: this is the actual data. if '0' it means no further Bytes follow
 * - messages are thus always a minimum of THREE Bytes: 1:Begin ("255") 2:Mode 3:DataLength 
 * - the program waits with reading in Bytes until there are a minimum of 3 Bytes in the buffer. Then it starts reading them.
 * - Once >3 Bytes are recieved if the first is a '255' it kicks off a reading cycle. If a different value is received, it is discarded
 * (2) Reading in the DataBytes
 * - if DataLength > 0, first a check is being done on whether the number of Bytes in the buffer is correct for the Datalength of the message:
 * -- If there are less Bytes in the buffer than expected, the program waits until (CommsTimeout) and if they haven't arrived, discards the Bytes
 * -- If there are too many Bytes in the buffer, the program doesn't trust the information, and discards all Bytes in the buffer.
 * - If the Bytes in the buffer correspond to the Datalength, the bytes can be read either into an array or into variables - but this is done within the 'cases' or Modes 
 * - 2 kinds of Modes: 
 * 0-99 are Performance modes and make the Arduino 'do' something (like switch LEDs on and off, or switch between presets)
 * 100-199  Preset modes: allow the loading of preset configurations of the entire LED string. They are slow to load but stored on the Arduino for quick retrieval
 * - Modes
 * - Mode 0 does Nothing. There's a reason for that:
 * - Mode 1-99 One-OFF modes, for example reading in a set of bytes to store in an Array, set 'Mode = 0' at the end of doing their job: this way the case for One-off modes runs only once
 * - Mode 100 - 199 CONTINUOUS  modes, where until the mode is changed, we want the program to keep executing this mode. 
 **/

//LED SETUP
#include <Adafruit_NeoPixel.h>
#define PIN 12
const  int nLEDs = 8; // standard arraylength
Adafruit_NeoPixel strip = Adafruit_NeoPixel(nLEDs, PIN, NEO_GRB + NEO_KHZ800);

// PROGRAM CONTROL and Diagnostic
const int ArduinoLedPin =  13;      // the number of the Arduino LED pin - it's blinking helps seeing if the program runs
int Diagnostic = 1;                // switches on all kinds of diagnostic feedback from various locations in the program
int LooptimeDiag = 1;              // minimal feedback for checking efficiency: only feeds back looptime
int ArrayDiag = 1;                 // if switched on, prints all arrays every cycle
unsigned long Slowdown = 1500;                  // Delay value (ms) added to each loop, only in 'Diagnostic' mode to allow inspecting the data coming back over serial
unsigned long msTable[8] = {
  0, 100, 200, 500, 1000, 1500, 2000, 5000};
unsigned long previousMillis = 0;  // will store last time at the end of the previous loop
unsigned long currentMillis = 0;  // will store the current time
int LoopIteration = 0;             // to track loop iterations
int BytesInBuffer = 0;             // number of unread Bytes waiting in the serial buffer
int DiscardedBytes = 0;            // number of Bytes discarded (read out of the serial buffer but not used) since the last start of a read operation. Indicates something is wrong
unsigned long CommsTimeout = 1000;    // When the program is expecting X bytes to arrive, this is how long it will wait before discarding
unsigned long WaitedForBytes = 0;      //variable to time how long we've been waiting for the required bytes to arrive. To use to break out of a loop when Bytes don't arrive

// PROGRAM - required for the core functionality of the sketch
byte Mode = 0;
byte ContMode = 0;
byte OnceMode = 0;
byte DataLength = 0;
int colorByte = 0;

// Standard empty arrays to be filled with by the program
byte STATEX[24] = {
  0, 10, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0, 0, 10, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0,  };
byte STATE0[nLEDs*3]= {
  10, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0, 0, 10, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0, 10 };
byte STATE1[nLEDs*3]= {
  10, 0, 0, 0, 50, 10, 10, 10, 10, 0, 0, 0, 10, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0, 10 };
byte STATE2[nLEDs*3]= {
  10, 0, 0, 0, 10, 10, 10, 50, 10, 0, 0, 0, 10, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0, 10 };
byte STATE3[nLEDs*3]= {
  10, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0, 0, 50, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0, 10 };
byte STATE4[nLEDs*3]= {
  10, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0, 0, 10, 0, 0, 0, 10, 10, 10, 50, 10, 0, 0, 10 };

void setup() {
  Serial.begin(9600); 
  pinMode(ArduinoLedPin, OUTPUT);
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  strip.setPixelColor(3, strip.Color(10,0,20)); // Set new pixel 'on'
  strip.show();              // Refresh LED states
}

//**********************************************************
//*************         M A i N       **********************
//**********************************************************

void loop() 
{ 
  ++LoopIteration;
  BytesInBuffer = Serial.available();
  Serial.print("]");
  Serial.println(BytesInBuffer);
  if (BytesInBuffer == 0) { 
    DiscardedBytes = 0; 
  }
  if (Diagnostic == 1) {    
    delay(Slowdown); 
    Serial.print("[ **** NEW LOOP: ");  
    Serial.println(LoopIteration);
    Serial.print("[ currentMillis: ");
    Serial.println(currentMillis);
    Serial.print("[ Slowdown: ");
    Serial.println(Slowdown);
    LoopBlink(LoopIteration);
  }

  
  // Start when data arrived
  if (BytesInBuffer > 2) // all messages are minimum 3 Bytes so are waiting for 3 before getting going.
  {
    if (Serial.read() == 255) 

      //*************        R E A D       **********************

    { // SECTION 1: MODE and LENGTH
      Serial.println("255! Yep");
      Mode=Serial.read();
      DataLength=Serial.read();
      BytesInBuffer = Serial.available();
      if (Mode > 99) { 
        ContMode = Mode; 
      }
      if (Mode < 100) {
        OnceMode = Mode; 
      }
      if (Diagnostic == 1) { 
        Serial.print("[ Mode: ");
        Serial.print(Mode);
        Serial.print(" // ContMode: ");
        Serial.print(ContMode);
        Serial.print(" // OnceMode: ");
        Serial.println(OnceMode);
        Serial.print("[ DataLength: ");
        Serial.print(DataLength);
        Serial.print(" - Bytes in Buffer: ");
        Serial.println(BytesInBuffer);
      }

      //NOT ENOUGH BYTES
      if (BytesInBuffer < DataLength){

        if (Diagnostic == 1) { 
          Serial.print("[ Waiting for ");
          Serial.print(DataLength-BytesInBuffer);
          Serial.println(" more Bytes");
        }
        delay (500);
        BytesInBuffer = Serial.available();
        if (BytesInBuffer < DataLength){
        // Do some waiting around here, if that doesn't help:
        Serial.print("[ ERROR: Missing ");
        Serial.print(DataLength - BytesInBuffer);
        Serial.println("Bytes in buffer, Aborting read operation, dumping data");
        char dump[BytesInBuffer];
        Serial.readBytes(dump, BytesInBuffer);}

      }
      if (BytesInBuffer > DataLength) {
        //TOO MANY BYTES
        Serial.print("[ ERROR: ");
        Serial.print(DataLength-BytesInBuffer);
        Serial.println(" too many Bytes in buffer. Dumping data ");
        char dump[BytesInBuffer];
        Serial.readBytes(dump, BytesInBuffer);
      }

    }
    else // IF 3 bytes or more AND the first is not '255'; cycle back to start without doing anything with the read Byte
    { 
      ++DiscardedBytes;
      Serial.print("[ ERROR: Bytes received not starting with '255' Discarded: ");
      Serial.println(DiscardedBytes);
    } //End invalid data section (ie data did not start with '255' and is non-255 byte is discarded)
  } // End reading / discarding data section which only runs when there are 3 Bytes or ore in the buffer


  //*************       O N C E  M O D E S      **********************
  // this section represents modes that run once only - a state change, or a download of settings

    switch (OnceMode) {
  case 1: //All Off
    {
      for(int i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, 0); // Erase pixel, but don't refresh!
      }
      strip.show();  
      OnceMode = 0;      // Refresh LED states
      break;
    }
  case 2:
    {
      Serial.readBytes((char *)STATEX, DataLength);
      ArrayToPixels(STATEX, 24);
      OnceMode = 0;
      break;
    }
  case 99: //Mode 99 is CONFIG. Bytes set: Diagnostic, Delay
    {
      Diagnostic = Serial.read();
      int i = Serial.read();
      Slowdown = msTable[i];
      LooptimeDiag = Serial.read();
      ArrayDiag = Serial.read();
      CommsTimeout = msTable[Serial.read()];
      if (Diagnostic == 1) { 
        Serial.print("[ Diagnostic set to: ");
        Serial.println(Diagnostic);
        Serial.print("[ Slowdown set to: ");
        Serial.println(Slowdown);
        Serial.print("[ CommsTimeout set to: ");
        Serial.println(CommsTimeout);
      }
      OnceMode = 0;
      break;
    }
  default: 
    if (Diagnostic == 1) { 
      Serial.print("[ Once Mode ");
      Serial.print(OnceMode);
      Serial.println(" not yet implemented");
    }
  }

  //*************       C O N T I N U O U S  M O D E S      **********************
  // this section represents modes that run continuously, once with every main loop cycle- to be used for time-base effects

  switch (ContMode) 

  { //Start MODES Section
  case 0:
    {
      if (Diagnostic == 1) { 
        Serial.println("[ Continuous Mode 0 - not doing anything");
      }
      break;
    }

  default: 
    if (Diagnostic == 1) { 
      Serial.print("[ Cont Mode ");
      Serial.print(ContMode);
      Serial.println(" not yet implemented");
    }
  }  // END Cont Modes Section



    //*************       L A S T  P A R T  O F  M A I N  P R O G R A M      **********************

  currentMillis = millis();
  if (LooptimeDiag == 1) {    
    Serial.print("[ Looptime: ");  
    Serial.println(currentMillis - previousMillis); 
  }
  if (ArrayDiag == 1)
  {
    ArrayToSerial(STATEX, 24);
    ArrayToSerial(STATE0, 24);
  }
  if (Diagnostic == 1) {  
    Serial.print("[ **** END OF LOOP ");
    Serial.print(LoopIteration);
    Serial.print(",   Looptime: ");  
    Serial.println(currentMillis - previousMillis); 
  }
  previousMillis = currentMillis;
} //End main loop


//*************       F U N C T I O N S      **********************

// Array to NeoPixels
void ArrayToPixels(byte Array[],int N) {
  for (int i = 0; i < N/3; i++)
  {
    int pix = i;
    int r = Array[i*3];
    int g = Array[i*3 + 1];
    int b = Array[i*3 + 2];
    colorByte = strip.Color(r, g, b);
    strip.setPixelColor(pix, strip.Color(r, g, b)); // Set new pixel 'on'
    strip.show();              // Refresh LED states
  }
}

// PrintArrayToSerial
void ArrayToSerial(byte Array[],int N) {
  for (int i=0; i< N ; i++)
  {
    Serial.print(" ");
    Serial.print(Array[i], DEC);
    Serial.print(" ");
  } 
  Serial.println("");
}

// Blink ArduinoLED to show program is running. Toggles On/Off every loop
void LoopBlink(int Loop)
{
  if (LoopIteration % 2)
  { 
    digitalWrite(ArduinoLedPin, HIGH);
  }
  else
  { 
    digitalWrite(ArduinoLedPin, LOW);
  }
}
// End blinkLed






























