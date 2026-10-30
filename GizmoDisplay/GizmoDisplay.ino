/*
  GizmoDisplay

  This is driving a set of 8 15-segment LED displays and a single Red LED for the error box.
  Each LED (ACPSA04-41SEKWA) is connected to a latched LED driver (TLC59283RGER) 
  and the data is daisy chained across the display, Left to right.
  https://www.mouser.com/ProductDetail/604-ACPSA04-41SEKWA
  https://www.mouser.com/datasheet/3/308/1/ACPSA04_41SEKWA.pdf
  https://www.digikey.com/en/products/detail/texas-instruments/TLC59283RGER/3523721
  https://www.ti.com/lit/ds/symlink/tlc59283.pdf?HQS=dis-dk-null-digikeymode-dsf-pf-null-wwe&ts=1773952884581
s
  The controller is a Seeed XIAO-ESP32-C6 module.  It has Wifi, Bluetooth, some GPIO and a battery charger built in for a Li-ion battery.
  The Battery is a 18650 rechargeable battery.
  
  A battery monitor (1/2 the battery voltage) is connected to D0/A0 so an analog reading can determine the battery voltage.

  A USB voltage monitor (1/2 the USB connection voltage) is connected to D1/A1 so an analog reading can determine if USB is connected and supplying power.,
  
  A single Red LED is located in the Error box which also has a 150 ohm series resistor attached to the battery voltage.
  GPIO 0 is connected to the Cathode of the LED which will light the LED if driven low.

  The goal is to develope a phone app that can send a message or pattern to be displayed on the Gizmo.
  Currently working on the internal generation and control of the display based on a character string. 


  Single string will be read in, and two special characters will be looked for:
  \n - new line character will separate sets of characters to be displayed
  % - will be used to modify parts of the string and perform actions

    Changes to the string (done in update_string)
    %N .. %NNNNNNN  - replace with a random number from 2 to 8 digits (Number of adjacent N's +1)
    %B .. %BB       - replace with the battery voltage in volts
    %V              - replace with Version number
  
    Immediate Actions (in parse_display_string and ble_support)  If sent as a separate string, does not change the display string
    %1 - turn on the Error Box LED
    %0 - turn off the Error Box LED
    %Sxxxx - update the Speed setting.  How fast the characters scroll.  
              Each "x" must be a digit (0-9) indicating the # of mS between updates to the display (0001 to 9999)
    %Exxxx - update the substring delay/pause.  Eacxh string or substring deliniated by a \n will have a delay after it is displayed.
              Each "x" must be a digit (0-9) indicating the # of mS between updates to the display (0001 to 9999)
    
    Other immidiate Actions done only in parse_display_string
    %Pxxxx - just Pause.  Each "x" must be a digit (0-9) indicating the # of mS to just pause and do not do any updates (0001 to 9999)
    %T - run the test pattern to make sure all segments of each display are working - does use the blocking delay (currently)
    %Z - put the board in deep sleep = must press the reset button to wake it up!

  A string of just " " will blank the display.
  A single string will be continually re-displayed until a new string is received.

  An incoming string of "%Sxxxx" (Speed setting), "%Exxxx" (Substring Pause) will not replace the looping string, 
  so can be used to dynamically adjust the update rate of the displaying string.

  Release Notes:

  V1.0 : xx/xx/2026 Initial version

*/

// Version number
String VERSION = "1.0"; // Must be a 3 character version number

// Includes
#include <SPI.h>
#include <BLEServer.h>

// Using XIAO Pin Mapping (Uses the XIAO pin names)
#define Bat_Monitor  A0 // GPIO0 / D0 Battery Voltage/2
#define USB_Monitor  A1 // GPIO1 / D1 USB voltage/2
#define DRIVER_LAT   D4 // GPIO22 Latch output signal
#define DRIVER_Blank D3 // GPIO21 Blank output signal
#define ErrorBox_LED D5 // GPIO23 Blank output signal
// SPI PINS are the defualt values for the XIAO-ESP32-C6
//#define SPI_CLK  D8 / / GPIO19 SClk out
//#define SPI_MISO D9  // GPIO20 Data in is not connected to anything and not used
//#define SPI_MOSI D10 // GPIO21 Data out is used to control the 15 segment LEDs

// See the following for generating UUIDs:  https://www.uuidgenerator.net/
#define SERVICE_UUID          "9f3bcfd9-4387-42cf-91c0-99c7157407a8" // A Single Service
#define CHARACTERISTIC_UUID_1 "87f9dd84-6e96-49e9-be73-4d7389ee4ed4" // Can have multiple characteristics

