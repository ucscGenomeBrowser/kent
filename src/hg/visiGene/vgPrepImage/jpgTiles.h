/****************************************************
** 
** FILE:   	jpgTiles.h  (uses libjpeg)
** CREATED:	6th February 2006
** AUTHOR: 	Galt Barber
**
** PURPOSE:	Convert input RGB data rows to jpg tile-set and thumbnail and optionally 
**              a full-size jpg for downloading as well.
**		Call jpgTiles with image width and height and outdir and outpath 
**              Quality settings can also be optionally specified.
**                 [-quality N|-variable 50 60 70 80 85]
**              Currently just operates on one output image tileset and stops,
**              so run the program again for each input image.
**
** NOTES:
**      Keeps the same size image as input and uses RGB 3-band 24-bit color
**      with default quality 75%. Caller must create a subdir under outdir with the root name of
**      the infile, i.e. infile/  and it puts the tiles for levels 0-4 there, 
**      each higher level zooms out by 2 in both dimensions.
**      N is % quality, from 3 to 95 and is optional, otherwise uses libjpeg default (75%).
**
*******************************************************/


#include "jpeglib.h"

#define TILESIZE 512

#define THUMBWIDTH 200

typedef struct 
    {
    char *outFile;
    FILE * output_file;
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JDIMENSION num_scanlines;   // for now always 1
    } jpegTile;

typedef struct 
    {
    int level;       // 0 to 4
    int toggle;      // alternates 0, 1 or FALSE, TRUE
    int scanline;    // keep track
    int imgWidth;    // will vary with zoom-level
    int imgHeight;
    int numTilesX;   // tiles per row
    int numTilesY;   // tiles vertically
    int hangX;  // leftover pixels e.g. width mod tileSize
    int hangY;  //  note: hang is 1 to 512, not 0-511
    int row;         // count from 0 to numTilesY-1
    UINT16 *pAccum;  // accumulate pixels
    UINT8 *pRGBTriplets;  // copy of condensed values averaged
    jpegTile **tiles;  // tiles in current row of tiles
    } levelInfo;


void jpgTiles(int nWidth, int nHeight,
    char *outFullDir, 
    char *outFullRoot, 
    char *outThumbPath, 
    unsigned char *(*readScanline)(), 
    int *inQuality, 
    boolean makeFullSize
    );
/* encode jpg tiles and thumbnail for given image, 
 * optionally create fullsize image too (e.g. when input is from jp2 source)
 * optionally specify jpg quality for each of 5 levels in int array */

