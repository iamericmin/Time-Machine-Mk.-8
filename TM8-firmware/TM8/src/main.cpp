#include <Arduino.h>
#include <Wire.h>
#include "wiring_private.h" // pinPeripheral() function
#include <cdm4101.h>
#include <stdio.h>
#include <RTCZero.h>
#include <Adafruit_MAX1704X.h>
#include <LowPower.h>
#include <SparkFunLIS3DH.h>
#include <bme68xLibrary.h>

#define INACTIVITY_TIMEOUT 2000 // inactivity threshold of 2 seconds
#define BUTTON_DELAY 200 // delay between button readings for switching between programs

TwoWire wire1(&sercom2, 4, 3); // new Wire object to set up second I2C port on SERCOM 2

CDM4101 lcd; // LCD object

RTCZero rtc; // RTC object

Adafruit_MAX17048 fuel;

LowPowerClass lowpower;

LIS3DH accel(I2C_MODE, 0x18);

Bme68x bme;

const uint8_t btn1 = 2; // top left button
const uint8_t btn2 = 38; // bottom left button
const uint8_t btn3 = 24; // top right button

const uint8_t led5 = 5; // LED5, middle auxilliary LED

uint8_t leds[] = {7, A3, A1, 8, 5};

// I found after lots of trial and error this is the only way to get TM8 to read PA12
// without the whole thing freezing or acting up
// returns 0 when closed, 1 when open
#define readBtn4 (PORT->Group[0].IN.reg & (1 << 12))

bool menuActive = false; // main Menu ISR handler variable,
// becomes true when main menu is opened and false when menu's over

bool splitActive = false; // ISR handler variable for recording splits in chronograph

// list of programs in main menu 8 elements long with each element 5 bytes (4 bytes + line end)
// first program is "quit", gets called when mainMenu() quits from no activity. Returns to main() loop.
char mainPrograms[7][5] = {"quit", "Chro", "data", "Adju", "Prty", "Race", "Flsh"};

#define NUM_MAIN_PROGRAMS 7 // number of main programs

// RTC time variables
uint8_t seconds = 0;
uint8_t minutes = 0;
uint8_t hours = 0;

/*
Main menu interrupt handler.
When ISR is invoked, it simply changes menuActive to true and goes right back to main().
ISR is kept as short as possible to maintain high clock accuracy. Simply,
Interrupt called -> menuActive becomes true -> main() detects menuActive == true and runs mainMenu() -> menuActive turns false again
*/
void menuInt() {
  menuActive = true;
}

// ISR for recording splits in chronograph
void recordSplitInt() {
  splitActive = true;
}

// hex array for tachometer animation frames
uint8_t tachInit[7] = {0x00, 0x04, 0x0C, 0x2C, 0x6C, 0x6D, 0x6F};
uint8_t swipeDown[10] = {0x40, 0x61, 0x71, 0x7B, 0x7F, 0x3F, 0x1E, 0x0E, 0x04, 0x00};

// "swipe down" animatin
void animSwipeDown(uint8_t animDelay) {
  for (int i=0; i<10; i++) {
    for (int d=0; d<4; d++) {
      lcd.Digits[d] = swipeDown[i];
    }
    lcd.Update(0);
    lcd.Update(1);
    delay(animDelay);
  }
}

// animation mimicking tachometer start-up on vintage cars
void animTach() {
  srand(analogRead(A0)); // set random seed to analog noise on A0
  uint8_t buffer; // buffer variable used for swapping leds[] elements
  for (int i=4; i>=0; i--) { // decrease RNG range for no overlap
    int ledToLight = random(0, i+1); // choose random LED to light
    buffer= leds[ledToLight]; // swap randomly selected LED with last array value.
    leds[ledToLight] = leds[i]; // randomly selected LED goes last in the array
    leds[i] = buffer; // last array element goes to where randomly selected LED was
    digitalWrite(leds[i], 1); // light up randomly selected LED
    delay(100);
  }
  for (int i=0; i<7; i++) { // for each frame of tachInit[] animation
    for (int d=0; d<4; d++) { // display on all digits
      lcd.Digits[d] = tachInit[i]; 
    }
    lcd.Update(0); // update both LCDs
    lcd.Update(1);
    delay(80);
  }
  delay(250);
  for (int i=6; i>=0; i--) { // like above, but opposite
    for (int d=0; d<4; d++) {
      lcd.Digits[d] = tachInit[i];
    }
    lcd.Update(0);
    lcd.Update(1);
    delay(80);
  }
  for (int i=4; i>=0; i--) { // Turns off all LEDs in a totally new random order.
    int ledToLight = random(0, i+1);
    buffer= leds[ledToLight];
    leds[ledToLight] = leds[i];
    leds[i] = buffer;
    digitalWrite(leds[i], 0);
    delay(100);
  }
}