// Constant Table for display characters based on the ASCII character set
const uint16_t ASCII_chars [128] = {
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0 - 15 Special characters for display control
0x0080, 0x0200, 0x2000, 0x0800, 0x0002, 0x0010, 0x0012, 0x3400, //  16- 23 Unique symbols "6 outside segments", left pair, right pair
0x0040, 0x0100, 0x0400, 0x1000, 0x0008, 0x0004, 0x0001, 0x0020, //  24- 31 More Unique symbols. 8 spinning singles
// Standard characters
0x0000, 0x4200, 0x0240, 0x064B, 0x2CD9, 0x3535, 0x0449, 0x0040, //  32- 39 " !"#$%&'"
0x1100, 0x0024, 0x156D, 0x0449, 0x0040, 0x0401, 0x4000, 0x0104, //  40- 47 "()*+'-./"
0x2B96, 0x0048, 0x0E83, 0x2E81, 0x2611, 0x2C91, 0x2C93, 0x2280, //  48- 55 "01234567"
0x2E93, 0x2E91, 0x2200, 0x6200, 0x1100, 0x0C01, 0x0024, 0x4688, //  56- 63 "89:;<=>?"
0x0ED2, 0x2693, 0x1993, 0x0892, 0x2AC8, 0x0893, 0x0093, 0x2C92, //  64- 71 "@ABCDEFG"
0x2613, 0x08C8, 0x00C6, 0x1113, 0x0812, 0x2332, 0x3232, 0x2A92, //  72- 79 "HIJKLMNO"
0x0693, 0x7A92, 0x1693, 0x2C91, 0x00C8, 0x2A12, 0x0116, 0x3216, //  80- 87 "PQRSTUVW"
0x1124, 0x0619, 0x0984, 0x0892, 0x1020, 0x2A80, 0x1004, 0x0800, //  88- 95 "XYZ[\]^_"
0x0040, 0x080D, 0x2C13, 0x0C03, 0x2E03, 0x0807, 0x0508, 0x2F00, //  96-103 "'abcdefg"
0x2413, 0x0008, 0x0046, 0x1148, 0x1040, 0x240B, 0x2403, 0x2C03, // 104-111 "hijklmno"
0x0033, 0x3031, 0x0403, 0x1C00, 0x1441, 0x2802, 0x0006, 0x280A, // 112-119 "pqrstuvw"
0x1124, 0x2E40, 0x0805, 0x1101, 0x0048, 0x0424, 0x0130, 0x0000  // 120-127 "Xyz{|}~ "
};

const char SPECIAL = '%'; // Used to define the special character that is not displayed and is for identifying codes
// This needs to work in the app and the '\t' was not easy to manually enter, so went with a character that did not display well!

// Globals for use by anyone...
bool debug = false; // indicates if we should output any serial data

// BLE connection use
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false; // flag to show what the current status is from the callback
bool previousDeviceConnected = false; // last value of device connected, so we can tell when it changes

// data from app
bool new_string_rcvd = false; // used to indicate a new string has been received
String app2xaio = " "; // return string from the APP used for message display
bool new_input_string = false; // start by processing the default string

// using the evil string object...
String input_string = " "; // just a blank screen if not overwritten.  Copied from app2xaio after received but only at the start of the main loop.
String display_string = " "; // just a blank screen if not overwritten. Copied from input string at the start of the main loop and used for displaying.

// Global Veriables for display
uint16_t Str_length;  // used to know if we are in-bounds as a check
uint16_t Delay_ms = 200; // starting value for delay in mS
uint16_t EndOfSubstring_ms = 1000; // delay between substrings
uint16_t Pause_ms =   0; // pause requested in ms


// BLE support classes

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      if (debug) Serial.println("BLEServerCallbacks, Device just connected. 8-)");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      if (debug) Serial.println("BLEServerCallbacks, Device just disconnected. 8-(");
    }
};

class CharacteristicsCallBack: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pString) override {
    app2xaio = pString->getValue();
    new_string_rcvd = true;
    if (debug) Serial.println("BLECharacteristicCallbacks, app2xaio: " + app2xaio + "  Length = " + app2xaio.length());
  }
};


void ble_initialize() {
  // Create the BLE Device, the name has 8 character max!  (additional charcaters are ignored?!)
  BLEDevice::init("Gizmo42");

  // Create the BLE Server
  pServer = BLEDevice::createServer(); // XIAO is going to be a Server!
  pServer->setCallbacks(new MyServerCallbacks());  // hook in the simple callbacks so we know when we are connected

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic for receiveing data from the APP (client)
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_1,
//    BLECharacteristic::PROPERTY_READ |
//    BLECharacteristic::PROPERTY_WRITE |
//    BLECharacteristic::PROPERTY_NOTIFY |
//    BLECharacteristic::PROPERTY_INDICATE
//    BLECharacteristic::PROPERTY_NOTIFY
    BLECharacteristic::PROPERTY_WRITE 
  );                   

  // Create BLE Descriptor
    BLEDescriptor *pDescr;  
    pDescr = new BLEDescriptor((uint16_t)0x2901);
    pDescr->setValue("Receive a single string to be parsed and then displayed on the Gizmo display."); // Does not appear to have the 20 character limitation...
    pCharacteristic->addDescriptor(pDescr);
    pCharacteristic->setCallbacks(new CharacteristicsCallBack());

  // Start the service
  pService->start();

  // Start advertising so we can be discovered
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID); // needed to have the service discovered by others
  pAdvertising->setScanResponse(false); // no additional data to be sent
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter ???
  //pAdvertising->setAdvertisingInterval(uint16_t); // set an advertising rate
  BLEDevice::startAdvertising();
  if (debug) Serial.println("ble_initialize, Start advertising");
}


