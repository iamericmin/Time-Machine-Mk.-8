/*
 _______             __  ___         __   _            __  _____       ___ 
/_  __(_)_ _  ___   /  |/  /__ _____/ /  (_)__  ___   /  |/  / /__    ( _ )
 / / / /  ' \/ -_) / /|_/ / _ `/ __/ _ \/ / _ \/ -_) / /|_/ /  '_/   / _  |
/_/ /_/_/_/_/\__/ /_/  /_/\_,_/\__/_//_/_/_//_/\__/ /_/  /_/_/\_(_)  \___/ 
TIME MACHINE MK. 8
MIN WORKS 2024
@designedbyericmin

GENERAL RULE OF THUMB:
Button 1: top left. scroll up
Button 2: buttom left. scroll down
Button 3: top right. confirm/enter
Button 4: top left. cancel/exit
*/

#include <Arduino.h>
#include <Wire.h>
#include "wiring_private.h" // pinPeripheral() function
#include <RTCZero.h>
#include <Adafruit_MAX1704X.h>
#include <ArduinoLowPower.h>
#include <SparkFunLIS3DH.h>
#include <bme68xLibrary.h>
#include <SparkFun_External_EEPROM.h>
#include <time.h>
#include <TM8_util.h>

#define INACTIVITY_TIMEOUT 2000 // inactivity threshold of 2 seconds
#define BUTTON_DELAY 100 // delay between button readings for scrolling, long press, etc.
#define LCD_ADDRESS 0x38
#define FUEL_ADDRESS 0x36
#define ACCEL_ADDRESS 0x18
#define BME_ADDRESS 0x76
#define ROM_ADDRESS 0x50

TwoWire wire1(&sercom2, 4, 3); // new Wire object to set up second I2C port on SERCOM 2
RTCZero rtc; // RTC object
Adafruit_MAX17048 fuel;
LIS3DH accel(I2C_MODE, 0x18);
Bme68x bme;
bme68xData BMEData;
ExternalEEPROM rom;
TM8_util TM8;

// I found after lots of trial and error this is the only way to get TM8 to read PA12
// without the whole thing freezing or acting up
// returns 0 when closed, 1 when open
#define readBtn1 (PORT->Group[0].IN.reg & (1 << 14))
#define readBtn2 (PORT->Group[0].IN.reg & (1 << 13))
#define readBtn3 (PORT->Group[1].IN.reg & (1 << 11))
#define readBtn4 (PORT->Group[0].IN.reg & (1 << 12))

const uint8_t btn1 = 2; // top left button
const uint8_t btn2 = 38; // bottom left button
const uint8_t btn3 = 24; // top right button
const uint8_t btn4 = 22; // bottom right button

const uint8_t leftBL = 26; // left backlight (red)
const uint8_t rightBL = 3; // right backlight (red)

uint8_t localLEDs[5] = {7, A3, A1, 8, 5};

bool menuActive = false; // main Menu ISR handler variable,
// becomes true when main menu is opened and false when menu's over

bool showDateActive = false; // interrupt for showing date on home screen.
// triggered by BTN1

bool splitActive = false; // ISR handler variable for recording splits in chronograph

bool actionActive = false; // ISR handler for action button (BTN2)

bool dispMode = 1; // 0 for wakeToCheck, 1 for AOD