// simple animation for system check/bootup animation
void blinkGo() {
  for (int i=0; i<5; i++) {
    lcd.dispStr("    ", 1);
    noTone(9);
    delay(30);
    for (int i=0; i<4; i++) {
      lcd.dispCharRaw(i, 0x54, 1);
    }
    tone(9, 4000);
    delay(30);
  }
  lcd.dispStr(" GO ", 1);
  noTone(9);
}

/*
bootup animation. Scans all I2C buses and devices. Checks for response
*/
void bootUpSitRep() {
  animTach(); // vintage tachometer animation
  uint8_t error; // I2C error code
  uint8_t address; // I2C device address
  uint8_t deviceCount = 0; // total number of devices detected. Increments with each device detected. Should be 5.
  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address); // start with first I2C bus
    error = Wire.endTransmission();
    if (error == 0) {
      if (address == 0x36) { // if MAX17048 fuel gauge detected on address 0x36
        deviceCount++;
        lcd.dispStr("Fuel", 0);
        blinkGo();
        delay(500);
      } else if (address == 0x38) { // if left LCD detected on address 0x38
        deviceCount++;
        lcd.dispStr("LCDl", 0);
        blinkGo();
        delay(500);
      }
    }
  }
  for(address = 1; address < 127; address++ ) {
    wire1.beginTransmission(address); // scan second I2C bus
    error = wire1.endTransmission();
    if (error == 0) {
      if (address == 0x18) { // if LIS3DH accelerometer detected on address 0x18
        deviceCount++;
        lcd.dispStr("Accl", 0);
        blinkGo();
        delay(500);
      } else if (address == 0x76) { // if bme environmental sensor detected on address 0x76
        deviceCount++;
        lcd.dispStr("temp", 0);
        blinkGo();
        delay(500);
      } else if (address == 0x38) { // if right LCD detected on address 0x38
        deviceCount++;
        lcd.dispStr("LCDr", 0);
        blinkGo();
        delay(500);
      }
    }
  }
  delay(750);
  if (deviceCount == 5) { // if all devices detected
    lcd.dispStr("", 1);
    lcd.dispStr("All ", 0);
    tone(9, 2000, 100);
    delay(500);
    lcd.dispStr("Syst", 0);
    lcd.dispStr("ems", 1);
    tone(9, 2000, 100);
    delay(500);
    lcd.dispStr("", 1);
    for (int i=0; i<7; i++) {
      lcd.dispStr(" GO ", 0);
      tone(9, 4000);
      delay(75);
      lcd.dispStr("", 0);
      noTone(9);
      delay(75);
    }
  } else { // if a different number of devices detected
    for (int i=0; i<5; i++) {
      lcd.dispStr(" Err", 0);
      lcd.dispStr("or  ", 1);
      delay(500);
      lcd.dispStr("dcnt", 0);
      lcd.dispDec(deviceCount, 1); // show number of devices detected
      delay(500);
    }
  }
}

