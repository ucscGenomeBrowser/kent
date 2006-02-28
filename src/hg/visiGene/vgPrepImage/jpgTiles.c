/****************************************************
** 
** FILE:   	jpgTiles.c  (uses libjpeg)
** CREATED:	6th February 2006
** AUTHOR: 	Galt Barber
**
** PURPOSE:	Convert input RGB data rows to jpg tile-set and thumbnail and optionally 
**              a full-size jpg for downloading as well.
**		Call jpgTiles with image width and height and outdir and outpath 
**              Quality settings can also be optionally specified.
**                 [-quality N|-variable 50 60 70 80 85 85 85]
**              Currently just operates on one output image tileset and stops,
**              so run the program again for each input image.
**
** NOTES:
**      Keeps the same size image as input and uses RGB 3-band 24-bit color
**      with default quality 75%. Caller must create a subdir under outdir with the root name of
**      the infile, i.e. infile/  and it puts the tiles for levels 0-6 there, 
**      each higher level zooms out by 2 in both dimensions.
**      N is % quality, from 3 to 95 and is optional, otherwise uses libjpeg default (75%).
**
*******************************************************/

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include <string.h>   // memset - TODO maybe use a kent function instead?

#include "jpeglib.h"

#include "jpgTiles.h"

char *outDir = NULL;    /* output dir */
char *outRoot = NULL;  /* input filename without ext */


/* thumbnail vars */
jpegTile *thumb = NULL;
int thWidth;   
int thHeight;
float accW, dw;
float accH, dh;

int qualityLevels[7];

levelInfo *levels[7];  /* 0 to 6 */

jpegTile *newTile(char *fileName, int width, int height, int level)
/* new tile constructor */
{
jpegTile *self = malloc(sizeof(*self));
self->outFile = fileName;
self->output_file = fopen(self->outFile, "wb");  
if (!self->output_file) 
    {
    /* Abort on error */
    fprintf(stderr, "ERROR -> failed to open %s for writing\n", self->outFile);
    exit(1);
    }
self->num_scanlines = 1;
self->cinfo.err = jpeg_std_error(&self->jerr);
jpeg_create_compress(&self->cinfo);
self->cinfo.image_width = width;
self->cinfo.image_height = height;
self->cinfo.input_components = 3;  
self->cinfo.in_color_space = JCS_RGB;
jpeg_set_defaults(&self->cinfo);
jpeg_stdio_dest(&self->cinfo, self->output_file);
jpeg_set_quality(&self->cinfo, qualityLevels[level], TRUE);  
jpeg_start_compress(&self->cinfo, TRUE);
return self;
}

void destroyTile(jpegTile **pt)
/* tile destructor */
{
jpegTile *t = *pt;
if (t->jerr.num_warnings)
    {
    /* Abort on error */
    fprintf(stderr, "ERROR -> jerr.num_warnings > 0\n");
    exit(1);
    }
jpeg_finish_compress(&t->cinfo);
jpeg_destroy_compress(&t->cinfo);
fclose(t->output_file);
t->output_file = NULL;
free(t);
*pt = NULL;
}			

void writeTileRow(jpegTile *t, UINT8 *data)
/* write next row to compressor */
{
(void) jpeg_write_scanlines(&t->cinfo, &data, t->num_scanlines);
}


