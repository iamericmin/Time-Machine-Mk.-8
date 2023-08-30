// --------------------------------------
// i2c_scanner
//
// Modified from https://playground.arduino.cc/Main/I2cScanner/
// --------------------------------------

#include <Wire.h>
#include "wiring_private.h" // pinPeripheral() function
#include <cdm4101.h>
#include <Arduino.h> // need to replace this later with ASF

TwoWire wire1(&sercom2, 4, 3); // set up SERCOMs for I2C

CDM4101 lcd;

const uint8_t btn = 2; // top left button

void setup() {
  Wire.begin();
  wire1.begin();

  Serial.begin(9600);
  
  pinPeripheral(4, PIO_SERCOM_ALT); // setup SERCOM pins for secondary I2C bus
  pinPeripheral(3, PIO_SERCOM_ALT);

  pinMode(btn, INPUT_PULLUP);

  lcd.dispStr("12c", 0); // 1 instead of I for higher "I" character
  lcd.dispStr("scan", 1);
  while(digitalRead(btn) == 1) {
  }

}

void waitForPress() { // pauses program until BTN1 is pressed
  while(digitalRead(btn) == 1) {
  }
}

void scan1() {
  uint8_t error, address;
  int nDevices;

  lcd.dispStr("bus1", 0);
  lcd.dispStr("scan", 1);
  waitForPress();

  nDevices = 0;
  for(address = 1; address < 127; address++ ) // look for devices on I2C bus 1
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      if (address == 0x36) { // if MAX17048 fuel gauge IC is detected on I2C bus 1
        lcd.dispStr("Fuel", 0);
        lcd.dispStr("  36", 1);
        delay(2000);
      } if (address == 0x38) { // if left LCD is detected
        lcd.dispStr("LCDL", 0);
        lcd.dispStr("  38", 1);
        delay(2000);
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
  if (nDevices == 0) {
    lcd.dispStr("None", 0);
    lcd.dispStr("Nada", 1);
  } else {
    lcd.dispStr("bus1", 0);
    while(digitalRead(btn) == 1) {
      lcd.dispStr("scan", 1);
      delay(500);
      lcd.dispStr("done", 1);
      delay(500);
    }
  }
}

void scan2() {
  uint8_t error, address;
  int nDevices;

  lcd.dispStr("bus2", 0);
  lcd.dispStr("scan", 1);
  waitForPress();

  nDevices = 0;
  for(address = 1; address < 127; address++ ) // look for devices on I2C bus 2
  {
    wire1.beginTransmission(address);
    error = wire1.endTransmission();

    if (error == 0)
    {
      if (address == 0x18) { // if LIS3DH accelerometer is detected on I2C bus 2
        lcd.dispStr("Accl", 0);
        lcd.dispStr("  18", 1);
        delay(2000);
      } if (address == 0x38) { // if right LCD is detected
        lcd.dispStr("LCDr", 0);
        lcd.dispStr("  38", 1);
        delay(2000);
      } if (address == 0x76) { // if BME680 temp sensor is detected
        lcd.dispStr("temp", 0);
        lcd.dispStr("  76", 1);
        delay(2000);
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
  if (nDevices == 0) {
    lcd.dispStr("None", 0);
    lcd.dispStr("Nada", 1);
  } else {
    lcd.dispStr("bus2", 0);
    while(digitalRead(btn) == 1) {
      lcd.dispStr("scan", 1);
      delay(500);
      lcd.dispStr("done", 1);
      delay(500);
    }
  }
}

void loop() { // loop between scanning bus 1 and bus 2
  scan1();
  delay(1000);
  scan2();
}