// Check for a 6 character string to see if it is a valid request for a dynamic ms time update
// must be 6 characters and start with a "%S" or a "%E"
bool ms_delay_check (String potential_request) {
  bool ms_found = false; // start with a default of not found
  uint16_t temp_ms = 0; // potential ms value
  String temp_string = "";
//  if ( (debug) && (potential_request.charAt(0) == SPECIAL) ) Serial.println("ms_delay_check, found \t in first position ");
  if ( (potential_request.charAt(0) == SPECIAL) && (potential_request.length() == 6) ) {
    temp_string = potential_request.substring(2,6); // grab the expected 4 digit decimal value
    temp_ms = temp_string.toInt(); // convert to a integer, returns 0 if it can't
//    if (debug) {
//      Serial.print("ms_delay_check, temp_ms "); // OMG?!?!?!?  If I use "+" in this debug statement everything is screwed up after it
//      Serial.println(temp_ms);
//    }
    if (temp_ms > 0) {
      switch (potential_request.charAt(1)) {
        case 'S': // Speed setting
          Delay_ms = temp_ms; // uipdate a GLOBAL variable
          if (debug) {
            Serial.print("ms_delay_check, Delay_ms: ");
            Serial.println(Delay_ms);
          }
          ms_found = true; // We Got One!
          break;
        case 'E': // End of substring delay
          EndOfSubstring_ms = temp_ms; // uipdate a GLOBAL variable
          if (debug) {
            Serial.print("ms_delay_check, EndOfSubstring_ms: ");
            Serial.println(EndOfSubstring_ms);
          }
          ms_found = true; // We Got One !
          break;
        default: // something not yet implemented?
          break;
      } // switch
    } // if
  } // if
  return (ms_found);
} // ms_delay_check


// ble processes that need to be checked occasionally
// Also checks for a couple of short commands and can perform the request but not update the display string
void ble_support() {
  // conncted
  if (deviceConnected) {
    // check for a new string and also decide if it needs to be displayed or is only adjust some display rate(s)
    if (new_string_rcvd) {
      // only look once for each received string
      new_string_rcvd = false; 
      // and the string must have some length to be interesting
      if (app2xaio.length()>0) { // we have a string to look at!
        new_input_string = true; // good chance we want to update the input string
        if (debug) Serial.println("ble_support, app2xaio: " + app2xaio + "  Length = " + app2xaio.length());

        // look for a delay only which won't change the string that is currently being displayed
        if (app2xaio.length()==6) {
          if (ms_delay_check(app2xaio.substring(0,6))) { // Found and performed a valid time
            new_input_string = false; // no need to update the input_string
          }
        } else if (app2xaio.length()==12) {
          if ( (ms_delay_check(app2xaio.substring(0,6))) && (ms_delay_check(app2xaio.substring(6,12))) ) { // Found and performed two valid time updates
            new_input_string = false; // no need to update the input_string
          }          
        } 

        // look for an error box LED update
        if (app2xaio.length()==2) {
          if ( (app2xaio.charAt(0) == SPECIAL) && ( (app2xaio.charAt(1) == '0') || (app2xaio.charAt(1) == '1') ) ) { // Found a single request for error LED off
            update_errorbox(app2xaio.charAt(1));
            new_input_string = false; // no need to update the input_string
          }
        } 

      }
    }
  }

  // disconnecting
  if (!deviceConnected && previousDeviceConnected) {
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    if (debug) Serial.println("start advertising again");
    previousDeviceConnected = deviceConnected;
  }
 
  // connecting
  if (deviceConnected && !previousDeviceConnected) {
    // do stuff here on connecting
    previousDeviceConnected = deviceConnected;
  }
}