/*
Sets the time.
returns 0 in case of an error (hours > 23 or minutes > 59) but this shouldn't happen.
bool ampm is 0 for AM, 1 for PM
Automagically detects 12H/24H format and if user inputs in 12H format, asks for AM/PM as well
*/
bool setTime() {
  uint8_t hourTens = 0; // hour tens digit
  uint8_t hourOnes = 0; // hour ones digit
  uint8_t minuteTens = 0; // minute tens digit
  uint8_t minuteOnes = 0; // minute ones digit
  bool ampm = 0; // 0 for AM, 1 for PM
  lcd.dispStr("Hset", 1); // indicate hour set mode
  while(readBtn4) { // until btn4 is pressed
    lcd.dispDec(hourTens * 10 + hourOnes, 0); // display hour value to set
    if (!digitalRead(btn1)) { // btn1 increments tens digit
      hourTens++;
      if (hourTens > 2 || hourTens * 10 + hourOnes > 23) { // prevent overflow to stay within 0-23
        hourTens = 0;
      }
      delay(200);
    }
    if (!digitalRead(btn2)) { // btn2 increments ones digit
      hourOnes++;
      if (hourOnes > 9 || hourTens * 10 + hourOnes > 23) { // prevent overflow to stay within 0-23
        hourOnes = 0;
      }
      delay(200);
    }
  }
  lcd.dispStr("hour", 0); // confirm hour has been set
  lcd.dispStr(" set", 1);
  delay(750);
  lcd.dispStr("Mset", 1); // indicate minute set mode
  while(readBtn4) { // until btn4 is pressed
    lcd.dispDec(minuteTens * 10 + minuteOnes, 0); // display minute value to set
    if (!digitalRead(btn1)) { // btn1 increments tens digit
      minuteTens++;
      if (minuteTens > 5 || minuteTens * 10 + minuteOnes > 59) { // prevent overflow to stay within 0-59
        minuteTens = 0;
      }
      delay(200);
    }
    if (!digitalRead(btn2)) { // btn2 increments ones digit
      minuteOnes++;
      if (minuteOnes > 9 || minuteTens * 10 + minuteOnes > 59) { // prevent overflow to stay within 0-59
        minuteOnes = 0;
      }
      delay(200);
    }
  }
  uint8_t hours = hourTens * 10 + hourOnes; // compute hour and minute values from selection
  uint8_t minutes = minuteTens * 10 + minuteOnes;
  if (hours > 23 || minutes > 59) { // error handling if hour/min values are beyond acceptable range, shouldn't happen tho
    lcd.dispStr(" Err", 0);
    lcd.dispStr("or  ", 1);
    delay(1000);
    return 0; // quit setTime()
  }
  // for some reason, doesn't work properly without the next three lines
  lcd.dispDec(hours, 0);
  lcd.dispDec(minutes, 1);
  delay(2000);
  if (hours > 12) { // if hour is set above 12, automatically set to PM
    ampm = 1;
  } else if (hours == 0) { // if hour is set to 0, automatically set to AM
    ampm = 0;
  } else if (hours <= 12) { // if hour is in 12H format(1-12), ask for AM/PM
    while(readBtn4) {
      if (ampm) {
        lcd.dispStr(" PM ", 0);
      } else {
        lcd.dispStr(" AM ", 0);
      }
      if (!digitalRead(btn1)) { // btn1 sets time to AM
        ampm = 0; 
      }
      if (!digitalRead(btn2)) { // btn2 sets to PM
        ampm = 1;
      }
    }
  }

  if (hours < 12 && ampm) { // if user input is 12H format and PM
    rtc.setHours(hours + 12); // set RTC to user input + 12 cuz RTC only understands 24H format
  } else if (hours == 12 && !ampm) { // if hour is set to 12 and AM
    rtc.setHours(0); // set RTC to 0 cuz RTC would take 12 as 12 PM
  } else {
    rtc.setHours(hours);
  }
  rtc.setMinutes(minutes);
  rtc.setSeconds(0);
  lcd.dispStr("time", 0);
  lcd.dispStr(" set", 1);
  delay(1000);
  return 1;
}


uint32_t chronoSplits[10]; // 10-long split record