// list of programs in main menu 8 elements long with each element 5 bytes (4 bytes + line end)
// first program is "quit", gets called when mainMenu() quits from no activity. Returns to main() loop.
char mainPrograms[9][5] = {"quit", "Chro", "data", "Adju", "prty", "sens", "Race", "Flsh", "game"};
char daysOfTheWeek[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

uint8_t numPrograms = sizeof(mainPrograms) / sizeof(mainPrograms[0]);

// RTC time variables
uint8_t seconds = 0;
uint8_t minutes = 0;
uint8_t hours = 0;
uint16_t year = 23;
uint8_t month = 12;
uint8_t day = 20;

/*
Main menu interrupt handler.
When ISR is invoked, it simply changes menuActive to true and goes right back to main().
ISR is kept as short as possible to maintain high clock accuracy. Simply,
Interrupt called -> menuActive becomes true -> main() detects menuActive == true and runs mainMenu() -> menuActive turns false again
*/
void menuInt() {
  menuActive = true;
}

void showDateInt() {
  showDateActive = true;
}

void actionInt() {
  actionActive = true;
}

// ISR for recording splits in chronograph
void recordSplitInt() {
  splitActive = true;
}

uint8_t getDayOfWeek(uint16_t y, uint16_t m, uint16_t d) {
  return (d+=m<3?y--:y-2,23*m/9+d+4+y/4-y/100+y/400)%7;
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
  TM8.dispStr("hour", 1); // indicate hour set mode
  while(readBtn3) { // until btn4 is pressed
    TM8.dispDec(hourTens * 10 + hourOnes, 0); // display hour value to set
    if (!readBtn1) { // btn1 increments tens digit
      hourTens++;
      if (hourTens > 2 || hourTens * 10 + hourOnes > 23) { // prevent overflow to stay within 0-23
        hourTens = 0;
      }
      delay(BUTTON_DELAY);
    }
    if (!readBtn2) { // btn2 increments ones digit
      hourOnes++;
      if (hourOnes > 9 || hourTens * 10 + hourOnes > 23) { // prevent overflow to stay within 0-23
        hourOnes = 0;
      }
      delay(BUTTON_DELAY);
    }
    if (!readBtn4) { // exit function for when triggered by mistake
      return 0;
    }
  }
  TM8.dispStr("hour", 0); // confirm hour has been set
  TM8.dispStr(" set", 1);
  delay(750);
  TM8.dispStr(" min", 1); // indicate minute set mode
  while(readBtn3) { // until btn4 is pressed
    TM8.dispDec(minuteTens * 10 + minuteOnes, 0); // display minute value to set
    if (!readBtn1) { // btn1 increments tens digit
      minuteTens++;
      if (minuteTens > 5 || minuteTens * 10 + minuteOnes > 59) { // prevent overflow to stay within 0-59
        minuteTens = 0;
      }
      delay(BUTTON_DELAY);
    }
    if (!readBtn2) { // btn2 increments ones digit
      minuteOnes++;
      if (minuteOnes > 9 || minuteTens * 10 + minuteOnes > 59) { // prevent overflow to stay within 0-59
        minuteOnes = 0;
      }
      delay(BUTTON_DELAY);
    }
  }
  uint8_t hours = hourTens * 10 + hourOnes; // compute hour and minute values from selection
  uint8_t minutes = minuteTens * 10 + minuteOnes;
  if (hours > 23 || minutes > 59) { // error handling if hour/min values are beyond acceptable range, shouldn't happen tho
    TM8.dispStr(" Err", 0);
    TM8.dispStr("or  ", 1);
    delay(1000);
    return 0; // quit setTime()
  }
  // for some reason, doesn't work properly without the next three lines
  TM8.dispDec(hours, 0);
  TM8.dispDec(minutes, 1);
  delay(2000);
  if (hours > 12) { // if hour is set above 12, automatically set to PM
    ampm = 1;
  } else if (hours == 0) { // if hour is set to 0, automatically set to AM
    ampm = 0;
  } else if (hours <= 12) { // if hour is in 12H format(1-12), ask for AM/PM
    while(readBtn3) {
      if (ampm) {
        TM8.dispStr(" PM ", 0);
      } else {
        TM8.dispStr(" AM ", 0);
      }
      if (!readBtn1) { // btn1 sets time to AM
        ampm = 0; 
      }
      if (!readBtn2) { // btn2 sets to PM
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
  TM8.dispStr("time", 0);
  TM8.dispStr(" set", 1);
  delay(1000);
  for (int i=0; i<3; i++) {
    TM8.dispStr("", 0);
    TM8.dispStr("", 1);
    delay(50);
    TM8.dispStr("time", 0);
    TM8.dispStr("set", 1);
    delay(50);
  }
  return 1;
}

bool setDate() {
  uint8_t month = 12; // month value
  uint8_t dayTens = 2; // date tens digit
  uint8_t dayOnes = 9; // date ones digit
  TM8.dispStr("mnth", 1); // indicate month set mode
  while(readBtn3) { // until btn4 is pressed
    TM8.dispDec(month, 0); // display month value to set
    if (!readBtn1) { // btn1 increments month
      month++;
      if (month > 12 || month < 1) { // prevent overflow to stay within 1-12
        month = 1;
      }
      delay(BUTTON_DELAY);
    }
    if (!readBtn2) { // btn1 increments month
      month--;
      if (month > 12 || month < 1) { // prevent overflow to stay within 1-12
        month = 12;
      }
      delay(BUTTON_DELAY);
    }
    if (!readBtn4) { // exit function for when triggered by mistake
      return 0;
    }
  }
  TM8.dispStr("mnth", 0); // confirm month has been set
  TM8.dispStr(" set", 1);
  delay(750);
  TM8.dispStr(" day", 1); // indicate day set mode
  while(readBtn3) { // until btn4 is pressed
    TM8.dispDec(dayTens * 10 + dayOnes, 0); // display day value to set
    if (!readBtn1) { // btn1 increments tens digit
      dayTens++;
      if (dayTens > 3 || dayTens * 10 + dayOnes > 31) { // prevent overflow to stay within 0-31
        dayTens = 0;
      }
      delay(BUTTON_DELAY);
    }
    if (!readBtn2) { // btn2 increments ones digit
      dayOnes++;
      if (dayOnes > 9 || dayTens * 10 + dayOnes > 31) { // prevent overflow to stay within 0-31
        dayOnes = 0;
      }
      delay(BUTTON_DELAY);
    }
  }
  uint8_t date = dayTens * 10 + dayOnes;
  if (month > 12 || date > 31) { // error handling if hour/min values are beyond acceptable range, shouldn't happen tho
    TM8.dispStr(" Err", 0);
    TM8.dispStr("or  ", 1);
    delay(1000);
    return 0; // quit setTime()
  }
  // for some reason, doesn't work properly without the next three lines
  TM8.dispDec(month, 0);
  TM8.dispDec(date, 1);
  delay(2000);
  rtc.setDate(date, month, 23);
  TM8.dispStr("date", 0);
  TM8.dispStr(" set", 1);
  delay(1000);
  for (int i=0; i<3; i++) {
    TM8.dispStr("", 0);
    TM8.dispStr("", 1);
    delay(50);
    TM8.dispStr("date", 0);
    TM8.dispStr("set", 1);
    delay(50);
  }
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
  while(readBtn3) { // start when button 3 is pressed
    TM8.dispStr("btn3", 0);
    TM8.dispStr("strt", 1);
    if (!readBtn4) { // press btn4 to quit
      return 0;
    }
  } while(!readBtn3); // start on rising edge to prevent split recording immediately
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
    TM8.dispDec(chronoMinutes * 100 + chronoSeconds, 0); // display elapsed time on LCD
    TM8.dispDec(chronoMillis * 10 + chronoSplitsCounter, 1); // display elapsed milliseconds + split record slot on the right
    if (!readBtn3 && chronoSplitsCounter < 10) { // if button 3 is pressed and split record space is available
      while(!readBtn3) {digitalWrite(localLEDs[5], 1);} // show split time & light up LED5 while btn3 is depressed
      digitalWrite(localLEDs[5], 0); // turn off LED5
      TM8.dispDec(chronoMinutes * 100 + chronoSeconds, 0); // display split time
      TM8.dispDec(chronoMillis * 10 + chronoSplitsCounter, 1);
      chronoSplits[chronoSplitsCounter] = chronoMinutes * 100000 + chronoSeconds * 1000 + chronoMillis;
      Serial.print(chronoSplits[chronoSplitsCounter]); // debug messages
      Serial.println(chronoSplitsCounter);
      chronoSplitsCounter++; // increment chronoSplitsCounter
    }
    if (!readBtn1) { // if btn1 is pressed
      while(!readBtn1) {
        TM8.dispDec(rtc.getHours() * 100 + rtc.getMinutes(), 0); // display current time
        TM8.dispDec(rtc.getSeconds(), 1);
      }
    }
    // if (!readBtn4) {
    //   uint32_t millisBuffer = millis();
    //   TM8.dispDec(chronoMinutes * 100 + chronoSeconds, 0); // display stopped chrono time
    //   TM8.dispDec(chronoMillis * 10 + chronoSplitsCounter, 1);
    //   while(readBtn4) {};
    //   chronoStartTime = chronoStartTime - millisBuffer;
    // }
  }
  // quit chronograph animation
  delay(1000);
  TM8.dispStr("quit", 0);
  TM8.dispStr("chro", 1);
  delay(1000);
  for (int i=0; i<3; i++) {
    TM8.dispStr("quit", 0);
    TM8.dispStr("chro", 1);
    delay(75);
    TM8.dispStr("", 0);
    TM8.dispStr("", 1);
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
  while (readBtn3) {
    TM8.dispStr("trck", 0);
    TM8.dispStr(tracks[trackSelection], 1);
    if (!readBtn2) {
      trackSelection--;
      if (trackSelection > 4) trackSelection = 4;
      delay(BUTTON_DELAY);
    }
    else if (!readBtn1) {
      trackSelection++;
      if (trackSelection > 4) trackSelection = 0;
      delay(BUTTON_DELAY);
    }
  } while(!readBtn3); // start on rising edge to prevent split recording immediately
  delay(BUTTON_DELAY);
  while(readBtn3) { // start when button 3 is pressed
    TM8.dispStr("btn3", 0);
    TM8.dispStr("strt", 1);
    if (!readBtn4) { // press btn4 to quit
      return 0;
    }
  } while(!readBtn3); // start on rising edge to prevent split recording immediately
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
    TM8.dispDec(raceMinutes * 100 + raceSeconds, 0); // display elapsed time on LCD
    TM8.dispDec(raceMillis * 10 + raceSplitsCounter, 1); // display elapsed milliseconds + split record slot on the right
    if (!readBtn3 && raceSplitsCounter < 100) { // if button 3 is pressed and split record space is available.
      while(!readBtn3) {
        digitalWrite(localLEDs[5], 1); // show split time & light up LED5 while btn3 is depressed
        TM8.dispDec(raceMinutes * 100 + raceSeconds, 0); // display split time
        TM8.dispDec(raceMillis, 1);
      }
      digitalWrite(localLEDs[5], 0); // turn off LED5
      float vavg = (distances[trackSelection]) / (float)((float)(millis() - raceStartTime) / 1000 / 3600);
      TM8.dispDec((int)(vavg), 0);
      TM8.dispDec(((vavg - (int)(vavg)) * 100), 1);
      raceSplits[raceSplitsCounter] = raceMinutes * 100000 + raceSeconds * 1000 + raceMillis;
      raceSplitsCounter++; // increment raceSplitsCounter
      delay(2000);
    }
    if (!readBtn1) {
      while(!readBtn1) {
        TM8.dispDec(rtc.getHours() * 100 + rtc.getMinutes(), 0);
        TM8.dispDec(raceSplitsCounter, 1);
      }
    }
  }
  // quit chronograph animation
  delay(1000);
  TM8.dispStr("quit", 0);
  TM8.dispStr("race", 1);
  delay(1000);
  for (int i=0; i<3; i++) {
    TM8.dispStr("quit", 0);
    TM8.dispStr("race", 1);
    delay(75);
    TM8.dispStr("", 0);
    TM8.dispStr("", 1);
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
  TM8.dispStr("chro", 0); // prompt choice
  TM8.dispStr("race", 1);
  uint8_t chronoCounter = 0;
  uint8_t raceCounter = 0;
  while (readBtn1 && readBtn3); // wait for either btn1 or btn3 input
  if (!readBtn1) { // if chrono records selected
    delay(BUTTON_DELAY);
    while(readBtn4) {
      TM8.dispDec(chronoSplits[chronoCounter] / 1000, 0);
      TM8.dispDec((chronoSplits[chronoCounter] % 1000) * 10 + chronoCounter, 1);
      if (!readBtn1) {
        chronoCounter++;
        if (chronoCounter > 9) {
          chronoCounter = 0;
        }
        Serial.print(chronoSplits[chronoCounter]);
        delay(BUTTON_DELAY);
      } else if (!readBtn2) {
        chronoCounter--;
        if (chronoCounter > 9) {
          chronoCounter = 9;
        }
        Serial.print(chronoSplits[chronoCounter]);
        delay(BUTTON_DELAY);
      }
    }
  } else if (!readBtn3) { // if race records selected
    delay(BUTTON_DELAY);
    while(readBtn4) {
      TM8.dispDec(raceSplits[raceCounter] / 100, 0);
      TM8.dispDec((raceSplits[raceCounter] % 100) * 100 + raceCounter, 1);
      if (!readBtn1) {
        raceCounter++;
        if (raceCounter > 99) {
          raceCounter = 0;
        }
        Serial.print(raceSplits[raceCounter]);
        delay(BUTTON_DELAY);
      } else if (!readBtn2) {
        raceCounter--;
        if (raceCounter > 99) {
          raceCounter = 99;
        }
        Serial.print(raceSplits[raceCounter]);
        delay(BUTTON_DELAY);
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
  while(readBtn3) {
    if (cnt) {
      delay(BUTTON_DELAY);
      TM8.dispStr("OVTA", 0);
      TM8.dispStr("TIME", 1);
    } else {
      delay(BUTTON_DELAY);
      TM8.dispStr("it*s", 0);
      TM8.dispStr(" lit", 1);
    }
    if (!readBtn4) {
      TM8.dispStr("", 0);
      TM8.dispStr("", 1);
      delay(5000);
      TM8.animTach();
    }
    if (!readBtn2) {
      cnt = !cnt;
    }
    if (!readBtn1) {
      delay(5000);
      for (int i=0; i<5; i++) {
        digitalWrite(localLEDs[i], 1);
      }
      delay(5000);
      for (int i=0; i<5; i++) {
        digitalWrite(localLEDs[i], 0);
      }
      return 0;
    }
  }
  return 0;
}

void matchNumbersGame() {
  uint8_t numbers = 0;
  uint8_t hits = 0;
  while (hits < 3) {
  uint8_t target = rand() % 10;  /* generate a number from 0 - 9 */
  uint32_t startTime = millis();
    while(readBtn3) {
      if (numbers > 9) {numbers = 0;}
      if (numbers == 0) {TM8.dispStr("0000", 0);}
      else {TM8.dispDec(numbers * 1111, 0);}
      if (target == 0) {TM8.dispStr("0000", 1);}
      else {TM8.dispDec(target * 1111, 1);}
      if (millis() - startTime >= 200) {
        numbers++;
        startTime = millis();
      }
    }
    if (numbers == target) {
      hits++;
      TM8.dispStr("HIT ", 0);
      TM8.dispDec(hits, 1);
      delay(1000);
      for (int i=0; i<5; i++) {
        TM8.dispStr("HIT ", 0);
        TM8.dispDec(hits, 1);
        delay(50);
        TM8.dispStr("", 0);
        TM8.dispStr("", 1);
        delay(50);
      }
    } else {
      for (int i=0; i<5; i++) {
        TM8.dispStr("MISS", 0);
        TM8.dispStr("MISS", 1);
        tone(9, 4000);
        delay(50);
        TM8.dispStr("", 0);
        TM8.dispStr("", 1);
        noTone(9);
        delay(50);
      }
    }
  }
}

void reactionTimingGame() {

}

/*
F1 reaction game: hit 10 targets consecutively under 350ms reaction time to pass
*/
void F1ReactionGame() {
  int timing;
  uint8_t hits = 0;
  uint8_t corner;
  while (hits <= 10) {
    delay(rand() % 2051 + 2000); // start with a random delay from 2 to 4.5 seconds
    corner = rand() % 4;
    switch (corner) {
      case 0:
        for (int i=0; i<4; i++) {
          TM8.dispCharRaw(i, 0x71, 0);
        }
        break;
      case 1:
        for (int i=0; i<4; i++) {
          TM8.dispCharRaw(i, 0x1E, 0);
        }
        break;
      case 2:
        for (int i=0; i<4; i++) {
          TM8.dispCharRaw(i, 0x71, 1);
        }
        break;
      case 3:
        for (int i=0; i<4; i++) {
          TM8.dispCharRaw(i, 0x1E, 1);
        }
        break;
    }
    bool hit = 0;
    uint32_t startTime = millis();
    while(millis() - startTime <= 500) {
      if (corner == 0 && !readBtn1) {
        hits++;
        hit = 1;
        TM8.dispStr("hit ", 0);
        TM8.dispChar(3, hits, 0);
        TM8.dispDec(millis() - startTime, 1);
        delay(1000);
      } else if (corner == 1 && !readBtn2) {
        hits++;
        hit = 1;
        TM8.dispStr("hit ", 0);
        TM8.dispChar(3, hits, 0);
        TM8.dispDec(millis() - startTime, 1);
        delay(1000);
      } else if (corner == 2 && !readBtn3) {
        hits++;
        hit = 1;
        TM8.dispStr("hit ", 0);
        TM8.dispChar(3, hits, 0);
        TM8.dispDec(millis() - startTime, 1);
        delay(1000);
      } else if (corner == 3 && !readBtn4) {
        hits++;
        hit = 1;
        TM8.dispStr("hit ", 0);
        TM8.dispChar(3, hits, 0);
        TM8.dispDec(millis() - startTime, 1);
        delay(1000);
      }
    }
    if (!hit) {
      TM8.dispStr("TIME", 0);
      TM8.dispStr(" OUT", 1);
      delay(1000);
    }
    hit = 0;
    TM8.Command(LCD_CLEAR, 0);
    TM8.Command(LCD_CLEAR, 1);
  }
}

void game() {
  uint8_t gameNo = 1;
  while(readBtn3) {
    TM8.dispStr("GAME", 0);
    TM8.dispDec(gameNo, 1);
    if (!readBtn1) {gameNo++;delay(BUTTON_DELAY);}
    if (!readBtn2) {gameNo--;delay(BUTTON_DELAY);}
    if (gameNo < 1) {gameNo = 3;}
    if (gameNo > 3) {gameNo = 1;}
  }
  for (int i=3; i>0; i--) {
    TM8.animSwipeDown(30);
  }
  switch (gameNo) {
  case 1:
    matchNumbersGame();
    break;
  case 2:
    reactionTimingGame();
    break;
  case 3:
    F1ReactionGame();
    break;
  default:
    break;
  }
}

void flashLight() {
  bool flash = 0;
  TM8.dispStr("", 0);
  TM8.dispStr("", 1);
  while(readBtn1) {
    if (!readBtn4) {
      flash = !flash;
      delay(BUTTON_DELAY);
    }
    if (!readBtn3) {
      digitalWrite(6, !readBtn3);
    }
    // digitalWrite(6, flash);
    for (int i=0; i<5; i++) {
      digitalWrite(localLEDs[i], flash);
    }
  }
  digitalWrite(6, 0);
}

void showBMEData(uint16_t dispDelay) {
	bme.setOpMode(BME68X_FORCED_MODE);
	delayMicroseconds(bme.getMeasDur());
  
	if (bme.fetchData()) {
		bme.getData(BMEData);
    int temp = (int)BMEData.temperature;
    int hum = (int)BMEData.humidity;
		TM8.dispDec(temp, 0);
    TM8.dispDec(hum, 1);
	}

  delay(dispDelay);
}

void showAccelData(uint16_t dispDelay) {
  int xAxis = round(accel.readFloatAccelX() * 10);
  int yAxis = round(accel.readFloatAccelY() * 10);
  int zAxis = round(accel.readFloatAccelZ() * 10);

  TM8.dispDec(xAxis, 0);
  TM8.dispDec(yAxis, 1);

  delay(dispDelay);
}

void showTelemetry() {
  while (readBtn4) {
    TM8.dispStr("temp", 0);
    TM8.dispStr("accl", 1);
    if (!readBtn1) {
      while (readBtn4) {
        showBMEData(1000);
      }
    } else if (!readBtn3) {
      while (readBtn4) {
        showAccelData(100);
      }
    }
  }
}

uint8_t configure() {
  detachInterrupt(btn3);
  while(readBtn4) {
    dispMode ? TM8.dispStr("aod ", 0) : TM8.dispStr("wake", 0);
    if (!readBtn3) {
      dispMode = !dispMode;
      delay(BUTTON_DELAY);
    }
  }
  attachInterrupt(btn3, menuInt, FALLING);
  return 0;
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
  detachInterrupt(btn1);
  while (millis() - startTime <= INACTIVITY_TIMEOUT) { // while under timeout threshold
    TM8.dispDec(mainProgramNumber, 0); // display program number on the left, but it starts from 1, not 0
    TM8.dispStr(mainPrograms[mainProgramNumber], 1); // display program name on the right
    if (!readBtn1) { // if button 1 is pressed
      mainProgramNumber++; // increment main program counter and select next program
      if (mainProgramNumber >= numPrograms) { // roll back to program 0 after going through entire list
        mainProgramNumber = 1;
      }
      delay(BUTTON_DELAY); // short delay to prevent crazy fast scrolling
      startTime = millis(); // reset timer to 0 to extend time
    } else if (!readBtn2) { // if button 2 is pressed
      mainProgramNumber--; // increment main program counter and select next program
      if (mainProgramNumber >= numPrograms) { // roll back to program 0 after going through entire list
        mainProgramNumber = numPrograms - 1;
      }
      delay(BUTTON_DELAY); // short delay to prevent crazy fast scrolling
      startTime = millis(); // reset timer to 0 to extend time
    } else if (!readBtn3) { // if button 3 is pressed
      for (int i=0; i<3; i++) { // blink selected program 3 times on the display
        TM8.dispStr("", 0);
        TM8.dispStr("", 1);
        delay(50);
        TM8.dispDec(mainProgramNumber, 0);
        TM8.dispStr(mainPrograms[mainProgramNumber], 1);
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

//char mainPrograms[9][5] = {"quit", "Chro", "data", "Adju", "accl", "temp", "Race", "Flsh", "prty"};
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
    showTelemetry();
    break;
  case 6:
    raceChrono();
    break;
  case 7:
    flashLight();
    break;
  case 8:
    game();
    break;
  default:
    TM8.dispStr("Fuck", 0);
    delay(1000);
    break;
  }
  TM8.scrambleAnim(8, 30);
}

uint8_t firingOrder[] = {1, 5, 3, 7, 4, 8, 2, 6};

uint8_t starter() {
  for (int i=0; i<3; i++) {
    TM8.animSwipeDown(30);
  }

  uint8_t battLvl = (uint8_t) fuel.cellPercent();
  if (battLvl > 99) battLvl = 99;
  if (battLvl <= 10 && battLvl >= 0) {
    for (int i=0; i<3; i++) {
      TM8.dispStr(" NO ", 0);
      TM8.dispStr("FUEL", 1);
      delay(300);
      TM8.dispStr("", 0);
      TM8.dispStr("", 1);
      delay(300);
    }
  }
  uint16_t cnt = 0;
  uint8_t firingAnimCount = 0;
  int firingDelay = 50;
  while(1) {
    if (!readBtn3 && !readBtn1) {
      firingAnimCount = 0;
      while(!readBtn3 && !readBtn1) {
        cnt++;
        tone(9, cnt * 20 + 500);
        uint8_t graph = cnt / 40;
        switch (graph) {
          case 0:
            TM8.dispStr("", 0);
            TM8.dispStr("", 1);
            break;
          case 1:
            TM8.dispStr("8", 0);
            TM8.dispStr("", 1);
            break;
          case 2:
            TM8.dispStr("88", 0);
            TM8.dispStr("", 1);
            break;
          case 3:
            TM8.dispStr("888", 0);
            TM8.dispStr("", 1);
            break;
          case 4:
            TM8.dispStr("8888", 0);
            TM8.dispStr("", 1);
            break;
          case 5:
            TM8.dispStr("8888", 0);
            TM8.dispStr("8", 1);
            break;
          case 6:
            TM8.dispStr("8888", 0);
            TM8.dispStr("88", 1);
            break;
          case 7:
            TM8.dispStr("8888", 0);
            TM8.dispStr("888", 1);
            break;
          case 8:
            TM8.dispStr("8888", 0);
            TM8.dispStr("8888", 1);
            break;
        }
        if (cnt == 320) {
          delay(50);
          for (int i=0; i<5; i++) {
            TM8.dispStr("PROJ", 0);
            TM8.dispStr("ECT4", 1);
            tone(9, 4000);
            delay(60);
            TM8.dispStr("", 0);
            TM8.dispStr("", 1);
            noTone(9);
            delay(60);
          }
          noTone(9);
          return 0;
        }
      }
    } else if (readBtn3 || readBtn1) {
      while(cnt > 0) {
        cnt--;
        tone(9, cnt * 20 + 500);
        uint8_t graph = cnt / 50;
        switch (graph) {
          case 0:
            TM8.dispStr("", 0);
            TM8.dispStr("", 1);
            break;
          case 1:
            TM8.dispStr("8", 0);
            TM8.dispStr("", 1);
            break;
          case 2:
            TM8.dispStr("88", 0);
            TM8.dispStr("", 1);
            break;
          case 3:
            TM8.dispStr("888", 0);
            TM8.dispStr("", 1);
            break;
          case 4:
            TM8.dispStr("8888", 0);
            TM8.dispStr("", 1);
            break;
          case 5:
            TM8.dispStr("8888", 0);
            TM8.dispStr("8", 1);
            break;
          case 6:
            TM8.dispStr("8888", 0);
            TM8.dispStr("88", 1);
            break;
          case 7:
            TM8.dispStr("8888", 0);
            TM8.dispStr("888", 1);
            break;
          case 8:
            TM8.dispStr("8888", 0);
            TM8.dispStr("8888", 1);
            break;
        }
        if (cnt <= 0) {
          noTone(9);
        }
      }
    }
    if (readBtn3 && cnt == 0) {
      firingAnimCount++;
      for (int i=0; i<8; i++) {
        uint8_t index = firingOrder[i] - 1;
        if (index >= 4) {
          TM8.dispChar(index - 4, '-', 1);
          delay(firingDelay);
          TM8.dispCharRaw(index - 4, 0x3B, 1);
          delay(firingDelay);
          TM8.dispChar(index - 4, '0', 1);
          delay(firingDelay);
          TM8.dispCharRaw(index - 4, 0x44, 1);
          delay(firingDelay);
          TM8.Command(LCD_CLEAR, 1);
          delay(firingDelay);
        } else {
          TM8.dispChar(index, '-', 0);
          delay(firingDelay);
          TM8.dispCharRaw(index, 0x3B, 0);
          delay(firingDelay);
          TM8.dispChar(index, '0', 0);
          delay(firingDelay);
          TM8.dispCharRaw(index, 0x44, 0);
          delay(firingDelay);
          TM8.Command(LCD_CLEAR, 0);
          delay(firingDelay);
        }
      }
    }
    if (firingAnimCount >= 3) {
      TM8.dispStr("OVTA", 0);
      TM8.dispStr("TIME", 1);
      firingAnimCount = 0;
      LowPower.deepSleep();
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

void wakeToCheck() { 
  while (1) { // loop forever, "home screen" if you will
    // if menuInt() ISR is called, show time, and if pressed again(double click), enter menu.
    // goes back to sleep after 2 seconds
    if (menuActive) {
      while(!readBtn3);
      for (int i=0; i<8; i++) {
        uint32_t startTime = millis();
        TM8.dispDec(random() % 9000 + 1000, 0);
        TM8.dispDec(random() % 9000 + 1000, 1);
        if (!readBtn3 && millis() - startTime <= 200) {
          runMainProgram(mainMenu());
          attachInterrupt(btn3, menuInt, FALLING); // reattach interrupt to resume normal button function in main()
          attachInterrupt(btn1, showDateInt, FALLING);
          menuActive = false; // reset menuActive to false
          showDateActive = false;
        }
        delay(50);
      }
      if (menuActive) {
        uint32_t timeWhenTriggered = millis();
        while (millis() - timeWhenTriggered <= 2000) {
          uint8_t battLvl = (uint8_t) fuel.cellPercent();
          if (battLvl > 99) battLvl = 99;

          // LCD displays hours and minutes on the left, seconds on the right
          TM8.dispDec(rtc.getHours() * 100 + rtc.getMinutes(), 0);
          TM8.dispDec(rtc.getSeconds() * 100 + battLvl, 1);
        }
      }
      menuActive = false;
    } else if (showDateActive) {
      TM8.scrambleAnim(8, 30);
      TM8.dispDec(rtc.getMonth() * 100 + rtc.getDay(), 0);
      TM8.dispStr(daysOfTheWeek[getDayOfWeek(rtc.getYear() + 2000, rtc.getMonth(), rtc.getDay())], 1);
      delay(1000);
      showDateActive = false;
    }
    TM8.scrambleAnim(8, 30);
    TM8.dispStr("ovta", 0);
    TM8.dispStr("time", 1);
    USBDevice.detach();
    LowPower.deepSleep();
  }
}

void alwaysOnDisplay() {
  for(;;) { // loop forever, "home screen" if you will
    //if menuInt() ISR is called, pull up menu and run chosen main program 
    if (menuActive) {
      TM8.scrambleAnim(8, 30);
      runMainProgram(mainMenu());
      attachInterrupt(btn3, menuInt, FALLING); // reattach interrupt to resume normal button function in main()
      attachInterrupt(btn1, showDateInt, FALLING);
      menuActive = false; // reset menuActive to false
    } else if (showDateActive) {
      TM8.scrambleAnim(8, 30);
      uint32_t startTime = millis();
      while (millis() - startTime <= 1000) {
        TM8.dispDec(rtc.getMonth() * 100 + rtc.getDay(), 0);
        TM8.dispStr(daysOfTheWeek[getDayOfWeek(rtc.getYear() + 2000, rtc.getMonth(), rtc.getDay())], 1);
        if (!readBtn1 && millis() - startTime <= 200) {
          setDate();
          attachInterrupt(btn1, showDateInt, FALLING);
          showDateActive = false;
        }
      }
      TM8.scrambleAnim(8, 30);
      showDateActive = false;
    } else if (actionActive) {
      while(!digitalRead(btn2));
      TM8.mouseJiggler(btn2);
      attachInterrupt(btn2, actionInt, FALLING);
      actionActive = false;
    }

    uint8_t battLvl = (uint8_t) fuel.cellPercent();
    if (battLvl > 99) battLvl = 99;

    // LCD displays hours and minutes on the left, seconds on the right
    TM8.dispDec(rtc.getHours() * 100 + rtc.getMinutes(), 0);
    TM8.dispDec(rtc.getSeconds() * 100 + battLvl, 1); 
    USBDevice.detach();
    LowPower.deepSleep(996);
  }
}

/*
system initialization.
initializes all peripherals and confirms on LCD display after each peripheral set-up.
Order: rtc, LCD, I2C & SERCOM2, MAX17048, LIS3DH, BME680, I/O mode &direction, disable unnecessary peripherals
Finally, starts starter() and after that, runs second I2C echo check

Hold btn4 during boot to skip starter() and second I2C check, to save battery
*/
void setup() {
  rtc.begin(); // fire up RTC

  // set RTC time
  rtc.setHours(hours);
  rtc.setMinutes(minutes);
  rtc.setSeconds(seconds);
  rtc.setDate(day, month, year);

  // initialize LCDs
  TM8.init_lcd();

  // start both I2C buses
  Wire.begin();
  wire1.begin();

  // pinPeripheral(4, PIO_SERCOM); // SDA: D4 / PA08
  // pinPeripheral(3, PIO_SERCOM); // SCL: D3 / PA09

  TM8.dispStr("0nrg", 0);

  TM8.dispStr("I2C", 0); // confirm I2C initialization
  delay(50);

  // start MAX17048 fuel gauge
  if (!fuel.begin()) {
    TM8.dispStr(" FF ", 0); // Fuel Fail error on LCD
    for (int i=0; i<3; i++) {
      tone(9, 4000);
      delay(100);
      noTone(9);
      delay(100);
    }
    delay(2000);
  }
  TM8.dispStr("FUEL", 0); // confirms fuel sensor init
  delay(50);

  // start LIS3DH accelerometer
  if (accel.begin() != IMU_SUCCESS) {
    TM8.dispStr("ACCL", 0); // fail message on LCD
    TM8.dispStr("FAIL", 1);
    for (int i=0; i<3; i++) {
      tone(9, 4000);
      delay(100);
      noTone(9);
      delay(100);
    }
    delay(2000);
  }
  TM8.dispStr("ACCL", 0); // confirms accelerometer init
  TM8.dispStr("INIT", 1);
  delay(50);

  // rom.begin(0x50, wire1);

  // start BME680 enviro sensor
  bme.begin(BME_ADDRESS, wire1);
	/* Set the default configuration for temperature, pressure and humidity */
	bme.setTPH();
	/* Set the heater configuration to 300 deg C for 100ms for Forced mode */
	bme.setHeaterProf(300, 100);

  if (bme.checkStatus() != BME68X_OK) {
    TM8.dispStr("BME ", 0); // fail message on LCD
    TM8.dispStr("FAIl", 1);
    for (int i=0; i<3; i++) {
      tone(9, 4000);
      delay(100);
      noTone(9);
      delay(100);
    }
    delay(2000);
  }
  TM8.dispStr("BME ", 0); // confirms BME init
  TM8.dispStr("INIT", 1);
  delay(50);

  // enable pullups on all inputs to prevent floating
  pinMode(btn1, INPUT_PULLUP); // button 1, top left
  pinMode(btn2, INPUT_PULLUP); // button 2, bottom left
  pinMode(btn3, INPUT_PULLUP); // button 3, top right

  pinMode(6, OUTPUT); // flashlight
  pinMode(9, OUTPUT); // piezo

  for (int i=0; i<5; i++) { // set LEDs to output
    pinMode(localLEDs[i], OUTPUT);
  }

  // enable pullups for PA12, sets it to input, writes HIGH to it
  // this is the only way to configure PA12 without having it freeze TM8
  PORT->Group[0].PINCFG[12].reg = PORT_PINCFG_PULLEN | PORT_PINCFG_INEN;
  PORT->Group[0].OUTSET.reg = PORT_PA12;

  // TM8.dispStr("IO D", 0);
  // TM8.dispStr(" set", 1);

  // set button 3 (top right) to open main menu
  // set to FALLING because RISING would often trigger the interrupt but not actually run the ISR,
  // leading to systemw-wide clock delays
  LowPower.attachInterruptWakeup(btn1, showDateInt, FALLING);
  //LowPower.attachInterruptWakeup(btn2, actionInt, FALLING); // BTN2 is the "action buttton", programmable
  attachInterrupt(btn2, actionInt, FALLING); // for some reason, LowPower.attachInterruptWakeup does not work!!! No idea why!!!
  attachInterrupt(btn4, actionInt, FALLING);
  LowPower.attachInterruptWakeup(btn3, menuInt, FALLING);

  // disable all unnecessary peripherals
  SERCOM0->USART.CTRLA.bit.ENABLE=0;
  SERCOM1->USART.CTRLA.bit.ENABLE=0;
  SERCOM4->USART.CTRLA.bit.ENABLE=0;
  SERCOM5->USART.CTRLA.bit.ENABLE=0;
  SERCOM0->SPI.CTRLA.bit.ENABLE=0;
  SERCOM1->SPI.CTRLA.bit.ENABLE=0;
  SERCOM4->SPI.CTRLA.bit.ENABLE=0;
  SERCOM5->SPI.CTRLA.bit.ENABLE=0;
  SERCOM0->I2CM.CTRLA.bit.ENABLE=0;
  SERCOM1->I2CM.CTRLA.bit.ENABLE=0;
  SERCOM4->I2CM.CTRLA.bit.ENABLE=0;
  SERCOM5->I2CM.CTRLA.bit.ENABLE=0;
  SERCOM0->I2CS.CTRLA.bit.ENABLE=0;
  SERCOM1->I2CS.CTRLA.bit.ENABLE=0;
  SERCOM4->I2CS.CTRLA.bit.ENABLE=0;
  SERCOM5->I2CS.CTRLA.bit.ENABLE=0;
  I2S->CTRLA.bit.ENABLE=0;
  ADC->CTRLA.bit.ENABLE=0;
  DAC->CTRLA.bit.ENABLE=0;
  AC->CTRLA.bit.ENABLE=0;

  // confirm IO direction init
  TM8.dispStr("IO d", 0);
  TM8.dispStr(" set", 1);
  delay(50);

  //if BTN4 isn't pressed, run starter() and second system init
  if (readBtn4 || fuel.cellPercent() <= 10) {
    starter();
    TM8.sysCheck();
  }
  menuActive = false; // for some reason starter() triggers menuInt(), so I need this to prevent the watch from booting straight into the menu
  showDateActive = false; // same reason
  actionActive = false;
}

// main function
void loop() {
  alwaysOnDisplay();
}