void test() {  
  uint8_t ASCII_index = 16; // the starting symbol to use for this test pattern
  uint16_t Single_LED; // Data for a single LED
  int i, j, k; // used for a couple of loops

  for (i=0; i<16; i++) {
    // turn off the display
    digitalWrite(DRIVER_Blank, HIGH);  // turn the display off
    Single_LED = 1<<i;
    for (j=0;j<=7;j++) {
      // Start communication with the device
      SPI.transfer(highByte(Single_LED));  // Send a command to the device
      //data = SPI.transfer(0);  // Read the response
      SPI.transfer(lowByte(Single_LED));  // Send a command to the device
      //data = SPI.transfer(0);  // Read the response
      // strobe the latch signal
      digitalWrite(DRIVER_LAT, HIGH);  // start the strobe
      digitalWrite(DRIVER_LAT, HIGH);  // make it longer?
      digitalWrite(DRIVER_LAT,  LOW);  // end the strobe
      digitalWrite(DRIVER_LAT,  LOW);  // end the strobe
      }
    // turn on the display
    digitalWrite(DRIVER_Blank, LOW);  // turn the display on
    delay(500); // delay a bit and repeat!
  }
  for (i=0; i<60; i++) {
    // turn off the display
    digitalWrite(DRIVER_Blank, HIGH);  // turn the display off
    for (k=0;k<=7;k++) {
      Single_LED = ASCII_chars[ASCII_index + (i+k)%6];
      // Start communication with the device
      SPI.transfer(highByte(Single_LED));  // Send a command to the device
      //data = SPI.transfer(0);  // Read the response
      SPI.transfer(lowByte(Single_LED));  // Send a command to the device
      //data = SPI.transfer(0);  // Read the response
      // strobe the latch signal
      digitalWrite(DRIVER_LAT, HIGH);  // start the strobe
      digitalWrite(DRIVER_LAT, HIGH);  // make it longer?
      digitalWrite(DRIVER_LAT,  LOW);  // end the strobe
      digitalWrite(DRIVER_LAT,  LOW);  // end the strobe
    }
  // turn on the display
  digitalWrite(DRIVER_Blank, LOW);  // turn the display on
  if (i==0) update_errorbox ('1'); // turn on the error box LED
  else if (i==30) update_errorbox ('0'); // turn off the error box LED
  delay(80); // delay a bit and repeat!
  }
}


void update_display (int StartIndex, int StrLength)
{
  if (debug) {
    Serial.print("update_display: StartIndex ");
    Serial.print(StartIndex);
    Serial.print("  StrLength ");
    Serial.println(StrLength);
  }
  // turn off the display
  digitalWrite(DRIVER_Blank, HIGH);  // turn the display off

  // update all 8 digit drivers
  for (int i=7;i>=0;i--) {
      // Start communication with the device
      if (i>StrLength) {
        SPI.transfer(highByte(ASCII_chars[' ']));  // Send a command to the device
        SPI.transfer(lowByte(ASCII_chars[' ']));  // Send a command to the device
      } else if ((display_string.charAt(i+StartIndex)) > 128) {
        SPI.transfer(64+highByte(ASCII_chars[127 & display_string.charAt(i+StartIndex)]));  // Send a command to the device but include the decimal
        SPI.transfer(lowByte(ASCII_chars[127 & display_string.charAt(i+StartIndex)]));  // Send a command to the device
      } else {
        SPI.transfer(highByte(ASCII_chars[display_string.charAt(i+StartIndex)]));  // Send a command to the device
        SPI.transfer(lowByte(ASCII_chars[display_string.charAt(i+StartIndex)]));  // Send a command to the device
      }
      // strobe the latch signal
      digitalWrite(DRIVER_LAT, HIGH);  // start the strobe
      digitalWrite(DRIVER_LAT, HIGH);  // make it longer?
      digitalWrite(DRIVER_LAT,  LOW);  // end the strobe
      digitalWrite(DRIVER_LAT,  LOW);  // end the strobe
    }
    // turn the display back on
    digitalWrite(DRIVER_Blank, LOW);  // turn the display on
}


// turn the error box LED on ('1') or off (anything else)
void update_errorbox (char LED_control) {
  if (LED_control == '1')
    digitalWrite(ErrorBox_LED,  LOW);  // turn the error box LED on
  else
    digitalWrite(ErrorBox_LED, HIGH);  // turn the error box LED off
}


// Returns the USB connection 5V voltage in 10 mv value - returns 500 for a 5000 mV measuremnent
int measure_usb5v () {
  uint32_t Vusb = 0;
  int vusbf = 0;
  for(int i = 0; i < 16; i++) {
    Vusb += analogReadMilliVolts(A1); // Read and accumulate ADC voltage
  }
  vusbf = ((2 * Vusb / 16) + 5) /10; // Adjust for 1:2 divider and convert to volts with 2 decimal places
  if (debug) {
    Serial.print("USB measured as "); // Output voltage to 3 decimal places
    Serial.print(vusbf); // Output voltage to 3 decimal places
    Serial.println("0 mV"); // show as 4 decimal places and proper label 
  }
  return (vusbf); // 10mV value typically 370 or so for fully charged battery that is at 3.7V
}


// Returns the battery voltage in 10 mv value - returns 370 for a 3700 mV measuremnent
int measure_battery () {
  uint32_t Vbatt = 0;
  int vbattf = 0;
  for(int i = 0; i < 16; i++) {
    Vbatt += analogReadMilliVolts(A0); // Read and accumulate ADC voltage
  }
  vbattf = ((2 * Vbatt / 16) + 5) /10; // Adjust for 1:2 divider and convert to volts with 2 decimal places
  if (debug) {
    Serial.print("Battery measured as "); // Output voltage to 3 decimal places
    Serial.print(vbattf); // Output voltage to 3 decimal places
    Serial.println("0 mV"); // show as 4 decimal places and proper label 
  }  
  return (vbattf); // 10mV value typically 370 or so for fully charged battery that is at 3.7V
}


