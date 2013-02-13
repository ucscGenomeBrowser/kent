/* Handy hexidecimal functions
 *   If you don't want to use printf
 */

#include "common.h"

char nibbleToHex(char n)
/* convert nibble to hexidecimal character. 0 <= n <= 15. */
{
return n + ( n <= 9 ? '0' : 'A' );
}

void byteToHex(unsigned char n, char *hex)
/* convert byte to hexidecimal characters. 0 <= n <= 255. */
{
*hex++ = nibbleToHex(n & 0xf);
*hex++ = nibbleToHex(n >> 4);
}

char *byteToHexString(unsigned char n)
/* convert byte to hexidecimal string. 0 <= n <= 255. */
{
char hex[3];
byteToHex(n, hex);
hex[2] = 0;
return cloneString(hex);
}

