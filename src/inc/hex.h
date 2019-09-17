
#ifndef HEX_H
#define HEX_H

char nibbleToHex(char n);
/* convert nibble to hexidecimal character. 0 <= n <= 15. */

void byteToHex(unsigned char n, char *hex);
/* convert byte to two hexidecimal characters. 0 <= n <= 255. */

char *byteToHexString(unsigned char n);
/* convert byte to hexidecimal string. 0 <= n <= 255. */

void hexBinaryString(unsigned char *in, int inSize, char *out, int outSize);
/* Convert possibly long binary string to hex string.
 * Out size needs to be at least 2x inSize+1 */

/* Reverse Functions */

char hexToNibble(char n);
/* convert hexidecimal character to nibble. 0-9a-f. */

unsigned char hexToByte(char *hex);
/* convert hexidecimal characters to unsigned char. */

#endif /* HEX_H */

