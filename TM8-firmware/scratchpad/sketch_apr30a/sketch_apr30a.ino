// --------------------------------------
// i2c_scanner
//
// Modified from https://playground.arduino.cc/Main/I2cScanner/
// --------------------------------------

#include <Wire.h>
#include "wiring_private.h" // pinPeripheral() function

TwoWire wire1(&sercom2, 4, 3);

// Set I2C bus to use: Wire, Wire1, etc.
#define Wire1 wire1;

void setup() {
  Wire.begin();
  wire1.begin();
  
  pinPeripheral(4, PIO_SERCOM_ALT);
  pinPeripheral(3, PIO_SERCOM_ALT);
  Serial.begin(9600);
  while (!Serial)
     delay(10);
  Serial.println("\nI2C Scanner");
}


void scan1() {
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for(address = 1; address < 127; address++ ) 
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print("Device found on bus 1. Address 0x");
      if (address<16) 
        Serial.print("0");
      Serial.print(address,HEX);
      Serial.print(".");
      if (address == 0x36) {
        Serial.println(" This is the fuel gauge!");
      } else if (address == 0x38) {
        Serial.println(" This is the left LCD!");
      }

      nDevices++;
    }
    else if (error==4) 
    {
      Serial.print("Unknown error at address 0x");
      if (address<16) 
        Serial.print("0");
      Serial.println(address,HEX);
    }    
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");
}

void scan2() {
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for(address = 1; address < 127; address++ ) 
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    wire1.beginTransmission(address);
    error = wire1.endTransmission();

    if (error == 0)
    {
      Serial.print("Device found on bus 2. Address 0x");
      if (address<16) 
        Serial.print("0");
      Serial.print(address,HEX);
      Serial.print(".");
      if (address == 0x18) {
        Serial.println(" This is the accelerometer!");
      } else if (address == 0x76) {
        Serial.println(" This is the environmental sensor!");
      } else if (address == 0x38) {
        Serial.println(" This is the right LCD!");
      }
      nDevices++;
    }
    else if (error==4) 
    {
      Serial.print("Unknown error at address 0x");
      if (address<16) 
        Serial.print("0");
      Serial.println(address,HEX);
    }    
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");
}

void loop() {
  scan1();
  scan2();
  delay(5000);
}
