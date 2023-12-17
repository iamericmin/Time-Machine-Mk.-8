#ifndef _TM8_UTIL_H_
#define _TM8_UTIL_H_

#include <inttypes.h>

//----------------------------------------------------------------------------

#define LCD_NUM_DIGITS  4
#define LCD_NUM_SEGS 40

//----------------------------------------------------------------------------
// LCD commands.

#define LCD_BLINK_OFF 6
#define LCD_BLINK_ON  7
#define LCD_CLEAR     8

//----------------------------------------------------------------------------

class TM8_util
{
public:
	void init_lcd(void);
	void Command(uint8_t cmd, bool disp);
	void dispChar(uint8_t index, char c, bool disp);
  void dispCharRaw(uint8_t index, char c, bool disp);
	void dispStr(const char *s, bool disp);
	void dispStrTimed(char *s, bool disp);
	void dispDec(short n, bool disp);
  void blinkGo(bool isGo);

  uint8_t Digits[LCD_NUM_DIGITS];
  //uint8_t Segs[LCD_NUM_SEGS];

	void Update(bool);
	char ConvertChar(char c);

	uint8_t Blink;
	uint8_t Ctr;
};

//----------------------------------------------------------------------------

#endif // _TM8_UTIL_H_
