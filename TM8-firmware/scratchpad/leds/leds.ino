/*
  Blink
  Turns on an LED on for one second, then off for one second, repeatedly.

  This example code is in the public domain.
 */

// Pin 13 has an LED connected on most Arduino boards.
// Pin 11 has the LED on Teensy 2.0
// Pin 6  has the LED on Teensy++ 2.0
// Pin 13 has the LED on Teensy 3.0
// give it a name:

int leds[] = {7, A1, 8, A3, 5};

// the setup routine runs once when you press reset:
void setup() {
  for (int i=0; i<5; i++) {
    pinMode(leds[i], OUTPUT);
  }
}

// the loop routine runs over and over again forever:
void loop() {
  for (int i=0; i<5; i++) {
    digitalWrite(leds[i], 1);
  }

  delay(1000);
}
