//----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "Wire.h"
#include "wiring_private.h"

#include "cdm4101.h"

//----------------------------------------------------------------------------
TwoWire lcdBus2(&sercom2, 4, 3); //set up second I2C bus

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
	0x1A, // n
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

//----------------------------------------------------------------------------

void CDM4101::Update(bool disp) // if disp is 0, left LCD. if 1, right LCD.
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
    lcdBus2.beginTransmission(I2C_ADDR);
    lcdBus2.write(CMD_MODE_SET);
    lcdBus2.write(CMD_LOAD_DP);
    lcdBus2.write(CMD_DEVICE_SEL);
    lcdBus2.write(CMD_BANK_SEL);


    if(Blink) lcdBus2.write(CMD_BLINK);
    else      lcdBus2.write(CMD_NOBLINK);

  #if 1
    data[0] = (Digits[0] >> 4); // || LCD_BAR
    data[1] = (Digits[0] << 4) | (Digits[1] >> 3);
    data[2] = (Digits[1] << 5) | (Digits[2] >> 2);
    data[3] = (Digits[2] << 6) | (Digits[3] >> 1);
    data[4] = (Digits[3] << 7);
  #else
    lcdBus2.write(0x70);
    lcdBus2.write(0x3B);
    lcdBus2.write(0xB5);
    lcdBus2.write(0xD9);
    lcdBus2.write(0x80);
  #endif

    for(int i=0;i<5;i++) lcdBus2.write(data[i]);

    lcdBus2.endTransmission();
  }
}

//----------------------------------------------------------------------------

void CDM4101::init_lcd(void)
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
  
	lcdBus2.beginTransmission(I2C_ADDR);
	lcdBus2.write(CMD_MODE_SET);
	lcdBus2.write(CMD_LOAD_DP);
	lcdBus2.write(CMD_DEVICE_SEL);
	lcdBus2.write(CMD_BANK_SEL);
	lcdBus2.write(CMD_NOBLINK);
	lcdBus2.write(0x05);
	lcdBus2.write(0xD5);
	lcdBus2.write(0x9B);
	lcdBus2.write(0xFF);
	lcdBus2.write(0x00);

	lcdBus2.endTransmission();

	Ctr = 0;
}

//----------------------------------------------------------------------------

void CDM4101::Command(uint8_t cmd, bool disp)
{
	switch(cmd)
	{
		case CDM4101_BLINK_OFF :
			Blink = 0;
			Update(disp);
			break;

		case CDM4101_BLINK_ON :
			Blink = 1;
			Update(disp);
			break;

		case CDM4101_CLEAR :
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

//----------------------------------------------------------------------------

char CDM4101::ConvertChar(char c)
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

//----------------------------------------------------------------------------

void CDM4101::dispChar(uint8_t index, char c, bool disp)
{
	Digits[(int)index] = Segs[(int)(ConvertChar(c))];
	Update(disp);
}

//----------------------------------------------------------------------------

void CDM4101::dispCharRaw(uint8_t index, char c, bool disp)
{
	Digits[(int)index] = c;
	Update(disp);
}

//----------------------------------------------------------------------------

void CDM4101::dispStr(char *s, bool disp)
{
	uint8_t i,c;

	for(i=0;i<CDM4101_NUM_DIGITS;i++) Digits[i] = Segs[SPACE];

	i = 0;

	while((i < 4) && s[i])
	{
		c = Segs[(int)(ConvertChar(s[i]))];

		Digits[i] = c;
		i++;
	}

	Update(disp);
}

//----------------------------------------------------------------------------

void CDM4101::dispStrTimed(char *s, bool disp)
{
	dispStr(s, disp);
	Ctr = HOLD_TIME;
}


//----------------------------------------------------------------------------
// display a decimal value, right justified.

void CDM4101::dispDec(short n, bool disp)
{
	uint8_t i;
	char  str[5];

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

//----------------------------------------------------------------------------