void writeScanlineToTiles(int level, UINT8 *data)
/* write next row to compressor */
{
int size=0, pix=0;
int i,j;
UINT16 *p;
UINT8  *q1, *q2, *r;
levelInfo *l = levels[level];
char fileName[512];
if ((l->scanline % TILESIZE)==0)
    {
    for(i=0; i < l->numTilesX; ++i)
	{
	int tWidth = TILESIZE;
	int tHeight = TILESIZE;
	if ((i+1) == l->numTilesX)
	    tWidth = l->hangX;
	if (((l->scanline / TILESIZE)+1)==l->numTilesY)
	    tHeight = l->hangY;
	sprintf(fileName,"%s/%s/%s_%d_%03d.jpg",outDir,outRoot,outRoot,l->level,(l->row*l->numTilesX)+i);
	l->tiles[i] = newTile(fileName, tWidth, tHeight, level);
	}
    }
for(i=0; i < l->numTilesX; ++i)
    {
    writeTileRow(l->tiles[i], data+(i*3*TILESIZE));
    }
if ( (((l->scanline+1) % TILESIZE)==0) || (l->scanline+1==l->imgHeight) )
    {
    for(i=0; i < l->numTilesX; ++i)
	{
	destroyTile(&l->tiles[i]);
	}
    ++l->row;	    
    }    
++l->scanline;

/* condense data */
if (level == 6)
    return;
pix = (l->imgWidth >> 1);
size = pix*3;
if (!l->toggle)
    {
    memset(l->pRGBTriplets,0,size*sizeof(UINT8));
    memset(l->pAccum,0,size*sizeof(UINT16));
    }
p = l->pAccum;    
q1 = data;
q2 = q1+3;
for(i=0;i<pix;++i)
    {
    for(j=0;j<3;++j)
	{
	*p++ += *q1++ + *q2++;
	}
    q1+=3;		
    q2+=3;		
    }

if (l->toggle)
    {
    r = l->pRGBTriplets;
    p = l->pAccum;    
    for(i=0;i<size;++i)
        {
	*r++ = (*p++)>>2;
	}
    writeScanlineToTiles(level+1, l->pRGBTriplets);
    } 
l->toggle = !l->toggle;    
}

levelInfo *newLevel(int level, int width0, int height0)
/* constructor for new level */
{
int size=0;
levelInfo *self = malloc(sizeof(*self));
self->level = level;
self->toggle = FALSE;
self->scanline = 0;
self->imgWidth = width0 >> level;   
self->imgHeight = height0 >> level;
self->numTilesX = 1 + (self->imgWidth / TILESIZE); 
self->numTilesY = 1 + (self->imgHeight / TILESIZE);
self->hangX = self->imgWidth % TILESIZE; 
if (self->hangX==0) 
    {
    self->hangX = TILESIZE;
    --self->numTilesX;
    }
self->hangY = self->imgHeight % TILESIZE;
if (self->hangY==0) 
    {
    self->hangY = TILESIZE;
    --self->numTilesY;
    }
self->row=0;         /* count from 0 to numTilesY-1 */
size = (self->imgWidth >> 1)*3;
self->pRGBTriplets = (UINT8  *) malloc(size*sizeof(UINT8) );  /* RGB triplet buffer */
self->pAccum       = (UINT16 *) malloc(size*sizeof(UINT16));  /* accumulate pixels */
self->tiles = malloc(self->numTilesX*sizeof(jpegTile)); 
return self;
}

void destroyLevel(levelInfo **pl)
/* level destructor */
{
levelInfo *l = *pl;
free(l->tiles);
free(l);
*pl = NULL;
}			


void writeScanlineToThumb(UINT8 *pRGBTriplets, int nHeight) 
/* This is in effect a stretch-blt */
{
int i,j,jj,jjj,r,R,G,B;
UINT8 *p;
int *tc, *tr;
static int k=0,kk,rr,kkk;  /* these need to be initialized just once and kept */
static boolean newH = TRUE;
static int thcols[THUMBWIDTH*3];
static int throws[THUMBWIDTH*3];
static UINT8 thdata[THUMBWIDTH*3];

/* Condense one input line of pixels to THUMBWIDTH (200) RGB pixels */
i=0;       /* index into thumb scanline */
accW = 0;  /* dw=nWidth/200; */
j=0;       /* index into pRGBTriplets orig scanline */
jj = 0;
for(i=0;i<THUMBWIDTH;++i)
    {
    accW += dw;	    
    jj = (int)accW;
    if (j>jj)
	j = jj;
    r = jj - j + 1;
    R = 0; G = 0; B = 0;
    p = pRGBTriplets+(j*3);
    for(jjj=j;jjj<=jj;++jjj)
	{
	R+=*p++;
	G+=*p++;
	B+=*p++;
	}
    thcols[i*3+0]=R/r;
    thcols[i*3+1]=G/r;
    thcols[i*3+2]=B/r;
    j = jj+1; 
    }

/* Condense one row into final output row */
if (newH)
    {
    newH = FALSE;
    accH += dh;	    
    kk = (int)accH;
    if (kk>=nHeight)   
	kk = nHeight-1;
    if (k>kk)
	k = kk;
    rr = kk - k + 1;
    kkk = k;
    memset(throws,0,THUMBWIDTH*3*sizeof(int)); /* clear row accum */
    }
tr = throws;
tc = thcols;
for(i=0;i<THUMBWIDTH*3;++i)
    {
    (*tr++) += *tc++;
    }

++kkk;
if (kkk <= kk)
    return;   /* condensed output row not yet finished */

/* row is ready to write */
/* normalize and copy */
p = thdata;
tr = throws;
for(i=0;i<THUMBWIDTH*3;++i)
    {
    *p++ = *tr++/rr;
    }

writeTileRow(thumb, thdata);
newH = TRUE;
k = kk+1; 

}