// This routine will update the display_string by putting in random numbers, the battery voltage 
// and turning on the decimal point on previous characters.
void update_display_string() {
  int vbat = 0;
  // Search for random number indicator '%N' anywhere in the string and replace with random (0-9) numbers
  for (int i=0; i<Str_length; i++) {
    if ( (display_string.charAt(i)==SPECIAL) && (display_string.charAt(i+1)=='N') ) {
      // first replace the tab
      display_string.setCharAt(i,'0'+random(0,9));
      // up to 7 more replacements
      for (int j=1; j<=7; i++) {
            if ( (display_string.charAt(i+j) == 'N')  && ( (i+j) < Str_length ) ) { 
              display_string.setCharAt(i+j,'0'+random(0,9));
            } else {
              j=8; // done with this loop
            }
      } // for j
    } // if
  } // for i
  // Search for a battery voltage '%B' and if found replace the B's with the voltage
  for (int i=0; i<Str_length; i++) {
    if ( (display_string.charAt(i)==SPECIAL) && (display_string.charAt(i+1)=='B') ) {
      // Get a new battery value
      vbat = measure_battery();  // measure the battery
      // first replace the "%B"
      display_string.setCharAt(i,char(128 | ('0'+ ((vbat/100) % 10) )) );  // "%", and add a decimal point
      display_string.setCharAt(i+1,'0'+((vbat/10) % 10));  // first B
      if ( (display_string.charAt(i+2) == 'B')  && ( (i+1) < Str_length ) ) {
        display_string.setCharAt(i+2,'0'+(vbat % 10)); // Second B
      }
    }
  // Search for a Version number request
  for (int i=0; i<Str_length; i++) {
    if ( (display_string.charAt(i)==SPECIAL) && (display_string.charAt(i+1)=='V') ) {
      // replace the "%V"
      display_string.setCharAt(i  ,char(128 | VERSION.charAt(0)) );
      display_string.setCharAt(i+1,VERSION.charAt(2));
      }
    }
  }    
  // Search for a decimal point request for the previous character
  // Must be a previous character, so can start looking at the second character
  for (int i=1; i<Str_length; i++) {
    if ( (display_string.charAt(i)==SPECIAL) && (display_string.charAt(i+1)=='D') && (i>0) ) {
      // Update the character to indicate the decimal point should be illuminated
      display_string.setCharAt( i-1, char(128 | display_string.charAt(i-1)) );  // add a decimal point to the previous character
      // And shorten the string by removing the "%D"
      display_string = display_string.substring(0,i) + display_string.substring(i+2);
      Str_length = display_string.length();
    }
    
  }
} // update_string


#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  10          // Time ESP32 will go to sleep (in seconds)

void deep_sleep(bool wake_up_later){
  // Blank the display
  display_string = "DEEP"; // Wait 1
  Str_length = display_string.length();  
  update_display(0,4);
  delay_check(1000);  // also runs ble_support
  display_string = "SLEEP"; // Wait 2
  Str_length = display_string.length();  
  update_display(0,5);
  delay_check(3000);  // also runs ble_support
  display_string = " "; // blank the display
  Str_length = display_string.length();
  update_display(0,1);
  if (wake_up_later) {
    // set up a timer to wake up
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.println("Going to sleep now...");
    delay(100); // Allow serial buffer to empty
    //Enter deep sleep
  } else {
    display_string = "Z"; // Wait 1
    Str_length = display_string.length();  
    update_display(0,1);
    delay_check(4000);  // also runs ble_support

    display_string = "Zzz"; // Wait 2
    Str_length = display_string.length();  
    update_display(0,3);
    delay_check(4000);  // also runs ble_support

    display_string = "Zzzzz"; // Wait 3
    Str_length = display_string.length();  
    update_display(0,5);
    delay_check(4000);  // also runs ble_support

    display_string = " "; // blank the display
    Str_length = display_string.length();  
    update_display(0,1);
  }
  // Time to go to sleep
  esp_deep_sleep_start();
  // The timer can make this reboot - not a very polite reccovery
}