/*
1/1000 second chronograph. Measures up to 59"59'999.
BTN4: start/stop chronograph
BTN3: record split. stops after all 10 slots are filled. record slot shown on rightmost digit.
BTN1: show time. Frequent use hampers precision.
Not using any interrupts to record splits cuz that caused problems where it would immediately record an interrupt
right when the chrono started. same for race chrono.
*/
bool chronoGraph() {
  uint8_t chronoSplitsCounter = 0;
  while(readBtn4) { // start when button 4 is pressed
    lcd.dispStr("btn4", 0);
    lcd.dispStr("strt", 1);
    if (!digitalRead(btn3)) { // press btn3 to quit
      return 0;
    }
  } while(!readBtn4); // start on rising edge to prevent split recording immediately
  uint32_t chronoStartTime = millis(); // time when chronograph started, in milliseconds
  uint32_t chronoMillis; // declare variable for elapsed time in milliseconds
  uint32_t chronoSeconds; // ...elapsed time in seconds
  uint32_t chronoMinutes; // ...elapsed time in minutes
  while(readBtn4) { // until button 4 is pressed (btn4 will quit chronograph)
    chronoMillis = millis() - chronoStartTime; // compute elapsed time in milliseconds
    chronoSeconds = chronoMillis / 1000; // compute elapsed time in seconds
    chronoMinutes = chronoSeconds / 60; // compute elapsed time in minutes
    chronoMillis %= 1000; // compute elapsed milliseconds
    chronoSeconds %= 60; // compute elapsed seconds
    chronoMinutes %= 60; // compute elapsed minutes
    lcd.dispDec(chronoMinutes * 100 + chronoSeconds, 0); // display elapsed time on LCD
    lcd.dispDec(chronoMillis * 10 + chronoSplitsCounter, 1); // display elapsed milliseconds + split record slot on the right
    if (!digitalRead(btn3) && chronoSplitsCounter < 10) { // if button 3 is pressed and split record space is available
      while(!digitalRead(btn3)) {digitalWrite(led5, 1);} // show split time & light up LED5 while btn3 is depressed
      digitalWrite(led5, 0); // turn off LED5
      lcd.dispDec(chronoMinutes * 100 + chronoSeconds, 0); // display split time
      lcd.dispDec(chronoMillis * 10 + chronoSplitsCounter, 1);
      chronoSplits[chronoSplitsCounter] = chronoMinutes * 100000 + chronoSeconds * 1000 + chronoMillis;
      Serial.print(chronoSplits[chronoSplitsCounter]); // debug messages
      Serial.println(chronoSplitsCounter);
      chronoSplitsCounter++; // increment chronoSplitsCounter
    }
    if (!digitalRead(btn1)) { // if btn1 is pressed
      while(!digitalRead(btn1)) {
        lcd.dispDec(rtc.getHours() * 100 + rtc.getMinutes(), 0); // display current time
        lcd.dispDec(rtc.getSeconds(), 1);
      }
    }
    // if (!readBtn4) {
    //   uint32_t millisBuffer = millis();
    //   lcd.dispDec(chronoMinutes * 100 + chronoSeconds, 0); // display stopped chrono time
    //   lcd.dispDec(chronoMillis * 10 + chronoSplitsCounter, 1);
    //   while(readBtn4) {};
    //   chronoStartTime = chronoStartTime - millisBuffer;
    // }
  }
  // quit chronograph animation
  delay(1000);
  lcd.dispStr("quit", 0);
  lcd.dispStr("chro", 1);
  delay(1000);
  for (int i=0; i<3; i++) {
    lcd.dispStr("quit", 0);
    lcd.dispStr("chro", 1);
    delay(75);
    lcd.dispStr("", 0);
    lcd.dispStr("", 1);
    delay(75);
  }
  return 0;
}

char tracks[12][5] = {
  "spa",
  "mnza",
  "nurb",
  "mnco",
  "szka"
};

float distances[12] {4.352, 3.600, 12.944, 2.074, 3.608};

uint32_t raceSplits[100] = {};
uint16_t averageSpeeds[100] = {};
uint8_t trackSelection = 0;

/*
Race chronograph. In addition to all features in the regular chronograph,
also shows average speed per split.
Can only measure up to 9"59.999
100-deep split record
*/
bool raceChrono() {
  uint8_t raceSplitsCounter = 0;
  while (readBtn4) {
    lcd.dispStr("trck", 0);
    lcd.dispStr(tracks[trackSelection], 1);
    if (!digitalRead(btn1)) {
      trackSelection--;
      if (trackSelection > 4) trackSelection = 4;
      delay(200);
    }
    else if (!digitalRead(btn3)) {
      trackSelection++;
      if (trackSelection > 4) trackSelection = 0;
      delay(200);
    }
  } while(!readBtn4); // start on rising edge to prevent split recording immediately
  uint16_t distance = distances[trackSelection];
  delay(200);
  while(readBtn4) { // start when button 4 is pressed
    lcd.dispStr("btn4", 0);
    lcd.dispStr("strt", 1);
    if (!digitalRead(btn3)) { // press btn3 to quit
      return 0;
    }
  } while(!readBtn4); // start on rising edge to prevent split recording immediately
  uint32_t raceStartTime = millis(); // time when chronograph started, in milliseconds
  uint32_t raceMillis; // declare variable for elapsed time in milliseconds
  uint32_t raceSeconds; // ...elapsed time in seconds
  uint32_t raceMinutes; // ...elapsed time in minutes
  while(readBtn4) { // until button 4 is pressed (btn4 will quit chronograph)
    raceMillis = millis() - raceStartTime; // compute elapsed time in milliseconds
    raceSeconds = raceMillis / 1000; // compute elapsed time in seconds
    raceMinutes = raceSeconds / 60; // compute elapsed time in minutes
    raceMillis %= 1000; // compute elapsed milliseconds
    raceSeconds %= 60; // compute elapsed seconds
    raceMinutes %= 60; // compute elapsed minutes
    lcd.dispDec(raceMinutes * 100 + raceSeconds, 0); // display elapsed time on LCD
    lcd.dispDec(raceMillis * 10 + raceSplitsCounter, 1); // display elapsed milliseconds + split record slot on the right
    if (!digitalRead(btn3) && raceSplitsCounter < 100) { // if button 3 is pressed and split record space is available.
      while(!digitalRead(btn3)) {
        digitalWrite(led5, 1); // show split time & light up LED5 while btn3 is depressed
        lcd.dispDec(raceMinutes * 100 + raceSeconds, 0); // display split time
        lcd.dispDec(raceMillis, 1);
      }
      digitalWrite(led5, 0); // turn off LED5
      float vavg = (distances[trackSelection]) / (float)((float)(millis() - raceStartTime) / 1000 / 3600);
      lcd.dispDec((int)(vavg), 0);
      lcd.dispDec(((vavg - (int)(vavg)) * 100), 1);
      raceSplits[raceSplitsCounter] = raceMinutes * 100000 + raceSeconds * 1000 + raceMillis;
      raceSplitsCounter++; // increment raceSplitsCounter
      delay(2000);
    }
    if (!digitalRead(btn1)) {
      while(!digitalRead(btn1)) {
        lcd.dispDec(rtc.getHours() * 100 + rtc.getMinutes(), 0);
        lcd.dispDec(raceSplitsCounter, 1);
      }
    }
  }
  // quit chronograph animation
  delay(1000);
  lcd.dispStr("quit", 0);
  lcd.dispStr("race", 1);
  delay(1000);
  for (int i=0; i<3; i++) {
    lcd.dispStr("quit", 0);
    lcd.dispStr("race", 1);
    delay(75);
    lcd.dispStr("", 0);
    lcd.dispStr("", 1);
    delay(75);
  }
  return 0;
}

