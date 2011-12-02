/* timeGifPng - program to do some GIF and PNG timing. */
#include "png.h"
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "memgfx.h"
#include "portable.h"
#include "verbose.h"



void usage()
/* Explain usage and exit. */
{
errAbort(
  "timeGifPng - program to do some GIF and PNG timing\n"
  "usage:\n"
  "   timeGifPng nTimes file.gif [file.gif ..]\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct rgbaColor
    {
    unsigned char r, g, b, a;
    };
  
struct rgbaGfx
/* Structure you can draw on in red/green/blue/alpha (transparency). */
    {
    struct rgbaColor *pixels;
    int width, height;
    int clipMinX, clipMaxX;
    int clipMinY, clipMaxY;
    }; 


static void pngAbort(png_structp png, png_const_charp errorMessage)
/* type png_error wrapper around errAbort */
{
errAbort("%s", (char *)errorMessage);
}

static void pngWarn(png_structp png, png_const_charp warningMessage)
/* type png_error wrapper around warn */
{
warn("%s", (char *)warningMessage);
}

boolean saveToPng(FILE *f, struct rgbaGfx *rg, int level)
/* Save RGBA PNG to an already open file.
 * Reference: http://libpng.org/pub/png/libpng-1.2.5-manual.html */
{
png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
					  NULL, // don't need pointer to data for err/warn handlers
					  pngAbort, pngWarn);

if (!png)
    {
    errAbort("png_write_struct failed");
    return FALSE;
    }
png_infop info = png_create_info_struct(png);
if (!info)
    {
    png_destroy_write_struct(&png, NULL);
    errAbort("png create_info_struct failed");
    return FALSE;
    }

// If setjmp returns nonzero, it means png_error is returning control here.
// But that should not happen because png_error should call pngAbort which calls errAbort.
if (setjmp(png_jmpbuf(png)))
    {
    png_destroy_write_struct(&png, &info);
    fclose(f);
    errAbort("pngwrite: setjmp nonzero.  "
	     "why didn't png_error..pngAbort..errAbort stop execution before this errAbort?");
    return FALSE;
    }

// Configure PNG output params:
png_init_io(png, f);
png_set_IHDR(png, info, rg->width, rg->height, 8, // 8=bit_depth
	     PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
	     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
png_set_compression_level(png, level);

// Write header/params, write pixels, close and clean up.
// PNG wants a 2D array of pointers to byte offsets into palette/colorMap.
// rg has a 1D array of byte offsets.  Make row pointers for PNG:
png_byte **row_pointers = needMem(rg->height * sizeof(png_byte *));
int i;
for (i = 0;  i < rg->height;  i++)
    row_pointers[i] = (png_byte*)(&(rg->pixels[i*rg->width]));
png_set_rows(png, info, row_pointers);
png_write_png(png, info, PNG_TRANSFORM_IDENTITY, // no transform
	      NULL); // unused as of PNG 1.2
png_destroy_write_struct(&png, &info);
return TRUE;
}

void savePng(char *filename, struct rgbaGfx *rg, int level)
{
FILE *pngFile = mustOpen(filename, "wb");
if (!saveToPng(pngFile, rg, level))
    {
    remove(filename);
    errAbort("Couldn't save %s", filename);
    }
if (fclose(pngFile) != 0)
    errnoAbort("fclose failed");
}

void mapColor(unsigned char val, struct rgbColor *map, struct rgbaColor *color)
{
color->a = 0xff;
color->r = map[val].r;
color->g = map[val].g;
color->b = map[val].b;
}

struct rgbaGfx *convertMemToRgba(struct memGfx *mem)
{
struct rgbaGfx *rg;

AllocVar(rg);
rg->pixels = needLargeMem(sizeof(struct rgbaColor) * mem->width * mem->height);
rg->width = mem->width;
rg->height = mem->height;
rg->clipMinX = rg->clipMinY = 0;
rg->clipMaxX = rg->width;
rg->clipMaxY = rg->height;

int ii, jj;
struct rgbaColor *rgapixels =  rg->pixels;
unsigned char *pixels = mem->pixels;
for(ii=0; ii < rg->height; ii++)
    {
    for(jj=0; jj < rg->width; jj++)
        {
        struct rgbaColor color;
        mapColor(*pixels, mem->colorMap, &color);
        /*
        color.a = 0xff;
        color.r = 0xff;
        color.g = 0x3f;
        color.b = 0x1f;
        */
        *rgapixels++ = color;
        pixels++;
        }
    }


return rg;
}

void timeGifPng( int count, int fileNo, char *gifFile)
/* timeGifPng - program to do some GIF and PNG timing. */
{
/* first read in GIF to memory */
verboseTimeInit();
struct memGfx *mem = mgLoadGif(gifFile);
static long lastTime = -1;  // previous call time.
long pngTime, gifTime;
//verboseTime(1, "load gif");

struct rgbaGfx *rg = convertMemToRgba(mem);

lastTime = clock1000();
mgSaveGif(mem, "test.gif", FALSE);
gifTime = clock1000() - lastTime;

int fd = open("test.gif", O_RDONLY);
long gifSize = lseek(fd, 0L, 2);
close(fd);

printf("%d %d gif %ld %ld\n",count, fileNo,  gifTime, gifSize);
int ii;
for(ii=0; ii < 10; ii++)
    {
    lastTime = clock1000();
    savePng("test.png", rg, ii);
    pngTime = clock1000() - lastTime;

    fd = open("test.png", O_RDONLY);
    long pngSize = lseek(fd, 0L, 2);
    close(fd);
    printf("%d %d png%d %ld %ld\n",count, fileNo, ii, pngTime, pngSize);
    }

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
int ii,jj;
int numTimes = atoi(argv[1]);
for(jj=0; jj < numTimes; jj++)
    for(ii=2; ii < argc; ii++)
        timeGifPng(jj, ii -2 , argv[ii]);
return 0;
}
