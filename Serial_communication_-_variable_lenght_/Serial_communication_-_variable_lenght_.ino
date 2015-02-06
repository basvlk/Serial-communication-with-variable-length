/**
 * this program is to test a way of dynamically communicating over the serial bus. 
 * The main objectives are:
 * a fast way of receiving short commands, responding with low latency
 * also allowing longer messages to be sent, where latency is not a problem
 * Robust for messages being to short or too long
 * ability to send error messages back
 * a Diagnostic on/off function, sending a lot more information back
 * 
 * Principle:
 * - the first Byte of any message is '255'
 * - Reading '255' kicks off a reading cycle. If a different value is received, it is discarded
 * - the second byte is 'Mode' which is used further down in the program for 'cases'
 * - the third Byte is 'DataLength" and says how many bytes will follow: this is the actual data. if '0' it means no further Bytes follow
 * messages are thus always a minimum of THREE Bytes: Begin (255). Mode, DataLength
 * 
 
 ?? FIX: only go into a 'case' if new data has been received, otherwise if the new data arrives while in the case that does reading,
 it will read while there and not through the proper initialise - we - 255 cycle.
 In other words: all the reading needs to be contained and no other reading functions should happen, should only be picked up by thefirst part
 **/
#include <Adafruit_NeoPixel.h>

const int ArduinoLedPin =  13;      // the number of the LED pin
#define PIN 12
const  int nLEDs = 8; // standard arraylength
Adafruit_NeoPixel strip = Adafruit_NeoPixel(nLEDs, PIN, NEO_GRB + NEO_KHZ800);

unsigned long previousMillis = 0;        // will store last time LED was updated
int LoopIteration = 0;            // to track loop iterations
int DiscardedBytes = 0;          // adds one everytime a byte is discarded
const long CommsTimeout = 1000;           // When the program is expecting X bytes to arrive, this is how long it will wait before discarding

int BytesInBuffer = 0;
byte Mode = 0;
byte DataLength = 0;
int colorByte = 0;

// Standard empty arrays to be filled with by the program
byte STATEX[24] = {
  0, 10, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0, 0, 10, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0,  };
byte STATE0[nLEDs*3];
byte STATE1[nLEDs*3];
byte STATE2[nLEDs*3];
byte STATE3[nLEDs*3];
byte STATE4[nLEDs*3];

void setup() {
  Serial.begin(9600); 
  pinMode(ArduinoLedPin, OUTPUT);
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  strip.setPixelColor(3, strip.Color(10,0,20)); // Set new pixel 'on'
  strip.show();              // Refresh LED states
delay(500);
}

void loop() 
{ 
  ++LoopIteration;
  unsigned long currentMillis = millis();
  Serial.print("[ CurrentMillis: ");
  Serial.println(currentMillis);
  Serial.print("[ Loop#: ");
  Serial.println(LoopIteration);
  Serial.print("[ Looptime: ");
  Serial.println(currentMillis - previousMillis);
  previousMillis = currentMillis;

  // Blink ArduinoLED to show program is running. One blink a loop
  if (LoopIteration % 2)
  { 
    digitalWrite(ArduinoLedPin, HIGH);
  }
  else
  { 
    digitalWrite(ArduinoLedPin, LOW);
  }
  // End blinkLed

  delay (1000); 
  BytesInBuffer = Serial.available();
  // "real" code
  Serial.print("]");
  Serial.println(BytesInBuffer);
  if (BytesInBuffer == 0) { 
    DiscardedBytes = 0; 
  }
  // Start when data arrived
  if (BytesInBuffer > 2) // all messages are minimum 3 Bytes so are waiting for 3 before getting going.
  {

    if (Serial.read() == 255) 
      // ********START OF MAIN PROGRAM READING DATA
    { // SECTION 1: MODE and LENGTH
      Serial.println("255! Yep");

      Mode=Serial.read();
      Serial.print("Mode: ");
      Serial.println(Mode);

      DataLength=Serial.read();
      Serial.print("DataLength: ");
      Serial.println(DataLength);
      
    }// End valid data section (ie data starts with 255)
    else
    { 
      ++DiscardedBytes;
      Serial.print("Discarded: ");
      Serial.println(DiscardedBytes);
    }//End invalid data section (ie data did not start with '255' and is non-255 byte is discarded)
  }// End reading / discarding data section 

  // ***** Runs every cycle, whether data arrived or not:

  
  
  switch (Mode) 

  { //Start MODES Section

  case 0: //All Off
    {
      for(int i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, 0); // Erase pixel, but don't refresh!
      }
      strip.show();              // Refresh LED states
      break;
    }
    case 1:
    {
    Serial.readBytes((char *)STATEX, DataLength);
    ArrayToPixels(STATEX, 24);
    }
    
  }  //Start MODES Section
  
  
  ArrayToSerial(STATEX, 24);
  ArrayToSerial(STATE0, 24);




} //End main loop

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
















