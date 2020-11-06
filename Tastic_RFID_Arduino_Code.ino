/* 
 *  11/3/2020 - Modified code.  Removed Serial LCD functionality.  
 *  Reuse 3 pin serial terminal output to trigger transistor circuit for vibration motor for haptic feedback.
 *  
 *  
 *  10/14/2020 - Modified original code to use modern SD library and current Arduino IDE. 
 *  This will allow the use of microSD cards greater than 2GB.
 *  Serial LCD functionality has not been tested with this version.
 * 
 * Original codebase and reference materials can be found here:
 * https://resources.bishopfox.com/resources/tools/rfid-hacking/attack-tools/
 * 
 * 
 * 
 * 
 * 
 * 
 * Tastic RFID Thief
 * By Fran Brown
 * Bishop Fox - www.bishopfox.com
 * 
 * Version 1.0 - 01Aug2013
 * Minor updates to comments: 18Feb2015
 * 
 * ************* 
 * Libraries:
 * 	http://sdfatlib.googlecode.com/files/sdfatlib20111205.zip
 * 
 * *************
 * Summary:
 * 
 * The Tastic RFID Thief is a silent, long-range RFID reader that can  
 * steal the proximity badge information from an unsuspecting employee as they  
 * physically walk near this concealed device.
 * 
 * We used an Arduino microcontroller to weaponize a commercial RFID badge reader 
 * (the HID MaxiProx 5375 � bought on eBay) � effectively turning it into a custom, 
 * long-range RFID hacking tool.  This involved the creation of a small, portable PCB 
 * (designed in Fritzing) that can be inserted into almost any commercial RFID reader 
 * to steal badge info.
 *  
 * Note, this PCB can alternatively be inserted into an Indala reader for testing 
 * Indala Prox deployments (e.g. Indala Long-Range Reader 620).  Alternatively, the 
 * PCB could even be used to weaponize a high frequency (13.56MHz) RFID reader, such 
 * as the iClass R90 Long Range reader.  The PCB can be inserted into any RFID reader 
 * that supports the standard Wiegand DATA0/DATA1 output (which is pretty much all of them).
 *  
 * For more info, see:
 * 	http://www.bishopfox.com/resources/tools/rfid-hacking/attack-tools/
 *  
 * *************
 * Special Thanks:
 * 
 * Special thanks to the following projects, which served as initial
 * sources of code and concept inspiration for the Tastic RFID Thief
 * 
 * 	http://proxclone.com/Long_Range_Cloner.html
 * 	http://www.pagemac.com/azure/arduino_wiegand.php
 * 	http://colligomentis.com/2012/05/16/hid-reader-arduino-rfid-card-catcher/
 * 
 * 
*/


/*#include <SoftwareSerial.h>
#include <SPI.h>*/
#include <SD.h>

#define MAX_BITS 100                 // max number of bits 
#define WEIGAND_WAIT_TIME  3000      // time to wait for another weigand pulse.  
#define txPin 4 // Input lead on haptic feedback module


unsigned char databits[MAX_BITS];    // stores all of the data bits
volatile unsigned int bitCount = 0;
unsigned char flagDone;              // goes low when data is currently being captured
unsigned int weigand_counter;        // countdown until we assume there are no more bits

volatile unsigned long facilityCode=0;        // decoded facility code
volatile unsigned long cardCode=0;            // decoded card code

// Breaking up card value into 2 chunks to create 10 char HEX value
volatile unsigned long bitHolder1 = 0;
volatile unsigned long bitHolder2 = 0;
volatile unsigned long cardChunk1 = 0;
volatile unsigned long cardChunk2 = 0;


// SD card variables
const uint8_t chipSelect = 10; // CS from SD to Pin 10 on Arduino
//SdFat sd; // file system object for SD card
File myFile; // file object
char dataFile[] = "cards.txt"; // file to save card ids to


///////////////////////////////////////////////////////
// Process interrupts

// interrupt that happens when INTO goes low (0 bit)
void ISR_INT0()
{
  //Serial.print("0");
  bitCount++;
  flagDone = 0;
  
  if(bitCount < 23) {
      bitHolder1 = bitHolder1 << 1;
  }
  else {
      bitHolder2 = bitHolder2 << 1;
  }
    
  weigand_counter = WEIGAND_WAIT_TIME;  
  
}

