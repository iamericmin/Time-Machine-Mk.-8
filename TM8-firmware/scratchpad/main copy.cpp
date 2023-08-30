/*
  ____________  _________   __  ______   ________  _______   ________
 /_  __/  _/  |/  / ____/  /  |/  /   | / ____/ / / /  _/ | / / ____/
  / /  / // /|_/ / __/    / /|_/ / /| |/ /   / /_/ // //  |/ / __/   
 / / _/ // /  / / /___   / /  / / ___ / /___/ __  // // /|  / /___   
/_/ /___/_/  /_/_____/  /_/  /_/_/  |_\____/_/ /_/___/_/ |_/_____/
Time Machine Mk. 8 firmware
Eric Min 2023

*/

#include <Wire.h>
#include "wiring_private.h" // pinPeripheral() function
#include <cdm4101.h>
#include <Adafruit_ASFcore.h>
#include <RTCZero.h>
#include <math.h>

#define NUM_MENU_PROGRAMS 3

// redefine classes into shorter names for easier coding
TwoWire wire1(&sercom2, 4, 3); // set up SERCOMs for I2C

CDM4101 lcd;

RTCZero rtc;

Adafruit_MAX17048 batt;

ArduinoLowPowerClass lowpower;

Adafruit_LIS3DH accel = Adafruit_LIS3DH();

const uint8_t leftR = 26;
const uint8_t leftG = 31;
const uint8_t leftB = 28;
const uint8_t rightR = 25;
const uint8_t rightG = A5;
const uint8_t rightB = 27;

const uint8_t btn1 = 2; // top left button
const uint8_t btn2 = 38;
const uint8_t btn3 = 24;
const uint8_t btn4 = 21;

const uint8_t led1 = 7; // top left LED
const uint8_t led2 = A3; // top left LED
const uint8_t led3 = A1; // top left LED
const uint8_t led4 = 8; // top left LED
const uint8_t led5 = 5; // center LED

const uint8_t rot1 = 13; // rotary bit 1
const uint8_t rot2 = 10; // rotary bit 2
const uint8_t rot4 = 11; // rotary bit 3
const uint8_t rot8 = 12; // rotary bit 4

// set time for RTC
const uint16_t second = 50;
const uint16_t minute = 29;
const uint16_t hour = 12;

const uint8_t cooker[] = {13, 10, 11, 12}; // array for cooker knob

// animations
uint8_t tachInit[7] = {0x00, 0x04, 0x0C, 0x2C, 0x6C, 0x6D, 0x6F};
uint8_t swipeDown[10] = {0x40, 0x61, 0x71, 0x7B, 0x7F, 0x3F, 0x1E, 0x0E, 0x04, 0x00};
uint8_t swirl[6] = {0x40, 0x20, 0x08, 0x04, 0x02, 0x01};

// variables for interrupt callbacks
bool cookerActive = false;
bool fuelActive = false;
bool setTimeActive = false;

uint8_t menuProgramNumber = 0;

// animation mimicking tachometer start-up on vintage cars
void animTach() {
  for (int i=0; i<7; i++) {
    for (int d=0; d<4; d++) {
      lcd.Digits[d] = tachInit[i];
    }
    lcd.Update(0);
    lcd.Update(1);
    delay(80);
  }
  delay(200);
  for (int i=6; i>=0; i--) {
    for (int d=0; d<4; d++) {
      lcd.Digits[d] = tachInit[i];
    }
    lcd.Update(0);
    lcd.Update(1);
    delay(80);
  }
}

// swirl animation on digit 1
void animSwirl(uint8_t times, bool disp) {
  for (int i=0; i<times; i++) {
    for (int i=0; i<6; i++) {
      lcd.Digits[1] = swirl[i];
    }
    lcd.Update(disp);
    delay(30);
  }
}
// "swipe down" animatin
void animSwipeDown(bool disp) {
  for (int i=0; i<10; i++) {
    for (int d=0; d<4; d++) {
      lcd.Digits[d] = swipeDown[i];
    }
    lcd.Update(disp);
    delay(80);
  }
}

