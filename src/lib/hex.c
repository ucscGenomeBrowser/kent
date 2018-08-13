/* Handy hexidecimal functions
 *   If you don't want to use printf
 */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

char hexTab[16] = {'0', '1', '2', '3', '4', '5', '6', '7', 
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f', };
/* Convert 0-15 to a hex char */


char nibbleToHex(unsigned char n)
/* convert nibble to hexidecimal character. 0 <= n <= 15. */
{
return hexTab[n];
}

void byteToHex(unsigned char n, char *hex)
/* convert byte to hexidecimal characters. 0 <= n <= 255. */
{
*hex++ = hexTab[n >> 4];
*hex++ = hexTab[n & 0xf];
}

char *byteToHexString(unsigned char n)
/* convert byte to hexidecimal string. 0 <= n <= 255. */
{
char hex[3];
byteToHex(n, hex);
hex[2] = 0;
return cloneString(hex);
}

/* And the reverse functions: */

char hexToNibble(char n)
/* convert hexidecimal character to nibble. 0-9a-f. */
{
return n - ( n <= '9' ? '0' : ('a'-10) );
}


unsigned char hexToByte(char *hex)
/* convert hexidecimal characters to unsigned char. */
{
unsigned char n = hexToNibble(*hex++);
n <<= 4;
n += hexToNibble(*hex++);
return n;
}


void hexBinaryString(unsigned char *in, int inSize, char *out, int outSize)
/* Convert possibly long binary string to hex string.
 * Out size needs to be at least 2x inSize+1 */
{
assert(inSize * 2 +1 <= outSize);
while (--inSize >= 0)
    {
    unsigned char c = *in++;
    *out++ = hexTab[c>>4];
    *out++ = hexTab[c&0xf];
    }
*out = 0;
}