// interrupt that happens when INT1 goes low (1 bit)
void ISR_INT1()
{
  //Serial.print("1");
  databits[bitCount] = 1;
  bitCount++;
  flagDone = 0;
  
   if(bitCount < 23) {
      bitHolder1 = bitHolder1 << 1;
      bitHolder1 |= 1;
   }
   else {
     bitHolder2 = bitHolder2 << 1;
     bitHolder2 |= 1;
   }
  
  weigand_counter = WEIGAND_WAIT_TIME;  
}

///////////////////////////////////////////////////////
// SETUP function
void setup()
{
  pinMode(13, OUTPUT);  // LED
  pinMode(2, INPUT);     // DATA0 (INT0)
  pinMode(3, INPUT);     // DATA1 (INT1)
  
  Serial.begin(57600);
  Serial.println("RFID Readers");
  
  pinMode(txPin, OUTPUT);
  
  // Initialize SD card
  while(!SD.begin(chipSelect)) {
    Serial.println("No SD Card!");
  }
   
  // binds the ISR functions to the falling edge of INTO and INT1
  attachInterrupt(0, ISR_INT0, FALLING);  
  attachInterrupt(1, ISR_INT1, FALLING);

  weigand_counter = WEIGAND_WAIT_TIME;
}


///////////////////////////////////////////////////////
// LOOP function
void loop()
{
  // This waits to make sure that there have been no more data pulses before processing data
  if (!flagDone) {

    
    if (--weigand_counter == 0)
      flagDone = 1;	
  }
  
  // if we have bits and we the weigand counter went out
  if (bitCount > 0 && flagDone) {
    unsigned char i;
    
    getCardValues();
    getCardNumAndSiteCode();
       
    printBits();
    writeSD();
    vibrate();

     // cleanup and get ready for the next card
     bitCount = 0; facilityCode = 0; cardCode = 0;
     bitHolder1 = 0; bitHolder2 = 0;
     cardChunk1 = 0; cardChunk2 = 0;
     
     for (i=0; i<MAX_BITS; i++) 
     {
       databits[i] = 0;
     }
  }
}

///////////////////////////////////////////////////////
// PRINTBITS function
void printBits()
{
      // I really hope you can figure out what this function does
      Serial.print(bitCount);
      Serial.print(" bit card. ");
      Serial.print("FC = ");
      Serial.print(facilityCode);
      Serial.print(", CC = ");
      Serial.print(cardCode);
      Serial.print(", 44bit HEX = ");
      Serial.print(cardChunk1, HEX);
      Serial.println(cardChunk2, HEX);
}

///////////////////////////////////////////////////////
// Vibrate function
void vibrate()
{
      //uses pin 4 for haptic feedback module (vibration motor)
      digitalWrite(txPin, HIGH);
      delay(500);
      digitalWrite(txPin, LOW);
      delay(250);
      digitalWrite(txPin, HIGH);
      delay(500);
      digitalWrite(txPin, LOW);
      delay(250);
}

///////////////////////////////////////////////////////
// SETUP function
void getCardNumAndSiteCode()
{
     unsigned char i;
  
    // we will decode the bits differently depending on how many bits we have
    // see www.pagemac.com/azure/data_formats.php for more info
    // also specifically: www.brivo.com/app/static_data/js/calculate.js
    switch (bitCount) {

      
    ///////////////////////////////////////
    // standard 26 bit format
    // facility code = bits 2 to 9  
    case 26:
      for (i=1; i<9; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }
      
      // card code = bits 10 to 23
      for (i=9; i<25; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }
      break;

    ///////////////////////////////////////
    // 33 bit HID Generic    
    case 33:  
      for (i=1; i<8; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }
      
      // card code
      for (i=8; i<32; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }    
      break;

    ///////////////////////////////////////
    // 34 bit HID Generic 
    case 34:  
      for (i=1; i<17; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }
      
      // card code
      for (i=17; i<33; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }    
      break;
 
    ///////////////////////////////////////
    // 35 bit HID Corporate 1000 format
    // facility code = bits 2 to 14     
    case 35:  
      for (i=2; i<14; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }
      
      // card code = bits 15 to 34
      for (i=14; i<34; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }    
      break;

    }
    return;
  
}


