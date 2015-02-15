
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
int LooptimeDiag = 0;              // minimal feedback for checking efficiency: only feeds back looptime
int ArrayDiag = 1;                 // if switched on, prints all arrays every cycle
unsigned long Slowdown = 0;                  // Delay value (ms) added to each loop, only in 'Diagnostic' mode to allow inspecting the data coming back over serial
unsigned long msTable[8] = {
  0, 100, 200, 500, 1000, 1500, 2000, 5000};
unsigned long previousMillis = 0;  // will store last time at the end of the previous loop
unsigned long currentMillis = 0;  // will store the current time
int LoopIteration = 0;             // to track loop iterations
int BytesInBuffer = 0;             // number of unread Bytes waiting in the serial buffer
int DiscardedBytes = 0;            // number of Bytes discarded (read out of the serial buffer but not used) since the last start of a read operation. Indicates something is wrong
unsigned long CommsTimeout = 200;    // When the program is expecting X bytes to arrive, this is how long it will wait before discarding
unsigned long WaitedForBytes = 0;      //variable to time how long we've been waiting for the required bytes to arrive. To use to break out of a loop when Bytes don't arrive

// PROGRAM - required for the core functionality of the sketch
byte Mode = 0;
byte PrevContMode = 0; //Previous Continuous Mode and Once Mode are temporarily stored, 
byte PrevOnceMode = 0; //If data turns out to be invalid, ContMode and OnceMode are restored to the previous version
byte ContMode = 0;
byte OnceMode = 0;
byte DataLength = 0;
int colorByte = 0;

// Standard empty arrays to be filled with by the program
byte STATEX[24] = {
  0, 10, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0, 0, 10, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0,  };
byte STATE10[nLEDs*3]= {
  0, 48, 56, 43, 78, 0, 8, 74, 0, 0, 74, 25, 0, 85, 72, 0, 50, 86, 0, 11, 85, 31, 0, 83, };
byte STATE11[nLEDs*3]= {
  105, 0, 71, 0, 3, 101, 0, 23, 67, 0, 153, 205, 0, 153, 205, 0, 23, 67, 0, 3, 101, 105, 0, 71, };
byte STATE12[nLEDs*3]= {
  86, 29, 0, 138, 120, 0, 0, 43, 8, 0, 99, 34, 35, 91, 0, 47, 59, 0, 48, 75, 0, 28, 13, 0, };
byte STATE13[nLEDs*3]= {
  92, 117, 0, 23, 0, 102, 92, 117, 0, 23, 0, 102, 92, 117, 0, 23, 0, 102, 92, 117, 0, 23, 0, 102,  };
byte STATE14[nLEDs*3]= {
  23, 0, 102, 92, 117, 0, 23, 0, 102, 92, 117, 0, 23, 0, 102, 92, 117, 0, 23, 0, 102, 92, 117, 0, };
byte STATE15[nLEDs*3]= {
  96, 0, 2, 101, 0, 42, 80, 0, 105, 0, 3, 94, 0, 80, 91, 0, 86, 25, 39, 96, 0, 86, 87, 0, }; 
  byte STATE16[24] = {
  0, 10, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0, 0, 10, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0,  };
byte STATE17[nLEDs*3]= {
  0, 48, 56, 43, 78, 0, 8, 74, 0, 0, 74, 25, 0, 85, 72, 0, 50, 86, 0, 11, 85, 31, 0, 83, };
byte STATE18[nLEDs*3]= {
  105, 0, 71, 0, 3, 101, 0, 23, 67, 0, 153, 205, 0, 153, 205, 0, 23, 67, 0, 3, 101, 105, 0, 71, };
byte STATE19[nLEDs*3]= {
  86, 29, 0, 138, 120, 0, 0, 43, 8, 0, 99, 34, 35, 91, 0, 47, 59, 0, 48, 75, 0, 28, 13, 0, };
byte STATE20[nLEDs*3]= {
  0, 48, 56, 43, 78, 0, 8, 74, 0, 0, 74, 25, 0, 85, 72, 0, 50, 86, 0, 11, 85, 31, 0, 83, };
