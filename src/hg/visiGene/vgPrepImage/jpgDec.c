/****************************************************
** 
** FILE:   	jpgDec.c  (libjpeg)
** CREATED:	2006-02-12
** AUTHOR: 	Galt Barber
**
** PURPOSE:	Decode jpg file one row at a time through callback. 
**              It is just set up to open and read one jpg file once.  Returns width and height.
**              Uses a 24-bit RGB view on the jpg.
**
*******************************************************/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "jpeglib.h"

#include "jpgDec.h"

#define VBUF 1   /* # of lines to buffer at a time (I tried 8 and it did not speed up) */

struct jpeg_decompress_struct cinfo;
struct jpeg_error_mgr jerr;

JDIMENSION num_scanlines = 0;

FILE *input_file;

UINT8 *pRGBTriplets[VBUF];

static void jpgErrAbort(char *errMsg)
/* print err msg to stderr and exit with non-zero exit code */
{
/* Abort on error */
fprintf(stderr,"%s",errMsg);
exit(1);
}

void jpgDecInit(char *jpgPath, int *retWidth, int *retHeight)
/* initialize jpg decoder */
{
int i;

/* Initialize the JPEG decompression object with default error handling. */
cinfo.err = jpeg_std_error(&jerr);
jpeg_create_decompress(&cinfo);

if ((input_file = fopen(jpgPath, "rb")) == NULL) 
    {
    fprintf(stderr, "Error: can't open %s\n", jpgPath);
    exit(EXIT_FAILURE);
    }
jpeg_stdio_src(&cinfo, input_file);

(void) jpeg_read_header(&cinfo, TRUE);

/* this does not seem to help the speed at all
cinfo.mem->max_memory_to_use = 120 * 1024L * 1024L;
*/

/* (might help speed a little, don't know if quality affected adversely) 
cinfo.dct_method = JDCT_IFAST; 
*/

/* Start decompressor */
(void) jpeg_start_decompress(&cinfo);

*retWidth = cinfo.output_width;
*retHeight = cinfo.output_height;

/* Allocate scanline RGB triplet buffer */

for(i=0;i<VBUF;++i)
    {
    pRGBTriplets[i] = (UINT8 *) malloc(cinfo.output_width*3); /* RGB triplet buffer */
    if(!pRGBTriplets[i]) 
	jpgErrAbort("Error allocating pRGBTriplets memory buffer.\n");
    }
   
}

unsigned char *jpgReadScanline()
/* read the next RGB scanline */
{
static unsigned int line=0;
if (line >= num_scanlines)
    {
    num_scanlines = jpeg_read_scanlines(&cinfo, pRGBTriplets, VBUF);
    if(num_scanlines < 1)
	{
	jpgErrAbort("Error decoding jpg.\n");
	}
    line = 0;	    
    }    
return (unsigned char *)pRGBTriplets[line++];    
}

void jpgDestroy()
/* close all resources */
{
int i;
for(i=0;i<VBUF;++i)
    free(pRGBTriplets[i]);
jpeg_destroy_decompress(&cinfo);
if (input_file != stdin)
    fclose(input_file);
if (jerr.num_warnings) 
    jpgErrAbort("Error decoding jpg.\n");
}

