//----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "Arduino.h"

#include "Wire.h"
#include "wiring_private.h" // pinPeripheral() function

TwoWire Wire1(&sercom2, 4, 3);

#define I2C_ADDR  0x38

#define CMD_MODE_SET  0xCD
#define CMD_LOAD_DP   0x80
#define CMD_DEVICE_SEL  0xE0
#define CMD_BANK_SEL  0xF8
#define CMD_NOBLINK   0x70
#define CMD_BLINK   0x71

//----------------------------------------------------------------------------

#define NUM_LCD_DIGITS 8
#define CDM4101_NUM_DIGITS 8

//----------------------------------------------------------------------------
// LCD commands.

#define CDM4101_BLINK_OFF 6
#define CDM4101_BLINK_ON  7
#define CDM4101_CLEAR     8

//----------------------------------------------------------------------------

#define DASH            10
#define UNDERSCORE      11
#define SPACE           12
#define ASTERISK        39
#define ALPHA_START     13

#define HOLD_TIME   8

byte Digits[CDM4101_NUM_DIGITS];
byte Blink;
byte Ctr;

//----------------------------------------------------------------------------

unsigned char const Segs[] =
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
  0x77, // G
  0x3B, // H
  0x02, // i
  0x0F, // J
  0x38, // K - can't do
  0x2C, // L
  0x5A, // M - can't do
  0x1A, // n
  0x6F, // O
  0x79, // P
  0x75, // Q - can't do
  0x18, // r
  0x76, // S
  0x3C, // t
  0x0E, // u
  0x2F, // V - can't do
  0x4E, // W - can't do
  0x2B, // X - can't do
  0x37, // y
  0x5D, // Z

};

unsigned char LCD_Digits[NUM_LCD_DIGITS];
unsigned char LCD_Blink;

//----------------------------------------------------------------------------

void Update(int n)
{
  if (n == 1) {
    byte data[5]; // bytes to send display segments

    if(Ctr)
    {
      Ctr--;
      return;
    }

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
  }
  
  else if (n == 2) {
    byte data[5]; // bytes to send display segments

    if(Ctr)
    {
      Ctr--;
      return;
    }

    Wire1.beginTransmission(I2C_ADDR);
    Wire1.write(CMD_MODE_SET);
    Wire1.write(CMD_LOAD_DP);
    Wire1.write(CMD_DEVICE_SEL);
    Wire1.write(CMD_BANK_SEL);


    if(Blink) Wire1.write(CMD_BLINK);
    else      Wire1.write(CMD_NOBLINK);

  #if 1
    data[0] = (Digits[0] >> 4); // || LCD_BAR
    data[1] = (Digits[0] << 4) | (Digits[1] >> 3);
    data[2] = (Digits[1] << 5) | (Digits[2] >> 2);
    data[3] = (Digits[2] << 6) | (Digits[3] >> 1);
    data[4] = (Digits[3] << 7);
  #else
    Wire1.write(0x70);
    Wire1.write(0x3B);
    Wire1.write(0xB5);
    Wire1.write(0xD9);
    Wire1.write(0x80);
  #endif

    for(int i=0;i<5;i++) Wire1.write(data[i]);

    Wire1.endTransmission();
  }
}

//----------------------------------------------------------------------------

void Init1(void)
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

  Ctr = 0;
}

void Init2(void)
{
  Blink = 0;
  
  Digits[0] = Segs[SPACE];
  Digits[1] = Segs[SPACE];
  Digits[2] = Segs[SPACE];
  Digits[3] = Segs[SPACE];
  Wire1.beginTransmission(I2C_ADDR);
  Wire1.write(CMD_MODE_SET);
  Wire1.write(CMD_LOAD_DP);
  Wire1.write(CMD_DEVICE_SEL);
  Wire1.write(CMD_BANK_SEL);
  Wire1.write(CMD_NOBLINK);
  Wire1.write(0x05);
  Wire1.write(0xD5);
  Wire1.write(0x9B);
  Wire1.write(0xFF);
  Wire1.write(0x00);

  Wire1.endTransmission();

  Ctr = 0;
}

//----------------------------------------------------------------------------

void Command(byte cmd, int n)
{
  switch(cmd) 
  {
    case CDM4101_BLINK_OFF :
      Blink = 0;
      Update(n);
      break;

    case CDM4101_BLINK_ON :
      Blink = 1;
      Update(n);
      break;

    case CDM4101_CLEAR :
      Digits[0] = Segs[SPACE];
      Digits[1] = Segs[SPACE];
      Digits[2] = Segs[SPACE];
      Digits[3] = Segs[SPACE];

      Update(n);
      break;

    default :
      break;
  }
}

//----------------------------------------------------------------------------

char ConvertChar(char c)
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

void DispChar(byte index, char c, int n)
{
  Digits[(int)index] = Segs[(int)(ConvertChar(c))];
  Update(n);
}

//----------------------------------------------------------------------------

void dispStr(char *s, int n)
{
  byte i,c;

  for(i=0;i<CDM4101_NUM_DIGITS;i++) Digits[i] = Segs[SPACE];

  i = 0;

  while((i < 4) && s[i])
  {
    c = Segs[(int)(ConvertChar(s[i]))];

    Digits[i] = c;
    i++;
  }
  
  Update(n);
}

//----------------------------------------------------------------------------
// display a decimal value, right justified.

void DispDec(short n, int num)
{
  byte i;
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

  dispStr(str, num);
}

//----------------------------------------------------------------------------

int ctr=0;

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire1.begin();
  pinPeripheral (2,PIO_SERCOM_ALT);
  pinPeripheral (3,PIO_SERCOM_ALT);
  Init1();
  Init2();
}

void loop() {
  dispStr("    ", 1);
  dispStr("    ", 2);
  delay(3000);
  while (1) {
  dispStr("Stud", 1);
  dispStr("y", 2);
  delay(500);
  dispStr("HArd", 1);
  dispStr("    ", 2);
  delay(500);
  dispStr("PArt", 1);
  dispStr("y", 2);
  delay(500);
  dispStr("Hard", 1);
  dispStr("Er", 2);
  delay(500);
  }
}