/*
retrieves chronograph split records
first prompts whether to retrieve records from chrono or race
user selects btn1 for chrono, btn3 for race
*/
uint8_t chronoData() {
  lcd.dispStr("chro", 0); // prompt choice
  lcd.dispStr("race", 1);
  uint8_t chronoCounter = 0;
  uint8_t raceCounter = 0;
  while (digitalRead(btn1) && digitalRead(btn3)); // wait for either btn1 or btn3 input
  if (!digitalRead(btn1)) { // if chrono records selected
    delay(225);
    while(readBtn4) {
      lcd.dispDec(chronoSplits[chronoCounter] / 1000, 0);
      lcd.dispDec((chronoSplits[chronoCounter] % 1000) * 10 + chronoCounter, 1);
      if (!digitalRead(btn3)) {
        chronoCounter++;
        if (chronoCounter > 9) {
          chronoCounter = 0;
        }
        Serial.print(chronoSplits[chronoCounter]);
        delay(200);
      } else if (!digitalRead(btn1)) {
        chronoCounter--;
        if (chronoCounter > 9) {
          chronoCounter = 9;
        }
        Serial.print(chronoSplits[chronoCounter]);
        delay(200);
      }
    }
  } else if (!digitalRead(btn3)) { // if race records selected
    delay(255);
    while(readBtn4) {
      lcd.dispDec(raceSplits[raceCounter] / 100, 0);
      lcd.dispDec((raceSplits[raceCounter] % 100) * 100 + raceCounter, 1);
      if (!digitalRead(btn3)) {
        raceCounter++;
        if (raceCounter > 99) {
          raceCounter = 0;
        }
        Serial.print(raceSplits[raceCounter]);
        delay(200);
      } else if (!digitalRead(btn1)) {
        raceCounter--;
        if (raceCounter > 99) {
          raceCounter = 99;
        }
        Serial.print(raceSplits[raceCounter]);
        delay(150);
      }
    }
  }
  return 0;
}

/*
Party mode.
Watch flashes custom messages and LEDs
*/
uint8_t party() {
  uint8_t cnt = 0;
  while(digitalRead(btn3)) {
    if (cnt) {
      delay(200);
      lcd.dispStr("OVTA", 0);
      lcd.dispStr("TIME", 1);
    } else {
      delay(200);
      lcd.dispStr(" PAS", 0);
      lcd.dispStr("SION", 1);
    }
    if (!readBtn4) {
      lcd.dispStr("", 0);
      lcd.dispStr("", 1);
      delay(5000);
      animTach();
    }
    if (!digitalRead(btn2)) {
      cnt = !cnt;
    }
    if (!digitalRead(btn1)) {
      lcd.dispStr("", 0);
      lcd.dispStr("", 1);
      delay(5000);
      animTach();
      for (int i=0; i<5; i++) {
        lcd.dispStr("N vi", 0);
        lcd.dispStr("sion", 1);
        delay(133);
        lcd.dispStr("", 0);
        lcd.dispStr("", 1);
        delay(133);
      }
      for (int i=0; i<5; i++) {
        lcd.dispStr(" 74 ", 0);
        lcd.dispStr(" 74 ", 1);
        delay(133);
        lcd.dispStr("", 0);
        lcd.dispStr("", 1);
        delay(133);
      }
      lcd.dispStr(" 74 ", 0);
      lcd.dispStr(" 74 ", 1);
      delay(1000);
      return 0;
    }
  }
  return 0;
}

