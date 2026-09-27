/* pngWriteTest - check that a memGfx image written as a PNG decodes to the same pixels.
 *
 * The browser's PNG writer has been changed for speed twice: it now pins the PNG row filter
 * to UP instead of letting libpng try all five on every row (refs #38107), and it links
 * zlib-ng in place of the system zlib (refs #38125).  Both are meant to be lossless, so the
 * decoded image must be exactly the image that was drawn.  This test draws an image that
 * mixes noise, flat color, gradients and partial transparency, writes it with mgSaveToPng(),
 * reads it back with libpng, and compares every byte.  It also inflates the image data itself
 * and checks the filter byte at the start of every row, so a change that quietly brings back
 * the filter search shows up here rather than as a slower hgTracks. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "memgfx.h"
#include "dystring.h"
#include "obscure.h"
#include <png.h>
#include <zlib.h>

#define WIDTH 257
#define HEIGHT 300

static unsigned int seed = 12345;

static unsigned int nextRandom()
/* A fixed pseudo-random sequence, so the image is the same on every run and platform. */
{
seed = seed * 1103515245 + 12345;
return (seed >> 16) & 0x7fff;
}

static struct memGfx *drawImage()
/* Draw the test image.  Each band of rows is a different kind of content, so that a filter
 * search would have picked different filters for different rows. */
{
struct memGfx *mg = mgNew(WIDTH, HEIGHT);
int x, y;
for (y = 0; y < HEIGHT; y++)
    for (x = 0; x < WIDTH; x++)
        {
        /* Work out the components first: the MAKECOLOR macros do not parenthesize
         * their arguments. */
        int r, g, b, a = 255;
        if (y < 60)          /* noise, with alpha varying too */
            {
            r = nextRandom() & 0xff;
            g = nextRandom() & 0xff;
            b = nextRandom() & 0xff;
            a = nextRandom() & 0xff;
            }
        else if (y < 120)    /* flat color */
            {
            r = 30;
            g = 144;
            b = 255;
            }
        else if (y < 180)    /* horizontal gradient */
            {
            r = x & 0xff;
            g = (2 * x) & 0xff;
            b = 255 - r;
            }
        else if (y < 240)    /* vertical gradient, half transparent */
            {
            r = y & 0xff;
            g = 0;
            b = 255 - r;
            a = 128;
            }
        else                 /* sparse dots on white, like a dense track */
            r = g = b = (nextRandom() % 13 == 0) ? 0 : 255;
        Color c = MAKECOLOR_32_A(r, g, b, a);
        mg->pixels[y * mg->width + x] = c;
        }
return mg;
}

static int compareDecoded(char *fileName, struct memGfx *mg)
/* Read fileName with libpng and return how many rows differ from mg. */
{
FILE *f = mustOpen(fileName, "rb");
png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
png_infop info = png_create_info_struct(png);
if (setjmp(png_jmpbuf(png)))
    errAbort("libpng could not read %s", fileName);
png_init_io(png, f);
png_read_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);
if (png_get_image_width(png, info) != mg->width || png_get_image_height(png, info) != mg->height)
    errAbort("%s is %dx%d, expected %dx%d", fileName, png_get_image_width(png, info),
             png_get_image_height(png, info), mg->width, mg->height);
if (png_get_color_type(png, info) != PNG_COLOR_TYPE_RGBA || png_get_bit_depth(png, info) != 8)
    errAbort("%s is not 8-bit RGBA", fileName);
png_bytepp rows = png_get_rows(png, info);
int y, badRows = 0;
for (y = 0; y < mg->height; y++)
    if (memcmp(rows[y], &mg->pixels[y * mg->width], mg->width * 4) != 0)
        badRows++;
png_destroy_read_struct(&png, &info, NULL);
carefulClose(&f);
return badRows;
}

static unsigned readBigEndian32(unsigned char *p)
/* Read a 4-byte big-endian number, the byte order of PNG chunk lengths. */
{
return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3];
}

static void countRowFilters(char *fileName, int height, int rowBytes, int filterCounts[5])
/* Inflate the IDAT chunks of fileName and count the filter byte that starts each row. */
{
char *buf;
size_t size;
readInGulp(fileName, &buf, &size);
unsigned char *u = (unsigned char *)buf;
struct dyString *idat = dyStringNew(size);
size_t pos = 8;    /* skip the PNG signature */
while (pos + 12 <= size)
    {
    unsigned len = readBigEndian32(u + pos);
    if (pos + 12 + len > size)
        errAbort("%s: chunk runs past the end of the file", fileName);
    if (memcmp(u + pos + 4, "IDAT", 4) == 0)
        dyStringAppendN(idat, buf + pos + 8, len);
    pos += 12 + len;
    }
uLongf rawSize = (uLongf)height * (1 + rowBytes);
unsigned char *raw = needLargeMem(rawSize);
uLongf gotSize = rawSize;
if (uncompress(raw, &gotSize, (unsigned char *)idat->string, idat->stringSize) != Z_OK
    || gotSize != rawSize)
    errAbort("%s: image data did not inflate to %lu bytes", fileName, (unsigned long)rawSize);
int y;
for (y = 0; y < height; y++)
    {
    int filter = raw[(size_t)y * (1 + rowBytes)];
    if (filter < 0 || filter > 4)
        errAbort("%s: row %d has filter byte %d", fileName, y, filter);
    filterCounts[filter]++;
    }
freeMem(raw);
dyStringFree(&idat);
freeMem(buf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    errAbort("usage: pngWriteTest out.png");
char *fileName = argv[1];
struct memGfx *mg = drawImage();
mgSavePng(mg, fileName, FALSE);

int badRows = compareDecoded(fileName, mg);
printf("decoded rows that differ from the image drawn: %d of %d\n", badRows, mg->height);

/* Every row must carry the UP filter, the first row included. */
static char *filterNames[5] = {"NONE", "SUB", "UP", "AVERAGE", "PAETH"};
int filterCounts[5] = {0, 0, 0, 0, 0};
countRowFilters(fileName, mg->height, mg->width * 4, filterCounts);
int i;
for (i = 0; i < 5; i++)
    printf("rows with filter %s: %d\n", filterNames[i], filterCounts[i]);
mgFree(&mg);
return 0;
}