void jpgTiles(int nWidth, int nHeight,
    char *outFullDir, 
    char *outFullRoot, 
    char *outThumbPath, 
    unsigned char *(*readScanline)(), 
    int *inQuality, 
    boolean makeFullSize
    )
/* encode jpg tiles and thumbnail for given image, 
 * optionally create fullsize image too (e.g. when input is from jp2 source)
 * optionally specify jpg quality for each of 7 levels in int array */
{
int i;

char outFile[512];
struct jpeg_compress_struct cinfo;
struct jpeg_error_mgr jerr;
FILE * output_file = NULL;
JDIMENSION num_scanlines = 1;  /* for now, can do multiple lines later if we want */
INT32 nLine;
UINT8 *pRGBTriplets;
thWidth = THUMBWIDTH;
thHeight = (nHeight*THUMBWIDTH)/nWidth;

for(i=0;i<7;++i)
    {
    qualityLevels[i] = inQuality ? inQuality[i] : 75;
    }

outDir = outFullDir;
outRoot = outFullRoot;
	
if (makeFullSize)
    {
    sprintf(outFile,"%s/%s.jpg",outDir,outRoot);
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    cinfo.image_width = nWidth;
    cinfo.image_height = nHeight;
    cinfo.input_components = 3;  
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    output_file = fopen(outFile, "wb");  
    if (!output_file) {
	/* Abort on error */
	fprintf(stderr, "ERROR -> failed to open %s for writing\n", outFile);
	exit(1);
    }
    jpeg_stdio_dest(&cinfo, output_file);
    jpeg_set_quality(&cinfo, qualityLevels[0], TRUE);  
    jpeg_start_compress(&cinfo, TRUE);
    }

    
thumb = newTile(outThumbPath, thWidth, thHeight, 6);
accW = 0;
accH = 0;
dw=(float)nWidth/thWidth;
dh=(float)nHeight/thHeight;

/* ================================ */

/* start level 0-6 tiles */
levels[0] = newLevel(0, nWidth, nHeight);
levels[1] = newLevel(1, nWidth, nHeight);
levels[2] = newLevel(2, nWidth, nHeight);
levels[3] = newLevel(3, nWidth, nHeight);
levels[4] = newLevel(4, nWidth, nHeight);
levels[5] = newLevel(5, nWidth, nHeight);
levels[6] = newLevel(6, nWidth, nHeight);


/* Read all scanlines */
for(nLine = 0; nLine < nHeight; nLine++) 
    {
    pRGBTriplets = readScanline();
    if(pRGBTriplets) 
	{
	if (makeFullSize)
	    (void) jpeg_write_scanlines(&cinfo, &pRGBTriplets, num_scanlines);
	writeScanlineToTiles(0, pRGBTriplets);
	writeScanlineToThumb(pRGBTriplets,nHeight);
	} 
    else 
	{
	/* Abort on error */
	fprintf(stderr, "ERROR -> readScanLine() retuned NULL, err processing input\n");
	exit(1);
	}
    }

destroyLevel(&levels[4]);
destroyLevel(&levels[3]);
destroyLevel(&levels[2]);
destroyLevel(&levels[1]);
destroyLevel(&levels[0]);

destroyTile(&thumb);

if (makeFullSize)
    {
    if (jerr.num_warnings)
	{
	/* Abort on error */
	fprintf(stderr, "ERROR -> jerr.num_warnings > 0\n");
	exit(1);
	}

    /* Finish compression and release memory */
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    fclose(output_file);
    output_file = NULL;
    }

}