// Parse_display_string
// Look for control characters for some immediate actions that do not affect the display
// And if not found, count how many character until a new line \n character is found
// Returns a negative number if some special characters were found (and characters should be skipped)
// Returns a positive number indicating how many characters there are until a new line \n
int parse_display_string(int current_index) {
  int search_length=0; // initialize the length indicator
  int decimal_place=0; // location of the decimal point to be added
  String temp_string; // used to convert to a ms value

  // first look for tab character special cases
  if (display_string.charAt(current_index)==SPECIAL) {
    if (debug) Serial.println("Found a %");
    switch (display_string.charAt(current_index+1)) {
        case '1': // Turn on the error box LED
        case '0': // Turn off the error box LED
          update_errorbox (display_string.charAt(current_index+1));
          search_length = -2; // remove two characters from the search string, but nothing to display
          if (debug) {
            Serial.print("Error Box ");
            Serial.println(display_string.charAt(current_index+1));
          }
          break;
        case 'S': // Update the Speed delay for scrolling
        case 'E': // Update the End of substring delay
        case 'P': // Pause now (number of ms)
          // Needs to be 4 digits (0-9) following this character 
          temp_string = display_string.substring(current_index+2, current_index+6); // grab the expected 4 digit decimal value
          if (temp_string.toInt() > 0) {
            if (display_string.charAt(current_index+1) == 'S') { // Speed setting of 1 to 9999 mS per display
              Delay_ms = temp_string.toInt(); // uipdate a GLOBAL variable
              search_length = -6; // remove 5 characters from the search string, but nothing to display  
              if (debug) {
                Serial.print("Delay_ms ");
                Serial.println(Delay_ms);
              } 
            } else if (display_string.charAt(current_index+1) == 'E') { // Speed setting of 1 to 9999 mS per display
              EndOfSubstring_ms = temp_string.toInt(); // uipdate a GLOBAL variable
              search_length = -6; // remove 5 characters from the search string, but nothing to display  
              if (debug) {
                Serial.print("EndOfSubstring_ms ");
                Serial.println(EndOfSubstring_ms);
              }
            } else {
              Pause_ms = temp_string.toInt(); // uipdate a GLOBAL variable
              delay_check(Pause_ms); // perform the pause and also runs ble_support
              search_length = -6; // remove 5 characters from the search string, but nothing to display  
              if (debug) {
                Serial.print("Pause_ms ");
                Serial.println(Pause_ms);
              }
            }

          } else {
            search_length = -1; // just remove the tab - the rest will be displayed
          }
          break;
        case 'T': // Run a full test - not very polite to bluetooth, so use sparingly!!!
          test(); // perform a test sequence
          search_length = -2; // remove 2 characters, the % and T
          break;
        case 'Z': // Go to deep sleep to help charge the battery
          deep_sleep(false); // perform a test sequence
                            //   true - wake up after a short while - mostly for testing
                            //   false - need to power cycle to recover
          // should not need anything after this since it can only reboot
          search_length = -2; // remove 2 characters, the % and T
          break;
        default: // something not yet implemented?
          search_length = -1; // just remove the tab - the rest will be displayed
          break;
    }
  } else {
    if (search_length==0) { // look for a full substring
      // look for a newline character to indicate a subsection of the string to display
      while ( (Str_length-current_index >0) && ((current_index + search_length) < Str_length)) {
        // look for a newline character to indicate a subsection of the string to display
        if (display_string.charAt(current_index + search_length)=='\n') {
          // found one!
          if (debug) {
            Serial.print("search_length ");
            Serial.println(search_length);
          }
          break; // done looking
        } else search_length++;
      }
      if (debug) {
        Serial.print("parse_display_string ");
        Serial.println(search_length);
      }
    }
  }
  return search_length;
}


// Charching_check
// Look for a voltage lower than 3.0V and if so add a warning message
// Then return to whatever was displaying
void charging_check() {
  int blinks; // number of blinks to diaplay while charging
  // Is the battery so low that we should issue a warning?
  int vbat = measure_usb5v();  // measure the USB 5V
  if (vbat >= 300) {

    // turn off Bluetooth?

    // Blank the display
    display_string = " "; // blank the display
    Str_length = display_string.length();
    update_display(0,1);
    // wait a bit
    delay(1000);  // no ble_support

    // sit here until the cable is unplugged 
    while (measure_usb5v() >= 300) {
      // occasionally blink the error LED to let us know it is charging
      // number of blinks depends on the battery voltage
      vbat = measure_battery();  // measure the USB 5V
      blinks = (vbat+10)/100; // number of blinks indicate the battery voltage
      // added 10 so 3.90V would indicate a full charge (4V?)  some batteries did not quite go to 4V
      for (int i = 0; i< blinks; i++) {
        update_errorbox (char('1')); // LED on
        delay(100);  // no ble_support
        update_errorbox (char('0')); // LED off
        delay(400);  // no ble_support
      }
      delay(3500);  // no ble_support
    }  

  // Turn back on Bluetooth?
  
  } // if 

} // charging_check