void flashLight() {
  bool flash = 0;
  lcd.dispStr("", 0);
  lcd.dispStr("", 1);
  while(digitalRead(btn1)) {
    if (!readBtn4) {
      flash = !flash;
      delay(250);
    }
    if (!digitalRead(btn3)) {
      digitalWrite(6, !digitalRead(btn3));
    }
    digitalWrite(6, flash);
  }
  digitalWrite(6, 0);
}

/*
Main menu function.
holds a number of "main programs" that can be quickly accessed in the main menu.
BTN3 scrolls through list of programs, hold button to scroll quickly
BTN4 runs selected program
After approx. 2 seconds of inactivity, mainMenu() returns 0 and goes back to home screen.
*/
uint8_t mainMenu() {
  uint8_t mainProgramNumber = 1; // counter variable for scrolling through list of programs in main menu
  uint32_t startTime = millis(); // time when function starts
  detachInterrupt(btn3); // detach interrupts for button use during function
  delay(200); // short delay to not start fast-scrolling right as main menu is called
  while (millis() - startTime <= INACTIVITY_TIMEOUT) { // while under timeout threshold
    lcd.dispDec(mainProgramNumber, 0); // display program number on the left, but it starts from 1, not 0
    lcd.dispStr(mainPrograms[mainProgramNumber], 1); // display program name on the right
    if (!digitalRead(btn3)) { // if button 3 (top right) is pressed
      mainProgramNumber++; // increment main program counter and select next program
      if (mainProgramNumber >= NUM_MAIN_PROGRAMS) { // roll back to program 0 after going through entire list
        mainProgramNumber = 1;
      }
      delay(BUTTON_DELAY); // short delay to prevent crazy fast scrolling
      startTime = millis(); // reset timer to 0 to extend time
    } else if (!digitalRead(btn1)) { // if button 1 (top left) is pressed
      mainProgramNumber--; // increment main program counter and select next program
      if (mainProgramNumber >= NUM_MAIN_PROGRAMS) { // roll back to program 0 after going through entire list
        mainProgramNumber = NUM_MAIN_PROGRAMS - 1;
      }
      delay(BUTTON_DELAY); // short delay to prevent crazy fast scrolling
      startTime = millis(); // reset timer to 0 to extend time
    } else if (!readBtn4) { // if button 4 (bottom right) is pressed
      for (int i=0; i<3; i++) { // blink selected program 3 times on the display
        lcd.dispStr("", 0);
        lcd.dispStr("", 1);
        delay(50);
        lcd.dispDec(mainProgramNumber, 0);
        lcd.dispStr(mainPrograms[mainProgramNumber], 1);
        delay(50);
      }
      delay(500); // half-second delay
      return mainProgramNumber; // end mainMenu()
    }
  }
  return 0; // end mainMenu()
}

/*
gets program number from mainMenu() and runs it
typically you would call runMainProgram(mainMenu()) because
the value returned from mainMenu() goes where the prog argument is
*/

//char mainPrograms[7][5] = {"quit", "Chro", "data", "Adju", "Prty", "Race", "Flsh"};
void runMainProgram(uint8_t prog) {
  switch (prog) {
  case 0:
    break;
  case 1:
    chronoGraph();
    break;
  case 2:
    chronoData();
    break;
  case 3:
    setTime();
    break;
  case 4:
    party();
    break;
  case 5: 
    raceChrono();
    break;
  case 6:
    flashLight();
    break;
  default:
    lcd.dispStr("Fuck", 0);
    delay(1000);
    break;
  }
}

uint8_t firingOrder[] = {1, 5, 3, 7, 4, 8, 2, 6};

