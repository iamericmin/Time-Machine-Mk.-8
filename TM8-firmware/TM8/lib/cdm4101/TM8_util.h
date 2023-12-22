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

#define TM8_NUM_LEDS 5

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
  void animSwipeDown(uint8_t animDelay);
  void animTach(void);
  void scrambleAnim(uint8_t cnt, uint8_t animDelay);
  void sysCheck();
  void HIDutils(uint8_t btn);

  uint8_t digits[LCD_NUM_DIGITS];
  uint8_t leds[TM8_NUM_LEDS];
  //uint8_t Segs[LCD_NUM_SEGS];

	void Update(bool);
	char ConvertChar(char c);

	uint8_t Blink;
	uint8_t Ctr;
};

//----------------------------------------------------------------------------

#endif // _TM8_UTIL_H_