// Battery_check
// Look for a voltage lower than 3.0V and if so add a warning message
// Then return to whatever was displaying
void battery_check() {
  static uint8_t battery_skip_count = 0; // used to control how often the warning is displayed
  // Is the battery so low that we should issue a warning?
  int vbat = measure_battery();  // measure the battery
  if (vbat <= 300) {
    // Decide if we need to show the battery low warning message this time
    if (   (vbat <= 280) ||
         ( (vbat<290) && (battery_skip_count > 3) ) ||
         ( (vbat<300) && (battery_skip_count > 5) )
       ) {  // time to display !
      // Time to display, so reset the skip counter
      battery_skip_count = 0;
      // Blank the display
      display_string = " "; // blank the display
      Str_length = display_string.length();
      update_display(0,1);
      // Wait a bit
      delay_check(2000);  // also runs ble_support
      // Strobe the Error LED
      for (int i=0; i<41; i++) {
        update_errorbox (char(i%2 + '0'));
        delay_check(100);  // also runs ble_support
      }
      display_string = "Low Bat "; // blank the display
      Str_length = display_string.length();
      update_display(0,8);
      delay_check(2000);  // also runs ble_support
      // Blank the display
      display_string = " "; // blank the display
      Str_length = display_string.length();
      update_display(0,1);
      // Strobe the Error LED
      for (int i=0; i<41; i++) {
        update_errorbox (char(i%2 + '0'));
        delay_check(100);  // also runs ble_support
      } 
      // wait a bit
      delay_check(4000);  // also runs ble_support
    }
    battery_skip_count++;
  } else {
    battery_skip_count = 0; // reset the skip counter
  }
} // battery_check


void delay_check(int wait_ms) {
  // Replace the blocking delay with a mS timer polling and incoming string detection watch
  unsigned long initial_millis;
  int delta_millis = 0; // amount of time since the initial readingh
  
  // read in the current ms time
  initial_millis = millis();

  // the waiting loop  
  while (delta_millis < wait_ms) {
    delta_millis = millis() - initial_millis;  // normally a number less than 32767 (but should be less than 1000)
    if (delta_millis < 0) { // a wrap around has occured, so fix the compare time
      delta_millis += 65536; // get it back to a reasonable number
    }
    ble_support(); // keep looking for Bluetooth BLE activity
  }
  //delay(wait_ms); // temp holder before the polling was added
}


// the setup function runs once when you press reset or power the board
void setup()
{
    // Set up the startup state for the Blank and Lat signal connections to the driver
  pinMode(DRIVER_Blank, OUTPUT); // overall display control
  digitalWrite(DRIVER_Blank, HIGH); // turn off the display
  pinMode(DRIVER_LAT, OUTPUT); // Latch signal, strobes high to do something
  digitalWrite(DRIVER_LAT, LOW);
  // initialize Serial debug message port
  Serial.begin(115200);
  // initialize the SPI communications 
  SPI.begin();
  // Set up the two inputs (maybe not needed as they defualt to inputs?)
  pinMode(A0, INPUT); // battery voltage/2
  pinMode(A1, INPUT); // USB voltage/2
  // set up the error box LED
  pinMode(ErrorBox_LED, OUTPUT);
  digitalWrite(ErrorBox_LED, HIGH); // turn off the Error Box LED
  // setup Bluetooth BLE
  ble_initialize(); // setup ble 
  //setup the default string to display on powerup 
  //input_string = "       Who You Gonna Call ?        \n  GHOST\nBUSTERS!\n "; 
  //input_string = "       Who You Gonna Call ?        \n  GHOST\nBUSTERS!\n \nBAT %BBV\n "; 
  //input_string = "       Who You Gonna Call ?        \n  GHOST\nBUSTERS!\n \n555-2368\n%P2000        Who You Gonna Call ?        \n  GHOST\nBUSTERS!\n \nJL5-2020\n%P2000 \nBAT %BBV\n "; // default if the incoming app2xaio is not one to display
  //input_string = "BAT %BBV"; // default if the incoming app2xaio is not one to display
  //input_string = "%NNN \n Hi %NN\n       BAT %BBV        "; // default if the incoming app2xaio is not one to display
  //input_string = "%1  Hi  \n  Bye \n%0Three\n%1Four\n%0Five\n%1Six\n%0"; // default if the incoming app2xaio is not one to display
  //input_string = "%1  Hi  \n%0  Bye "; // default if the incoming app2xaio is not one to display
  //input_string = "  Hi  \n%P4000  Bye "; // default if the incoming app2xaio is not one to display
  //input_string = "%NNNNN"; // random numbers
  //input_string = "12345678"; // default if the incoming app2xaio is not one to display
  //input_string = "0OH%D123%D"; // default if the incoming app2xaio is not one to display
  //input_string = "       Firmware Version   V%V  "; // default if the incoming app2xaio is not one to display
  //input_string = "%T\n       Test Complete\n \n%BBV BAT\n%P2000 "; // default if the incoming app2xaio is not one to display
  //input_string = "%E0050%0\n%1\n%0\n%1\n%0\n%1\n%0\n%1";
  //input_string = "%BB";
  //input_string = "%BB\n%P9999W1\n%P9999W2\n%P9999W3\nZzzzzz\n%P5000%Z";
  //input_string = "@ABCDEFG";
  //input_string = "HIJKLMNO";
  //input_string = "PQRSTUVW";
  //input_string = "XYZ[\\]^_";
  //input_string = "'abcdefg";
  //input_string = "hijklmno";
  //input_string = "pqrstuvw";
  //input_string = "xyz{|}~ ";
  //input_string = " !\"#$%&'";
  //input_string = "()*+'-./";
  //input_string = "01234567";
  //input_string = "89:;<=>?";
  //input_string = "%0%BBB\n%1%BBB\n";
  //input_string = " ";  // blank

  // Cap
  //input_string = "       We're ready to believe you        \n  GHOST\nBUSTERS!\n \n555-2368\n%P2000        Who You Gonna Call ?        \n  GHOST\nBUSTERS!\n \nJL5-2020\n%P2000 "; // Requested
  // Hugh
  //input_string = "       Who You Gonna Call ?        \n  GHOST\nBUSTERS!\n \n555-2368\n%P3000 \n%P4000       Bustin' Makes Me Feel Good!        \n%P4000       I Ain't AFraid OF No Ghosts        \n%P4000 We Came\n We Saw\n        We Kicked It's Ass        \n%P4000       Switch Me On        \n%P4000       He Slimed Me        \n%P4000       We Got One!        \n%P4000       No Job Is Too BIG - No Fee Is Too BIG        \n%P4000       That's A BIG Twinkie        \n%P4000       When Someone Asks IF You're A God - You Say\n  YES!  \n%P4000       I Love This Town!!!        \n%P4000 Doeeee\n Raayyyy\n Egonnnn\n%P2000 \n%P4000";
  // Matt
  //input_string = "       Greenstream        \n%P2000";
  //input_string = "       THE GREENSTREAM GHOSTBUSTERS        \n%P2000";
  //
  //input_string = "       Ghostbusters - We're Ready to Beleive You!        \n  \n  GHOST\nBUSTERS!\n \n       We're Ready to Beleive You!        \n ";
  //
  //input_string = " \n%E0200\n%1WARNING\n%0 \n%1WARNING\n%0 \n%1WARNING\n%0 \n%1WARNING\n%0 \n%1WARNING\n%0 \n                  PKE ALERT!            \n \n%1WARNING\n%0  \n%1WARNING\n%0 \n%1WARNING\n%0  \n%1WARNING\n%0 \n%1WARNING\n%0 \n%E1000%P3000        Who You Gonna Call ?        \n  GHOST\nBUSTERS!\n \n555-2368\n%P4000 \n";
  //default test display string if no one asks for a specific sequence
  input_string = "       Who You Gonna Call ?        \n  GHOST\nBUSTERS!\n \n555-2368\n%P2000        Who You Gonna Call ?        \n  GHOST\nBUSTERS!\n \nJL5-2020\n%P2000 \nBAT %BBV\n "; // default if the incoming app2xaio is not one to display
}

