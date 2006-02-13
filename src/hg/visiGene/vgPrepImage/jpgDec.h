/****************************************************
** 
** FILE:   	jpg2Dec.h 
** CREATED:	2006-02-12
** AUTHOR: 	Galt Barber
**
** PURPOSE:	Decode jpg file one row at a time through callback. 
**              It is just set up to open and read one jpg file once.  Returns width and height.
**              Uses a 24-bit RGB view on the jpg.
**
*******************************************************/

void jpgDecInit(char *jpgPath, int *retWidth, int *retHeight);
/* initialize jpg decoder */

unsigned char *jpgReadScanline();
/* read the next RGB scanline */

void jpgDestroy();
/* close all resources */

