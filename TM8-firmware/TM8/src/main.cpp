#include <Arduino.h>
#include <Wire.h>
#include "wiring_private.h" // pinPeripheral() function
#include <cdm4101.h>
#include <stdio.h>
#include <RTCZero.h>

TwoWire wire1(&sercom2, 4, 3); // new Wire object to set up second I2C port on SERCOM 2

CDM4101 lcd; // LCD object

RTCZero rtc; // RTC object

const uint8_t btn1 = 2; // top left button
const uint8_t btn2 = 38; // bottom left button
const uint8_t btn3 = 24; // top right button
const uint8_t btn4 = 21; // bottom right button, doesn't work for some reason

bool menuActive = false; // main Menu ISR handler variable, becomes true when main menu is opened and false when menu's over

uint8_t mainProgramNumber = 0; // counter variable for scrolling through list of programs in main menu

// list of programs in main menu 8 elements long with each element 5 bytes (4 bytes + line end)
char mainPrograms[8][5] = {"Chro", "Timr", "Alrm", "Adju", "Prty", "Race", "temp", "Flsh"};

// calculate size of main programs list
uint8_t mainProgramsCount = sizeof(mainPrograms) / sizeof(mainPrograms[0]);

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

/*
Main menu function.
holds a number of "main programs" that can be quickly accessed in the main menu.
BTN3 scrolls through list of programs, hold button to scroll quickly
BTN1 runs selected program
After approx. 2 seconds of inactivity, mainMenu() returns 0 and goes back to home screen.
*/
uint8_t mainMenu() {
  uint16_t timeOut = 2000; // inactivity threshold of 2 seconds
  uint8_t buttonDelay = 200; // delay between button readings for switching between programs
  uint32_t startTime = millis(); // time when function starts
  detachInterrupt(btn3); // detach interrupts for button use during function
  delay(200); // short delay to not start fast-scrolling right as main menu is called
  while (millis() - startTime <= timeOut) { // while under timeout threshold
    lcd.dispDec(mainProgramNumber, 0); // display program number on the left
    lcd.dispStr(mainPrograms[mainProgramNumber], 1); // display program name on the right
    if (!digitalRead(btn3)) { // if button 3 (top right) is pressed
      mainProgramNumber++; // increment main program counter and select next program
      if (mainProgramNumber >= mainProgramsCount) { // roll back to program 0 after going through entire list
        mainProgramNumber = 0;
      }
      delay(buttonDelay); // short delay to prevent crazy fast scrolling
      startTime = millis(); // reset timer to 0 to extend time
    }
    if (!digitalRead(btn1)) { // if button 1 (top left) is pressed
      for (int i=0; i<3; i++) { // blink selected program 3 times on the display
        lcd.dispStr("", 0);
        lcd.dispStr("", 1);
        delay(50);
        lcd.dispDec(mainProgramNumber, 0);
        lcd.dispStr(mainPrograms[mainProgramNumber], 1);
        delay(50);
      }
      delay(500); // half-second delay
      return 0; // end mainMenu()
    }
  }
  return 0; // end mainMenu()
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
  // button 4 doesn't work for some reason

  // set button 3 (top right) to open main menu
  attachInterrupt(btn3, menuInt, FALLING);

  // initialize LCDs
  lcd.init_lcd();
}

// main function
void loop() {
  // LCD displays hours and minutes on the left, seconds on the right
  lcd.dispDec(rtc.getHours() * 100 + rtc.getMinutes(), 0);
  lcd.dispDec(rtc.getSeconds(), 1);

  // if menuInt() ISR is called, go to mainMenu()
  if (menuActive) {
    mainMenu();
    attachInterrupt(btn3, menuInt, FALLING); // reattach interrupt to resume normal button function in main()
    menuActive = false; // reset menuActive to false
  }
}