byte STATE21[nLEDs*3]= {
  105, 0, 71, 0, 3, 101, 0, 23, 67, 0, 153, 205, 0, 153, 205, 0, 23, 67, 0, 3, 101, 105, 0, 71, };
byte STATE22[nLEDs*3]= {
  86, 29, 0, 138, 120, 0, 0, 43, 8, 0, 99, 34, 35, 91, 0, 47, 59, 0, 48, 75, 0, 28, 13, 0, };
byte STATE23[nLEDs*3]= {
  92, 117, 0, 23, 0, 102, 92, 117, 0, 23, 0, 102, 92, 117, 0, 23, 0, 102, 92, 117, 0, 23, 0, 102,  };
byte STATE24[nLEDs*3]= {
  23, 0, 102, 92, 117, 0, 23, 0, 102, 92, 117, 0, 23, 0, 102, 92, 117, 0, 23, 0, 102, 92, 117, 0, };
byte STATE25[nLEDs*3]= {
  96, 0, 2, 101, 0, 42, 80, 0, 105, 0, 3, 94, 0, 80, 91, 0, 86, 25, 39, 96, 0, 86, 87, 0, }; 
  byte STATE26[24] = {
  0, 10, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0, 0, 10, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0,  };
byte STATE27[nLEDs*3]= {
  0, 48, 56, 43, 78, 0, 8, 74, 0, 0, 74, 25, 0, 85, 72, 0, 50, 86, 0, 11, 85, 31, 0, 83, };
byte STATE28[nLEDs*3]= {
  105, 0, 71, 0, 3, 101, 0, 23, 67, 0, 153, 205, 0, 153, 205, 0, 23, 67, 0, 3, 101, 105, 0, 71, };
byte STATE29[nLEDs*3]= {
  86, 29, 0, 138, 120, 0, 0, 43, 8, 0, 99, 34, 35, 91, 0, 47, 59, 0, 48, 75, 0, 28, 13, 0, };