uint8_t starter() {
  for (int i=0; i<3; i++) {
    animSwipeDown(30);
  }

  uint8_t battLvl = (uint8_t) fuel.cellPercent();
  if (battLvl > 99) battLvl = 99;
  if (battLvl <= 10 && battLvl >= 0) {
    for (int i=0; i<3; i++) {
      lcd.dispStr(" NO ", 0);
      lcd.dispStr("FUEL", 1);
      delay(300);
      lcd.dispStr("", 0);
      lcd.dispStr("", 1);
      delay(300);
    }
  }
  uint16_t cnt = 0;
  while(1) {
    if (!digitalRead(btn1)) {
      if (!digitalRead(btn3)) {
        while(!digitalRead(btn3)) {
          cnt++;
          tone(9, cnt * 25);
          uint8_t graph = cnt / 40;
          switch (graph) {
            case 0:
              lcd.dispStr("", 0);
              lcd.dispStr("", 1);
              break;
            case 1:
              lcd.dispStr("8", 0);
              lcd.dispStr("", 1);
              break;
            case 2:
              lcd.dispStr("88", 0);
              lcd.dispStr("", 1);
              break;
            case 3:
              lcd.dispStr("888", 0);
              lcd.dispStr("", 1);
              break;
            case 4:
              lcd.dispStr("8888", 0);
              lcd.dispStr("", 1);
              break;
            case 5:
              lcd.dispStr("8888", 0);
              lcd.dispStr("8", 1);
              break;
            case 6:
              lcd.dispStr("8888", 0);
              lcd.dispStr("88", 1);
              break;
            case 7:
              lcd.dispStr("8888", 0);
              lcd.dispStr("888", 1);
              break;
            case 8:
              lcd.dispStr("8888", 0);
              lcd.dispStr("8888", 1);
              break;
          }
          if (cnt == 320) {
            delay(50);
            for (int i=0; i<5; i++) {
              lcd.dispStr("8888", 0);
              lcd.dispStr("8888", 1);
              tone(9, 4000);
              delay(60);
              lcd.dispStr("", 0);
              lcd.dispStr("", 1);
              noTone(9);
              delay(60);
            }
            noTone(9);
            return 0;
          }
        }
      } else {
        while(digitalRead(btn3) && cnt > 0) {
          cnt--;
          tone(9, cnt * 25);
          uint8_t graph = cnt / 50;
          switch (graph) {
            case 0:
              lcd.dispStr("", 0);
              lcd.dispStr("", 1);
              break;
            case 1:
              lcd.dispStr("8", 0);
              lcd.dispStr("", 1);
              break;
            case 2:
              lcd.dispStr("88", 0);
              lcd.dispStr("", 1);
              break;
            case 3:
              lcd.dispStr("888", 0);
              lcd.dispStr("", 1);
              break;
            case 4:
              lcd.dispStr("8888", 0);
              lcd.dispStr("", 1);
              break;
            case 5:
              lcd.dispStr("8888", 0);
              lcd.dispStr("8", 1);
              break;
            case 6:
              lcd.dispStr("8888", 0);
              lcd.dispStr("88", 1);
              break;
            case 7:
              lcd.dispStr("8888", 0);
              lcd.dispStr("888", 1);
              break;
            case 8:
              lcd.dispStr("8888", 0);
              lcd.dispStr("8888", 1);
              break;
          }
        }
      }
    }
    //uint8_t firingOrder[] = {1, 5, 3, 7, 4, 8, 2, 6};
    if (digitalRead(btn3) && cnt == 0) {
      for (int i=0; i<8; i++) {
        uint8_t index = firingOrder[i] - 1;
        if (index >= 4) {
          lcd.dispChar(index - 4, '-', 1);
          delay(50);
          lcd.dispChar(index - 4, 'K', 1);
          delay(50);
          lcd.dispChar(index - 4, '0', 1);
          delay(50);
          lcd.dispCharRaw(index - 4, 0x44, 1);
          delay(50);
          lcd.Command(CDM4101_CLEAR, 1);
          delay(50);
        } else {
          lcd.dispChar(index, '-', 0);
          delay(50);
          lcd.dispChar(index, 'K', 0);
          delay(50);
          lcd.dispChar(index, '0', 0);
          delay(50);
          lcd.dispCharRaw(index, 0x44, 0);
          delay(50);
          lcd.Command(CDM4101_CLEAR, 0);
          delay(50);
        }
      }
    }
  }
}

float altitude(const int32_t press, const float seaLevel = 1013.25);
float altitude(const int32_t press, const float seaLevel) {
  /*!
  @brief     This converts a pressure measurement into a height in meters
  @details   The corrected sea-level pressure can be passed into the function if it is known,
             otherwise the standard atmospheric pressure of 1013.25hPa is used (see
             https://en.wikipedia.org/wiki/Atmospheric_pressure) for details.
  @param[in] press    Pressure reading from BME680
  @param[in] seaLevel Sea-Level pressure in millibars
  @return    floating point altitude in meters.
  */
  static float Altitude;
  Altitude =
      44330.0 * (1.0 - pow(((float)press / 100.0) / seaLevel, 0.1903));  // Convert into meters
  return (Altitude);
}  // of method altitude()

