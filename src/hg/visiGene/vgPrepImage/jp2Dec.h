/****************************************************
** 
** FILE:   	jp2Dec.h 
** CREATED:	2006-02-10
** AUTHOR: 	Galt Barber
**
** PURPOSE:	Decode jp2 file (e.g. from Allen Brain Atlas) one row at a time through callback. 
**              It is just set up to open and read one jp2 file once.  Returns width and height.
**              Uses a 24-bit RGB view on the jp2.
**
*******************************************************/

void jp2DecInit(char *jp2Path, int *retWidth, int *retHeight);
/* initialize jp2 decoder */

unsigned char *jp2ReadScanline();
/* read the next RGB scanline */

void jp2Destroy();
/* close all resources */