//////////////////////////////////////
// Function to append the card value (bitHolder1 and bitHolder2) to the necessary array then tranlate that to
// the two chunks for the card value that will be output
void getCardValues() {
  switch (bitCount) {
    case 26:
        // Example of full card value
        // |>   preamble   <| |>   Actual card value   <|
        // 000000100000000001 11 111000100000100100111000
        // |> write to chunk1 <| |>  write to chunk2   <|
        
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 2){
            bitWrite(cardChunk1, i, 1); // Write preamble 1's to the 13th and 2nd bits
          }
          else if(i > 2) {
            bitWrite(cardChunk1, i, 0); // Write preamble 0's to all other bits above 1
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 20)); // Write remaining bits to cardChunk1 from bitHolder1
          }
          if(i < 20) {
            bitWrite(cardChunk2, i + 4, bitRead(bitHolder1, i)); // Write the remaining bits of bitHolder1 to cardChunk2
          }
          if(i < 4) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i)); // Write the remaining bit of cardChunk2 with bitHolder2 bits
          }
        }
        break;

    case 27:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 3){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 3) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 19));
          }
          if(i < 19) {
            bitWrite(cardChunk2, i + 5, bitRead(bitHolder1, i));
          }
          if(i < 5) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 28:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 4){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 4) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 18));
          }
          if(i < 18) {
            bitWrite(cardChunk2, i + 6, bitRead(bitHolder1, i));
          }
          if(i < 6) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 29:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 5){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 5) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 17));
          }
          if(i < 17) {
            bitWrite(cardChunk2, i + 7, bitRead(bitHolder1, i));
          }
          if(i < 7) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 30:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 6){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 6) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 16));
          }
          if(i < 16) {
            bitWrite(cardChunk2, i + 8, bitRead(bitHolder1, i));
          }
          if(i < 8) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 31:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 7){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 7) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 15));
          }
          if(i < 15) {
            bitWrite(cardChunk2, i + 9, bitRead(bitHolder1, i));
          }
          if(i < 9) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 32:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 8){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 8) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 14));
          }
          if(i < 14) {
            bitWrite(cardChunk2, i + 10, bitRead(bitHolder1, i));
          }
          if(i < 10) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 33:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 9){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 9) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 13));
          }
          if(i < 13) {
            bitWrite(cardChunk2, i + 11, bitRead(bitHolder1, i));
          }
          if(i < 11) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 34:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 10){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 10) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 12));
          }
          if(i < 12) {
            bitWrite(cardChunk2, i + 12, bitRead(bitHolder1, i));
          }
          if(i < 12) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 35:        
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 11){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 11) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 11));
          }
          if(i < 11) {
            bitWrite(cardChunk2, i + 13, bitRead(bitHolder1, i));
          }
          if(i < 13) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 36:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 12){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 12) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 10));
          }
          if(i < 10) {
            bitWrite(cardChunk2, i + 14, bitRead(bitHolder1, i));
          }
          if(i < 14) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 37:
       for(int i = 19; i >= 0; i--) {
          if(i == 13){
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 9));
          }
          if(i < 9) {
            bitWrite(cardChunk2, i + 15, bitRead(bitHolder1, i));
          }
          if(i < 15) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;
  }
  return;
}


//////////////////////////////////////
// Begin code for SD card
void writeSD() {
  
    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    
  myFile = SD.open(dataFile, FILE_WRITE);

  // if the file opened okay, write to it:
  if (myFile) {
    myFile.println();
    myFile.print(bitCount);
    myFile.print(" bit card: ");
    myFile.print(cardChunk1, HEX);
    myFile.print(cardChunk2, HEX);
    myFile.print(", FC = ");
    myFile.print(facilityCode, DEC);
    myFile.print(", CC = ");
    myFile.print(cardCode, DEC);
    myFile.print(", BIN: ");
      for (int i = 19; i >= 0; i--) {
        myFile.print(bitRead(cardChunk1, i));
      }
      for (int i = 23; i >= 0; i--) {
        myFile.print(bitRead(cardChunk2, i));
      }
    
    // close the file:
    myFile.close();
    // print to the serial port too
    Serial.println("Wrote data to SD card");
  } 
  else {
    Serial.println("Error writing to file");
  }
}

// End code for SD card