byte STATE30[nLEDs*3]= {
  92, 117, 0, 23, 0, 102, 92, 117, 0, 23, 0, 102, 92, 117, 0, 23, 0, 102, 92, 117, 0, 23, 0, 102,  };

 

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
      Serial.println("Entered reading section: >=3 Bytes in buffer, first is 255");
      PrevContMode = ContMode; //before reading in the mode, backing up the previous modes
      PrevOnceMode = OnceMode;

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
        Serial.print(" // New ContMode: ");
        Serial.print(ContMode);
        Serial.print(" // New OnceMode: ");
        Serial.println(OnceMode);
        Serial.print("[ DataLength: ");
        Serial.print(DataLength);
        Serial.print(" - Bytes in Buffer: ");
        Serial.println(BytesInBuffer);
      }

      //NOT ENOUGH BYTES
      if (BytesInBuffer < DataLength){
        if (Diagnostic == 1) { 
          Serial.println("[ Entering 'NOT ENOUGH BYTES");
        }
        WaitedForBytes= 0;
        unsigned long StartMillis = millis();

        while ( (BytesInBuffer < DataLength) && (WaitedForBytes < CommsTimeout )){
          BytesInBuffer = Serial.available();
          if (Diagnostic == 1) { 
            Serial.print("[ DataLength: ");
            Serial.print(DataLength);
            Serial.print("=> BytesInBuffer: ");
            Serial.println (BytesInBuffer);
            Serial.print("[ CommsTimeout: ");
            Serial.print(CommsTimeout);
            Serial.print("=> WaitedForBytes: ");
            Serial.println(WaitedForBytes);
          }
          WaitedForBytes = (millis() - StartMillis);
        }
        /// End of while loop. Now there are 2 options: either the bytes arrived, or they didn't and the thing timed out
        if (BytesInBuffer == DataLength){
          if (Diagnostic == 1) {
            Serial.print("[ Bytes arrived after waiting for  ");
            Serial.print(WaitedForBytes);
            Serial.println("ms");
          }
        }

        if (BytesInBuffer < DataLength){
          Serial.print("[ ERROR: Missing ");
          Serial.print(DataLength - BytesInBuffer);
          Serial.print("Bytes in buffer, Waited ");
          Serial.print(WaitedForBytes);
          Serial.println("ms, Aborting read operation, dumping data");
          char dump[BytesInBuffer];
          Serial.readBytes(dump, BytesInBuffer);
          ContMode = PrevContMode; //restoring the previous modes so operation continues unchanged despite the invalid data
          OnceMode = PrevOnceMode ;
        }

      } // End of 'not enough Bytes'
      
      //TOO MANY BYTES
      if (BytesInBuffer > DataLength) {
        if (Diagnostic == 1) { 
          Serial.println("[ Entering 'TOO MANY BYTES");
        }
        Serial.print("[ ERROR: ");
        Serial.print(BytesInBuffer - DataLength);
        Serial.println(" too many Bytes in buffer. Dumping data ");
        char dump[BytesInBuffer];
        Serial.readBytes(dump, BytesInBuffer);
        ContMode = PrevContMode; //restoring the previous modes so operation continues unchanged despite the invalid data
        OnceMode = PrevOnceMode ;
      }

    }
    else // IF 3 bytes or more AND the first is not '255'; cycle back to start without doing anything with the read Byte
    { 
      ++DiscardedBytes;
      Serial.print("[ ERROR: Bytes received not starting with '255' Discarded: ");
      Serial.println(DiscardedBytes);
    } //End invalid data section (ie data did not start with '255' and is non-255 byte is discarded)
  } // End reading / discarding data section which only runs when there are 3 Bytes or ore in the buffer



  ////// BRIDGE section, between validating the incoming data, and reading in the data (if valid)
  if (Diagnostic == 1) { 
    Serial.print("[ Last read Mode: ");
    Serial.print(Mode);
    Serial.print(" // Current ContMode: ");
    Serial.print(ContMode);
    Serial.print(" // Current OnceMode: ");
    Serial.println(OnceMode);
  }


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
  case 10:
    {
      Serial.readBytes((char *)STATE10, DataLength);
      ArrayToPixels(STATE10, 24);
      OnceMode = 0;
      break;
    }
  case 11:
    {
      Serial.readBytes((char *)STATE11, DataLength);
      ArrayToPixels(STATE11, 24);
      OnceMode = 0;
      break;
    }
  case 12:
    {
      Serial.readBytes((char *)STATE12, DataLength);
      ArrayToPixels(STATE12, 24);
      OnceMode = 0;
      break;
    }
  case 13:
    {
      Serial.readBytes((char *)STATE13, DataLength);
      ArrayToPixels(STATE13, 24);
      OnceMode = 0;
      break;
    }
  case 14:
    {
      Serial.readBytes((char *)STATE14, DataLength);
      ArrayToPixels(STATE14, 24);
      OnceMode = 0;
      break;
    }
  case 15:
    {
      Serial.readBytes((char *)STATE15, DataLength);
      ArrayToPixels(STATE15, 24);
      OnceMode = 0;
      break;
    }
  case 20:
    {
      ArrayToPixels(STATE10, 24);
      OnceMode = 0;
      break;
    }
  case 21:
    {
      ArrayToPixels(STATE11, 24);
      OnceMode = 0;
      break;
    }
  case 22:
    {
      ArrayToPixels(STATE12, 24);
      OnceMode = 0;
      break;
    }
  case 23:
    {
      ArrayToPixels(STATE13, 24);
      OnceMode = 0;
      break;
    }
  case 24:
    {
      ArrayToPixels(STATE14, 24);
      OnceMode = 0;
      break;
    }
  case 25:
    {
      ArrayToPixels(STATE15, 24);
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
    ArrayToSerial(STATE10, 24);
    ArrayToSerial(STATE11, 24);
    ArrayToSerial(STATE12, 24);
    ArrayToSerial(STATE13, 24);
    ArrayToSerial(STATE14, 24);
    ArrayToSerial(STATE15, 24);
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


