// take read 4 bits from cooker knob and convert it into integer from 0-15
uint8_t readCooker() {
  uint8_t rot1 = !digitalRead(13);
  uint8_t rot2 = !digitalRead(10);
  uint8_t rot4 = !digitalRead(11);
  uint8_t rot8 = !digitalRead(12);
  uint8_t cookerVal = rot1 * pow(2, 0) + rot2 * pow(2, 1) + rot4 * pow(2, 2) + rot8 * pow(2, 3);

  return cookerVal;
}

/*
INTERRUPT HANDLERS
-> when interrupt is called, the interrupt callback changes a boolean state variable
and immediately goes back to main loop. Then, an "actual" function handles whatever
the interrupt was called to do. Simply,

Interrupt triggered -> state variable goes from "true" to "false"
-> function does its thing -> back to normal
*/
void showCooker() {
  lcd.dispDec(readCooker(), 0);
}

void showFuel() {
  uint8_t batteryLevel = round(batt.cellPercent());
  if (batteryLevel >= 100) {
    batteryLevel = 99;
  }

  lcd.dispDec(rtc.getSeconds() * 100 + batteryLevel, 1);
  lcd.dispDec(rtc.getHours() * 100 + rtc.getMinutes(), 0);
  delay(500);
}

// interrupt-driven function that shows the cooker value on the left LCD
void cookerInt() {
  cookerActive = true;
}

// shows battery percentage on right LCD
void fuelInt() {
  fuelActive = true;
}

void setTimeInt() {
  setTimeActive = true;
}

void menuInt() {
  menuProgramNumber++;
}

// sets the time
void setTime() {
  uint8_t times[3];

  for (int i=0; i<3; i++) {
    lcd.dispStr("    ", 0);
    lcd.dispStr("    ", 1);
    while (digitalRead(btn1)) {
      showCooker();
    }
    if (!digitalRead(btn1)) {
      times[i] = readCooker();
      lcd.dispStr("set ", 0);
      lcd.dispDec(times[i], 1);
      delay(1000);
    }
  }

  while (digitalRead(btn1) && digitalRead(btn3) == 1) {
    lcd.dispStr("A--P", 1);
  }
  if (!digitalRead(btn1)) {
    lcd.dispStr(" A  ", 0);
  } else if (!digitalRead(btn3)) {
    lcd.dispStr("  P ", 0);
  }
  delay(1000);

  if (times[0] > 12 || times[1] > 5 || times[2] > 9) {
    lcd.dispStr("Err ", 0);
    lcd.dispStr("Err ", 1);
    delay(1000);
  } else {
    rtc.setHours(times[0]);
    rtc.setMinutes(times[1] * 10 + times[2]);
    rtc.setSeconds(0);
    lcd.dispStr("time", 0);
    lcd.dispStr(" set", 1);
  }
}

void blinkGo() {
  for (int i=0; i<5; i++) {
    lcd.dispStr("    ", 1);
    delay(30);
    for (int i=0; i<4; i++) {
      lcd.dispCharRaw(i, 0x54, 1);
    }
    delay(30);
  }
  lcd.dispStr(" GO ", 1);
}

// bootup animation
void bootUpSitRep() {
  animTach();

  uint8_t error;
  uint8_t address;

  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      if (address == 0x36) {
        lcd.dispStr("Fuel", 0);
        blinkGo();
        delay(500);
      } else if (address == 0x38) {
        lcd.dispStr("LCDl", 0);
        blinkGo();
        delay(500);
      }
    }
    
  }


  for(address = 1; address < 127; address++ ) {
    wire1.beginTransmission(address);
    error = wire1.endTransmission();
    if (error == 0) {
      if (address == 0x18) {
        lcd.dispStr("Accl", 0);
        blinkGo();
        delay(500);
      } else if (address == 0x76) {
        lcd.dispStr("temp", 0);
        blinkGo();
        delay(500);
      } else if (address == 0x38) {
        lcd.dispStr("LCDr", 0);
        blinkGo();
        delay(500);
      }
    }
  }
  delay(750);
  lcd.dispStr("", 1);
  lcd.dispStr("All ", 0);
  delay(750);
  lcd.dispStr("Syst", 0);
  lcd.dispStr("ems", 1);
  delay(750);
  lcd.dispStr("", 1);
  for (int i=0; i<7; i++) {
    lcd.dispStr(" GO ", 0);
    delay(75);
    lcd.dispStr("", 0);
    delay(75);
  }
}

