/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
struct gif_header
	{
	char giftype[6];
	unsigned char wlo,whi;
	unsigned char hlo,hhi;
	unsigned char colpix;	/* flags */
	unsigned char bgcolor;
	unsigned char reserved;
	};

#define COLTAB	0x80
#define COLMASK 0x70
#define COLSHIFT 4
#define PIXMASK 7
#define COLPIXVGA13 (COLTAB | (5<<COLSHIFT) | 7)

struct gif_image
	{
	unsigned char xlo,xhi;
	unsigned char ylo,yhi;
	unsigned char wlo,whi;
	unsigned char hlo,hhi;
	unsigned char flags;
	};
#define ITLV_BIT 0x40

/* Various error codes used by decoder
 * and my own routines...   It's okay
 * for you to define whatever you want,
 * as long as it's negative...  It will be
 * returned intact up the various subroutine
 * levels...
 */
#define OUT_OF_MEMORY -10
#define BAD_CODE_SIZE -20
#define READ_ERROR -1
#define WRITE_ERROR -2
#define OPEN_ERROR -3
#define CREATE_ERROR -4
#define TOO_HIGH	-5

short gif_compress_data(int min_code_size, unsigned char *pt, long size, FILE *out);
int gif_decoder(int linewidth);