// system initialization
void setup() {

  rtc.begin(); // fire up RTC

  // set RTC time
  rtc.setHours(hours);
  rtc.setMinutes(minutes);
  rtc.setSeconds(seconds);

  // initialize LCDs
  lcd.init_lcd();

  // start both I2C buses
  Wire.begin();
  wire1.begin();

  Wire.setClock(400000);
  wire1.setClock(400000);

  pinPeripheral(4, PIO_SERCOM); // SDA: D4 / PA08
  pinPeripheral(3, PIO_SERCOM); // SCL: D3 / PA09

  // enable pullups on all inputs to prevent floating
  pinMode(btn1, INPUT_PULLUP); // button 1, top left
  pinMode(btn2, INPUT_PULLUP); // button 2, bottom left
  pinMode(btn3, INPUT_PULLUP); // button 3, top right

  pinMode(6, OUTPUT); // flashlight
  pinMode(9, OUTPUT); // piezo

  for (int i=0; i<5; i++) {
    pinMode(leds[i], OUTPUT);
  }

  // enable pullups for PA12, sets it to input, writes HIGH to it
  // this is the only way to configure PA12 without having it freeze TM8
  PORT->Group[0].PINCFG[12].reg = PORT_PINCFG_PULLEN | PORT_PINCFG_INEN;
  PORT->Group[0].OUTSET.reg = PORT_PA12;

  // set button 3 (top right) to open main menu
  // set to FALLING because RISING would often trigger the interrupt but not actually run the ISR,
  // leading to systemw-wide clock delays
  attachInterrupt(btn3, menuInt, FALLING);

  fuel.begin(); // initialize MAX17048
  accel.begin();
  bme.begin(0x76, wire1);

	if(bme.checkStatus())
	{
		if (bme.checkStatus() == BME68X_ERROR)
		{
			Serial.println("Sensor error:" + bme.statusString());
			return;
		}
		else if (bme.checkStatus() == BME68X_WARNING)
		{
			Serial.println("Sensor Warning:" + bme.statusString());
		}
	}
	/* Set the default configuration for temperature, pressure and humidity */
	bme.setTPH();
	/* Set the heater configuration to 300 deg C for 100ms for Forced mode */
	bme.setHeaterProf(300, 100);
	Serial.println("TimeStamp(ms), Temperature(deg C), Pressure(Pa), Humidity(%), Gas resistance(ohm), Status");

  starter();
  bootUpSitRep();
  menuActive = false; // for some reason starter() triggers menuInt(), so I need this to prevent the watch from booting straight into the menu

}

// main function
void loop() {

	bme68xData data;

	bme.setOpMode(BME68X_FORCED_MODE);
	delayMicroseconds(bme.getMeasDur());

	if (bme.fetchData())
	{
		bme.getData(data);
    int temp = (int)data.temperature;
    int hum = (int)data.humidity;
		lcd.dispDec(temp, 0);
    lcd.dispDec(hum, 1);
		Serial.print(String(data.temperature) + ", ");
		Serial.print(String(data.pressure) + ", ");
		Serial.print(String(data.humidity) + ", ");
		Serial.print(String(data.gas_resistance) + ", ");
		Serial.println(data.status, HEX);
	}

  // int xAxis = round(accel.readFloatAccelX() * 10);
  // int yAxis = round(accel.readFloatAccelY() * 10);
  // int zAxis = round(accel.readFloatAccelZ() * 10);

  // lcd.dispDec(xAxis, 0);
  // lcd.dispDec(yAxis, 1);

  // delay(100);
  

  // uint8_t battLvl = (uint8_t) fuel.cellPercent();
  // if (battLvl > 99) battLvl = 99;

  // // LCD displays hours and minutes on the left, seconds on the right
  // lcd.dispDec(rtc.getHours() * 100 + rtc.getMinutes(), 0);
  // lcd.dispDec(rtc.getSeconds() * 100 + battLvl, 1);

  // //if menuInt() ISR is called, pull up menu and run chosen main program
  // if (menuActive) {
  //   runMainProgram(mainMenu());
  //   attachInterrupt(btn3, menuInt, FALLING); // reattach interrupt to resume normal button function in main()
  //   menuActive = false; // reset menuActive to false
  // }
}