uint16_t stopWatch() {

  lcd.dispStr("Chro", 0);
  lcd.dispStr("Chro", 1);
  while (digitalRead(btn3) && digitalRead(btn4));
  if (!digitalRead(btn4)) {
    lcd.dispStr("end", 0);
    return 0;
  } else if (!digitalRead(btn3)) {
    lcd.dispStr("time", 0);
    return 145665;
  }
  delay(1000);
}

void mainMenu() {

}

void setup() {

  lcd.init_lcd();

  Wire.begin();
  wire1.begin();
  
  pinPeripheral(4, PIO_SERCOM_ALT); // setup SERCOM pins for secondary I2C bus
  pinPeripheral(3, PIO_SERCOM_ALT);

  //bootUpSitRep();

  pinMode(btn1, INPUT_PULLUP);
  pinMode(btn2, INPUT_PULLUP);
  pinMode(btn3, INPUT_PULLUP);
  pinMode(btn4, INPUT_PULLUP);

  pinMode(leftR, OUTPUT);
  pinMode(leftG, OUTPUT);
  pinMode(leftB, OUTPUT);
  pinMode(rightR, OUTPUT);
  pinMode(rightG, OUTPUT);
  pinMode(rightB, OUTPUT);

  rtc.begin();

  rtc.setHours(hour);
  rtc.setMinutes(minute);
  rtc.setSeconds(second);

  for (int i=0; i<4; i++) {
    pinMode(cooker[i], INPUT_PULLUP);
  }

  if (!batt.begin()) {
    lcd.dispStr(" no ", 0);
    lcd.dispStr("batt", 1);
    while (1) delay(10);
  }
  
  // attachInterrupt(rot1, cookerInt, CHANGE);
  // attachInterrupt(btn1, fuelInt, LOW);
  // attachInterrupt(btn2, setTimeInt, LOW);
  attachInterrupt(btn3, menuInt, LOW);
  //attachInterrupt(btn4, fuelInt, LOW); // for some reason enabling interrupts on this pin freezes the whole watch

  ADC->CTRLA.bit.ENABLE = 0;
  DAC->CTRLA.bit.ENABLE = 0;

}

void loop() {

  lcd.dispDec(menuProgramNumber, 0);

  // unsigned long currentMillis = millis();
  // unsigned long seconds = currentMillis / 1000;
  // unsigned long minutes = seconds / 60;
  // unsigned long hours = minutes / 60;
  // //unsigned long days = hours / 24;
  // currentMillis %= 1000;
  // seconds %= 60;
  // minutes %= 60;
  // hours %= 24;

  // lcd.dispDec(minutes * 100 + seconds, 0);
  // lcd.dispDec(currentMillis, 1);

  // uint16_t hoursMins = rtc.getHours() * 100 + rtc.getMinutes();
  // uint16_t seconds = rtc.getSeconds();
  // lcd.dispDec(hoursMins, 0);
  // lcd.dispDec(seconds, 1);

  // if (cookerActive) {
  //   for (int i=0; i<1000; i++) {
  //     showCooker();
  //   }
  //   cookerActive = false;
  // }

  // if (fuelActive) {
  //   showFuel();
  //   fuelActive = false;
  // }

  // if (setTimeActive) {
  //   detachInterrupt(btn1); // reading btn1 won't work in setTime() if interrupt is attached.
  //   detachInterrupt(btn3); // same here
  //   setTime();
  //   setTimeActive = false;
  //   attachInterrupt(btn1, fuelInt, LOW);
  //   attachInterrupt(btn3, backlightInt, LOW);
  // }

  // if (setBacklight) {
  //   digitalWrite(leftR, 0);
  //   digitalWrite(rightR, 0);
  //   delay(1000);
  //   digitalWrite(leftR, 1);
  //   digitalWrite(rightR, 1);
  //   setBacklight = false;
  // }

  // lcd.dispStr(" so ", 0);
  // lcd.dispStr("k1gk", 1);

  // LowPower.deepSleep();
}