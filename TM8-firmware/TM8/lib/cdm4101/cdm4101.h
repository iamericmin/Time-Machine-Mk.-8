#ifndef _CDM4101_H_
#define _CDM4101_H_

#include <inttypes.h>

//----------------------------------------------------------------------------

#define CDM4101_NUM_DIGITS  4
#define CDM4101_NUM_SEGS 40

//----------------------------------------------------------------------------
// LCD commands.

#define CDM4101_BLINK_OFF 6
#define CDM4101_BLINK_ON  7
#define CDM4101_CLEAR     8

//----------------------------------------------------------------------------

class CDM4101
{
public:
	void init_lcd(void);
	void Command(uint8_t cmd, bool disp);
	void dispChar(uint8_t index, char c, bool disp);
  void dispCharRaw(uint8_t index, char c, bool disp);
	void dispStr(char *s, bool disp);
	void dispStrTimed(char *s, bool disp);
	void dispDec(short n, bool disp);

  uint8_t Digits[CDM4101_NUM_DIGITS];
  //uint8_t Segs[CDM4101_NUM_SEGS];

	void Update(bool);
	char ConvertChar(char c);

	uint8_t Blink;
	uint8_t Ctr;
};

//----------------------------------------------------------------------------

#endif // _CDM4101_H_
