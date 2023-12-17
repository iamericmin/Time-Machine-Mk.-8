//----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "Wire.h"
#include "wiring_private.h"

#include "TM8_util.h"

#include <Arduino.h>
#include <RTCZero.h>
#include <Adafruit_MAX1704X.h>
#include <ArduinoLowPower.h>
#include <SparkFunLIS3DH.h>
#include <bme68xLibrary.h>
#include <SparkFun_External_EEPROM.h>
#include <time.h>

//----------------------------------------------------------------------------
TwoWire wireTwo(&sercom2, 4, 3); //set up second I2C bus

/*
LCD specific defines
*/
#define I2C_ADDR	(0x38) //hard-wired I2C address of LCD
#define CMD_MODE_SET	0xCD
#define CMD_LOAD_DP		0x80
#define CMD_DEVICE_SEL	0xE0
#define CMD_BANK_SEL	0xF8
#define CMD_NOBLINK		0x70
#define CMD_BLINK		0x71
#define DASH            10
#define UNDERSCORE      11
#define SPACE           12
#define ASTERISK        39
#define ALPHA_START     13
#define HOLD_TIME   8

/*
TM8 hardware definitions
*/
#define LCD_ADDRESS 0x38
#define FUEL_ADDRESS 0x36
#define ACCEL_ADDRESS 0x18
#define BME_ADDRESS 0x76
#define ROM_ADDRESS 0x50

//----------------------------------------------------------------------------

static uint8_t Segs[] =
{
	0x6F, // 0x30 0
	0x03, // 0x31 1
	0x5D, // 0x32 2
	0x57, // 0x33 3
	0x33, // 0x34 4
	0x76, // 0x35 5
	0x7E, // 0x36 6
	0x43, // 0x37 7
	0x7F, // 0x38 8
	0x77, // 0x39 9
	0x10, //  -
	0x04, //  _
	0x00, //  space
	0x7B, // A
	0x3E, // b
	0x6C, // C
	0x1F, // d
	0x7C, // E
	0x78, // F
	0x6E, // G
	0x3A, // H
	0x03, // I
	0x0F, // J
	0x3B, // K - can't do
	0x2C, // L
	0x5A, // M - can't do
	0x6B, // n
	0x6F, // O
	0x79, // P
	0x73, // Q
	0x18, // r
	0x76, // S
	0x3C, // t
	0x0E, // u
	0x2F, // V
	0x35, // W
	0x2B, // X
	0x37, // y
	0x5D, // Z
	0x01, // ° - use * to represent it in your string
};

void TM8_util::Update(bool disp) // if disp is 0, left LCD. if 1, right LCD.
{
	char data[5]; // bytes to send display segments

	if(Ctr)
	{
		Ctr--;
		return;
	}

  if (!disp) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(CMD_MODE_SET);
    Wire.write(CMD_LOAD_DP);
    Wire.write(CMD_DEVICE_SEL);
    Wire.write(CMD_BANK_SEL);


    if(Blink) Wire.write(CMD_BLINK);
    else      Wire.write(CMD_NOBLINK);

  #if 1
    data[0] = (Digits[0] >> 4); // || LCD_BAR
    data[1] = (Digits[0] << 4) | (Digits[1] >> 3);
    data[2] = (Digits[1] << 5) | (Digits[2] >> 2);
    data[3] = (Digits[2] << 6) | (Digits[3] >> 1);
    data[4] = (Digits[3] << 7);
  #else
    Wire.write(0x70);
    Wire.write(0x3B);
    Wire.write(0xB5);
    Wire.write(0xD9);
    Wire.write(0x80);
  #endif

    for(int i=0;i<5;i++) Wire.write(data[i]);

    Wire.endTransmission();
  } else if (disp) {
    wireTwo.beginTransmission(I2C_ADDR);
    wireTwo.write(CMD_MODE_SET);
    wireTwo.write(CMD_LOAD_DP);
    wireTwo.write(CMD_DEVICE_SEL);
    wireTwo.write(CMD_BANK_SEL);


    if(Blink) wireTwo.write(CMD_BLINK);
    else      wireTwo.write(CMD_NOBLINK);

  #if 1
    data[0] = (Digits[0] >> 4); // || LCD_BAR
    data[1] = (Digits[0] << 4) | (Digits[1] >> 3);
    data[2] = (Digits[1] << 5) | (Digits[2] >> 2);
    data[3] = (Digits[2] << 6) | (Digits[3] >> 1);
    data[4] = (Digits[3] << 7);
  #else
    wireTwo.write(0x70);
    wireTwo.write(0x3B);
    wireTwo.write(0xB5);
    wireTwo.write(0xD9);
    wireTwo.write(0x80);
  #endif

    for(int i=0;i<5;i++) wireTwo.write(data[i]);

    wireTwo.endTransmission();
  }
}

