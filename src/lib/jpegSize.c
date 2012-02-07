/* jpegSize - read a jpeg header and figure out dimensions of image.
 * Adapted by Galt Barber from Matthias Wandel's jhead program */
#include "common.h"
#include "jpegSize.h"


/* sections containing width and height     */
#define M_SOF0  0xC0            /* Start Of Frame N                        */
#define M_SOF1  0xC1            /* N indicates which compression process   */
#define M_SOF2  0xC2            /* Only SOF0-SOF2 are now in common use    */
#define M_SOF3  0xC3
#define M_SOF5  0xC5            /* NB: codes C4 and CC are NOT SOF markers */
#define M_SOF6  0xC6
#define M_SOF7  0xC7
#define M_SOF9  0xC9
#define M_SOF10 0xCA
#define M_SOF11 0xCB
#define M_SOF13 0xCD
#define M_SOF14 0xCE
#define M_SOF15 0xCF

#define M_SOI   0xD8            /* Start Of Image (beginning of datastream)*/
#define M_EOI   0xD9            /* End Of Image (end of datastream)        */
#define M_SOS   0xDA            /* Start Of Scan (begins compressed data)  */
#define M_JFIF  0xE0            /* Jfif marker                             */
#define M_EXIF  0xE1            /* Exif marker                             */

#define MAX_SECTIONS 40

typedef unsigned char uchar;

void jpegSize(char *fileName, int *width, int *height)
/* Read image width and height.
 * Parse marker stream until SOS or EOI; */
{
FILE * infile = mustOpen(fileName, "r"); 
int sectionsRead = 0;
boolean done = FALSE;
boolean foundJFIF = FALSE;
/* Scan the JPEG headers. */
if (fgetc(infile) != 0xff || fgetc(infile) != M_SOI)
    errAbort("error reading jpg header: %s",fileName);
while(!done)
    {
    int itemlen;
    int marker = 0;
    int ll,lh, got;
    int a=0;
    uchar * data;

    if (sectionsRead >= MAX_SECTIONS)
	errAbort("Too many sections in jpg file: %s",fileName);

    for (a=0;a<7;a++)
	{
	marker = fgetc(infile);
	if (marker != 0xff) 
	    break;
	if (a >= 6)
	    errAbort("too many padding bytes: %s",fileName);
	}

    /* 0xff is legal padding, but if we get that many, something's wrong. */
    if (marker == 0xff)
	errAbort("too many padding bytes: %s",fileName);

    /* Read the length of the section. */
    lh = fgetc(infile);
    ll = fgetc(infile);

    itemlen = (lh << 8) | ll;

    if (itemlen < 2)
	errAbort("invalid jpeg marker: %s",fileName);

    data = (uchar *)needMem(itemlen);
    if (data == NULL)
	errAbort("Could not allocate %d bytes memory", itemlen);

    /* Store first two pre-read bytes. */
    data[0] = (uchar)lh;
    data[1] = (uchar)ll;

    got = fread(data+2, 1, itemlen-2, infile); /* Read the whole section. */
    if (got != itemlen-2)
	errAbort("Premature end of file?: %s",fileName);
    
    ++sectionsRead;

    switch(marker)
	{
	case M_SOS:   /* stop before hitting compressed data */
	    done = TRUE;
	    break;
	case M_EOI:   /* in case it's a tables-only JPEG stream */
	    errAbort("No image in jpeg!: %s",fileName);
	case M_JFIF:
	    /* Regular jpegs always have this tag, 
	       exif images have the exif marker instead or in addition 
	       - could add check to make sure this is present
	    */
	    foundJFIF = TRUE;
	    break;

	case M_SOF0:
	case M_SOF1:
	case M_SOF2:
	case M_SOF3:
	case M_SOF5:
	case M_SOF6:
	case M_SOF7:
	case M_SOF9:
	case M_SOF10:
	case M_SOF11:
	case M_SOF13:
	case M_SOF14:	    
	case M_SOF15:
	    *height = data[3]*256+data[4];
	    *width  = data[5]*256+data[6];
	    done = TRUE;
	    break;
	default:
	    /* Skip any other sections. */
	    break;
	}
	
    freez(&data);
    
    }
fclose(infile);
if (!foundJFIF)
    errAbort("JFIF marker not found jpeg: %s",fileName);
return;
}


