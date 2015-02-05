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
 **/

const int ArduinoLedPin =  13;      // the number of the LED pin
const  int nLEDs = 8; // standard arraylength

unsigned long previousMillis = 0;        // will store last time LED was updated
int LoopIteration = 0;            // to track loop iterations
int DiscardedBytes = 0;          // adds one everytime a byte is discarded
const long CommsTimeout = 1000;           // When the program is expecting X bytes to arrive, this is how long it will wait before discarding

byte Mode = 0;
byte DataLength = 0;

// Standard empty arrays to be filled with by the program
byte STATE0[nLEDs*3];
byte STATE1[nLEDs*3];
byte STATE2[nLEDs*3];
byte STATE3[nLEDs*3];
byte STATE4[nLEDs*3];

void setup() {
  Serial.begin(9600); 
  pinMode(ArduinoLedPin, OUTPUT);
}

void loop() 
{ ++LoopIteration;
  unsigned long currentMillis = millis();
  Serial.print("[ CurrentMillis: ");
  Serial.println(currentMillis);
  Serial.print("[ Loop#: ");
  Serial.println(LoopIteration);
  Serial.print("[ Looptime: ");
  Serial.println(currentMillis - previousMillis);
  previousMillis = currentMillis;

  if (LoopIteration % 2)
  { 
   digitalWrite(ArduinoLedPin, HIGH);
  }
  else
  { 
    digitalWrite(ArduinoLedPin, LOW);
  }

  delay (500); 

Serial.print("]");
  Serial.println(Serial.available());

  // Start when data arrived
  if (Serial.available() > 2) // all messages are minimum 3 Bytes so are waiting for 3 before getting going.
  {
    int inByte=Serial.read();
    Serial.print("inByte: ");
    Serial.println(inByte);

    if (inByte == 255) 
      // ********START OF MAIN PROGRAM READING DATA
    { // SECTION 1: MODE and LENGTH
      Serial.println("255! Yep");

      Mode=Serial.read();
      Serial.print("Mode: ");
      Serial.println(Mode);

      DataLength=Serial.read();
      Serial.print("DataLength: ");
      Serial.println(DataLength);
      // SECTION 2: Read in the databytes
      if (DataLength > 1)
      { // if there are databytes
        Serial.readBytes((char *)STATE0, DataLength);
      } // end reading databytes
    }// End valid data section (ie data starts with 255)
    else
    { 
      ++DiscardedBytes;
      Serial.print("Discarded: ");
      Serial.println(DiscardedBytes);
    }//End invalid data section (ie data did not start with '255' and is non-255 byte is discarded)
  }// End reading / discarding data section 

// ***** Runs every cycle, whether data arrived or not:

} //End main loop











