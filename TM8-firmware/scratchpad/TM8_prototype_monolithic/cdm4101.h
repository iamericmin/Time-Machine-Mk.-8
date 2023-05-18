#ifndef _CDM4101_H_
#define _CDM4101_H_

#include <inttypes.h>

//----------------------------------------------------------------------------

#define CDM4101_NUM_DIGITS  4

//----------------------------------------------------------------------------
// LCD commands.

#define CDM4101_BLINK_OFF 6
#define CDM4101_BLINK_ON  7
#define CDM4101_CLEAR     8

//----------------------------------------------------------------------------

class CDM4101
{
public:
	void Init(void);
	void Command(byte cmd);
	void DispChar(byte index, char c);
	void DispStr(char *s);
	void DispStrTimed(char *s);
	void DispDec(short n);

private:
	void Update(void);
	char ConvertChar(char c);

	byte Digits[CDM4101_NUM_DIGITS];
	byte Blink;
	byte Ctr;
};

//----------------------------------------------------------------------------

#endif // _CDM4101_H_
