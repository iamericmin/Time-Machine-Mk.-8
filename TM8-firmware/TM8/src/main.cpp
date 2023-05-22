#include <Arduino.h>
#include <Wire.h>
#include "wiring_private.h" // pinPeripheral() function
#include <cdm4101.h>
#include <stdio.h>
#include <RTCZero.h>

#define INACTIVITY_TIMEOUT 2000 // inactivity threshold of 2 seconds
#define BUTTON_DELAY 200 // delay between button readings for switching between programs

TwoWire wire1(&sercom2, 4, 3); // new Wire object to set up second I2C port on SERCOM 2

CDM4101 lcd; // LCD object

RTCZero rtc; // RTC object

const uint8_t btn1 = 2; // top left button
const uint8_t btn2 = 38; // bottom left button
const uint8_t btn3 = 24; // top right button

const uint8_t led5 = 5; // LED5, middle auxilliary LED

// I found after lots of trial and error this is the only way to get TM8 to read PA12
// without the whole thing freezing or acting up
// returns 0 when closed, 1 when open
#define readBtn4 (PORT->Group[0].IN.reg & (1 << 12))

bool menuActive = false; // main Menu ISR handler variable, becomes true when main menu is opened and false when menu's over
bool splitActive = false;

// list of programs in main menu 8 elements long with each element 5 bytes (4 bytes + line end)
// first program is "quit", gets called when mainMenu() quits from no activity. Returns to main() loop.
char mainPrograms[9][5] = {"quit", "Chro", "data", "Alrm", "Adju", "Prty", "Race", "temp", "Flsh"};

#define NUM_MAIN_PROGRAMS 9 // number of main programs

// RTC time variables
uint8_t seconds = 0;
uint8_t minutes = 29;
uint8_t hours = 12;

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
BTN3: record split. slots 0-9, automatic rollover. record slot shown on rightmost digit.
*/
void chronoGraph() {
  bool canRecordSplit = 1; // becomes false if all 10 split record slots are full
  attachInterrupt(btn3, recordSplitInt, FALLING);
  uint8_t chronoSplitsCounter = 0;
  while(readBtn4) { // start when button 4 is pressed
    lcd.dispStr("btn4", 0);
    lcd.dispStr("strt", 1);
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
    if (splitActive) { // if button 3 is pressed, show split time for 3 seconds
      while(!digitalRead(btn3)) {digitalWrite(led5, canRecordSplit);} // show split time & light up LED5 while btn3 is depressed
      digitalWrite(led5, 0); // turn off LED5
      splitActive = false; // reset split record ISR trigger bool
      lcd.dispDec(chronoMinutes * 100 + chronoSeconds, 0); // display split time
      lcd.dispDec(chronoMillis * 10 + chronoSplitsCounter, 1);
      if (chronoSplitsCounter < 10) { // if split record slots are left, store split time in chronoSplits[] and increment counter
        chronoSplits[chronoSplitsCounter] = chronoMinutes * 100000 + chronoSeconds * 1000 + chronoMillis;
        chronoSplitsCounter++;
      } else {
        canRecordSplit = 0;
      }
    }
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
  detachInterrupt(btn3); // detatch temporary btn3 ISR used for chronograph
}

/*
retrieves chronograph split records
first prompts whether to retrieve records from chrono or race
user selects btn1 for chrono, btn3 for race
*/
uint8_t chronoData() {
  lcd.dispStr("chro", 0); // prompt choice
  lcd.dispStr("race", 1);
  while (digitalRead(btn1) && digitalRead(btn3)); // wait for either btn1 or btn3 input
  if (!digitalRead(btn1)) { // if chrono records selected
    for (int i=0; i<10; i++) {
      Serial.print(chronoSplits[i] / 1000);
      Serial.print(" ||||| ");
      Serial.println(chronoSplits[i] % 1000);
      lcd.dispDec(chronoSplits[i] / 1000, 0);
      lcd.dispDec(chronoSplits[i] % 1000, 1);
      delay(1000);
    }
  } else if (!digitalRead(btn3)) { // if race records selected
    return 0;
  }
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
    }
    if (!readBtn4) { // if button 4 (bottom right) is pressed
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
  case 4:
    setTime();
    break;
  default:
    lcd.dispStr("Fuck", 0);
    delay(1000);
    break;
  }
}

// system initialization
void setup() {
  rtc.begin(); // fire up RTC

  // set RTC time
  rtc.setHours(hours);
  rtc.setMinutes(minutes);
  rtc.setSeconds(seconds);

  // start both I2C buses
  Wire.begin();
  wire1.begin();

  pinPeripheral(4, PIO_SERCOM_ALT); // SDA: D4 / PA08
  pinPeripheral(3, PIO_SERCOM_ALT); // SCL: D3 / PA09

  // enable pullups on all inputs to prevent floating
  pinMode(btn1, INPUT_PULLUP); // button 1, top left
  pinMode(btn2, INPUT_PULLUP); // button 2, bottom left
  pinMode(btn3, INPUT_PULLUP); // button 3, top right

  pinMode(led5, OUTPUT);

  // enable pullups for PA12 and set it as output
  // this is the only way to configure PA12 without having it freeze TM8
  PORT->Group[0].PINCFG[12].reg = PORT_PINCFG_PULLEN | PORT_PINCFG_INEN;
  PORT->Group[0].OUTSET.reg = PORT_PA12;

  // set button 3 (top right) to open main menu
  // set to FALLING because RISING would often trigger the interrupt but not actually run the ISR,
  // leading to systemw-wide clock delays
  attachInterrupt(btn3, menuInt, FALLING);

  // initialize LCDs
  lcd.init_lcd();

  Serial.begin(115200); // for debug reasons
}

// main function
void loop() {
  // LCD displays hours and minutes on the left, seconds on the right
  lcd.dispDec(rtc.getHours() * 100 + rtc.getMinutes(), 0);
  lcd.dispDec(rtc.getSeconds(), 1);

  // if menuInt() ISR is called, pull up menu and run chosen main program
  if (menuActive) {
    runMainProgram(mainMenu());
    attachInterrupt(btn3, menuInt, FALLING); // reattach interrupt to resume normal button function in main()
    menuActive = false; // reset menuActive to false
  }
  
}