// the loop function runs over and over again forever
void loop() {
  int CurrentStart = 0; // current string staring position
  int CurrentLength;

   // allow ble to update the string to be displayed
  ble_support(); // keep looking for Bluetooth BLE activity

  // update the input_string if a new string has been received from bluetooth
  if (new_input_string) {
    new_input_string = false; // only update once at this point in the loop
    input_string = app2xaio; // pass the incoming string to the string that will be displayed
    if (debug) {
      Serial.print("main loop,  new string, input_string = {");
      Serial.print(input_string + "}  ");
      Serial.print(" length = ");
      Serial.println(input_string.length());
    }
  }
  // reening check and 
  charging_check(); // look at the 5V feedback to see if we are plugged into a USB cable providing power
                    // Display will be off since we do not want to use that power for display

  // Battery check and possible warning message
  battery_check(); // If too low, blank the display for a while and flash the ERROR box LED

  // Start each loop with a copy of the input_string
  display_string = input_string;
  Str_length = display_string.length();
  update_display_string(); // Replace random number placeholders, battery voltage and decimal places
  
  while (CurrentStart < Str_length) {
    // look for things in the string that do not go to the display, process and then skip over them
    CurrentLength = parse_display_string(CurrentStart); // parse string staring at i
    while (CurrentLength < 0) {
      CurrentStart = CurrentStart - CurrentLength; // add in the number of characters to skip - already processed
      CurrentLength = parse_display_string(CurrentStart); // parse string staring at i
    }
    
    // display one line (substring) and if longer than 8 characters scroll the display
    for (int i = CurrentStart; i<=min( max(CurrentStart,CurrentStart+CurrentLength-8),max(CurrentStart,Str_length-8) ); i++) {
      update_display(i,CurrentLength-1);
      // delay, but keep checking for incoming strings
      if (i > CurrentStart)
        delay_check(Delay_ms); // the scrolling delay
    }
    
    CurrentStart += (CurrentLength + 1); // account for the /n character separating each substring

    //add a short pause between substrings
    delay_check(EndOfSubstring_ms);  // also runs ble_support

  } // While there are still characters to work on in the string

} // loop