void TM8_util::init_lcd(void)
{
	Blink = 0;

	Digits[0] = Segs[SPACE];
	Digits[1] = Segs[SPACE];
	Digits[2] = Segs[SPACE];
	Digits[3] = Segs[SPACE];

	Wire.beginTransmission(I2C_ADDR);
	Wire.write(CMD_MODE_SET);
	Wire.write(CMD_LOAD_DP);
	Wire.write(CMD_DEVICE_SEL);
	Wire.write(CMD_BANK_SEL);
	Wire.write(CMD_NOBLINK);
	Wire.write(0x05);
	Wire.write(0xD5);
	Wire.write(0x9B);
	Wire.write(0xFF);
	Wire.write(0x00);

	Wire.endTransmission();
  
	wireTwo.beginTransmission(I2C_ADDR);
	wireTwo.write(CMD_MODE_SET);
	wireTwo.write(CMD_LOAD_DP);
	wireTwo.write(CMD_DEVICE_SEL);
	wireTwo.write(CMD_BANK_SEL);
	wireTwo.write(CMD_NOBLINK);
	wireTwo.write(0x05);
	wireTwo.write(0xD5);
	wireTwo.write(0x9B);
	wireTwo.write(0xFF);
	wireTwo.write(0x00);

	wireTwo.endTransmission();

	Ctr = 0;
}

void TM8_util::Command(uint8_t cmd, bool disp)
{
	switch(cmd)
	{
		case LCD_BLINK_OFF :
			Blink = 0;
			Update(disp);
			break;

		case LCD_BLINK_ON :
			Blink = 1;
			Update(disp);
			break;

		case LCD_CLEAR :
			Digits[0] = Segs[SPACE];
			Digits[1] = Segs[SPACE];
			Digits[2] = Segs[SPACE];
			Digits[3] = Segs[SPACE];

			Update(disp);
			break;

		default :
			break;
	}
}

char TM8_util::ConvertChar(char c)
{
	if((c >= 'a') && (c <= 'z')) c = c - 'a' + ALPHA_START;
	else
		if((c >= 'A') && (c <= 'Z')) c = c - 'A' + ALPHA_START;
		else
			if((c >= '0') && (c <= '9')) c = c - '0';
			else
				if(c == '-') c = DASH;
				else
					if(c == '_') c = UNDERSCORE;
					else
						if(c == '*') c = ASTERISK;
						else c = SPACE;
	return c;
}

void TM8_util::dispChar(uint8_t index, char c, bool disp)
{
	Digits[(int)index] = Segs[(int)(ConvertChar(c))];
	Update(disp);
}

void TM8_util::dispCharRaw(uint8_t index, char c, bool disp)
{
	Digits[(int)index] = c;
	Update(disp);
}

void TM8_util::dispStr(const char *s, bool disp)
{
	uint8_t i,c;

	for(i=0;i<LCD_NUM_DIGITS;i++) Digits[i] = Segs[SPACE];

	i = 0;

	while((i < 4) && s[i])
	{
		c = Segs[(int)(ConvertChar(s[i]))];

		Digits[i] = c;
		i++;
	}

	Update(disp);
}

void TM8_util::dispStrTimed(char *s, bool disp)
{
	dispStr(s, disp);
	Ctr = HOLD_TIME;
}

void TM8_util::dispDec(short n, bool disp)
{
	uint8_t i;
	char str[5];

	if(n < -999L) n = -999L;
	if(n > 9999L) n = 9999L;

	for(i=0;i<5;i++) str[i] = 0;

	itoa(n, str, 10);

	while(str[3] == 0)
	{
		for(i=3;i>0;i--) str[i] = str[i-1];
		str[0] = ' ';
	}

	dispStr(str, disp);
}

/*
Little helper function for various animations
*/
void TM8_util::blinkGo(bool isGo) {
  for (int i=0; i<5; i++) {
    dispStr("    ", 1);
    noTone(9);
    delay(30);
    for (int i=0; i<4; i++) {
      dispCharRaw(i, 0x54, 1);
    }
    tone(9, random(1, 7) * 1000);
    delay(30);
  }
  isGo ? dispStr(" GO ", 1) : dispStr("Err ", 1);
  noTone(9);
  delay(500);
}