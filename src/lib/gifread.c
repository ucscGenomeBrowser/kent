/* gifread.c - The high level GIF reading routines.  See writegif for the
   write side.  Also gifdecode.c for lower level GIF reading code. */

#include "common.h"
#include "gifcodes.h"
#include "memgfx.h"


static struct gif_header gif;
static struct gif_image gim;
static int gif_line;
static char iphase;
static WORD iy;
static UBYTE gif_cmap[256*3];
static FILE *gif_file;
static struct memGfx *gif_mg;
static int gif_width, gif_height;

int gif_get_byte()
/* Get next byte from file for decoder.
 * return -1 at end of file. */
{
return(fgetc(gif_file));
}


int gif_out_line(UBYTE *pixels, int linelen)
/* Output a line of gif. */
{
int y;

y = gif_line;
if (gim.flags&ITLV_BIT)
    {
    y = iy;
    switch (iphase)
        {
        case 0:
        case 1:
            iy+=8;
            break;
        case 2:
            iy += 4;
            break;
        case 3:
            iy += 2;
            break;
        }
    if (iy >= gif_height)
        {
        switch (iphase)
            {
            case 0:
                iy = 4;
                break;
            case 1:
                iy = 2;
                break;
            case 2:
                iy = 1;
                break;
            }
        iphase++;
        }
    }
gif_line++;
memcpy(gif_mg->pixels + y*gif_mg->width, pixels, linelen);
return(0);
}



struct memGfx *mgLoadGif(char *name)
/* Create memory image based on gif file. 
 * Note this is based on a very old gif reader
 * that only handles the GIF87a version. 
 * This is the same that mgSaveGif creates at
 * least.  This version of gif was always
 * color mapped. */
{
int c;
char type[7];
int gif_colors = 0;

gif_line = 0;
iphase = 0;
iy = 0;
gif_mg = NULL;
gif_file = mustOpen(name, "rb");
if (fread(&gif, 1, sizeof(gif), gif_file) < sizeof(gif))
    {
    goto TRUNCOUT;
    }
memcpy(type, gif.giftype, 6);
type[6] = 0;
if (!startsWith("GIF", type))
    {
    errAbort("Not a good GIF file");
    goto BADOUT;
    }
if (!sameString("GIF87a", type))
    {
    errAbort("Gif is version %s, sadly load_gif only speaks version GIF87a", type);
    goto BADOUT;
    }
gif_colors = (1<<((gif.colpix&PIXMASK)+1));
if (gif.colpix&COLTAB)
    {
    int size = gif_colors*3;
    if (fread(gif_cmap, 1, size, gif_file) < size)
        goto TRUNCOUT;
    }
for (;;)    /* skip over extension blocks and other junk til get ',' */
    {
    if ((c = fgetc(gif_file)) == READ_ERROR)
        goto TRUNCOUT;
    if (c == ',')
        break;
    if (c == ';')    /* semi-colon is end of piccie */
        goto TRUNCOUT;
    if (c == '!')    /* extension block */
        {
        if ((c = fgetc(gif_file)) == READ_ERROR)    /* skip extension type */
            goto TRUNCOUT;
        for (;;)
            {
            if ((c = fgetc(gif_file)) == READ_ERROR)
                goto TRUNCOUT;
            if (c == 0)    /* zero 'count' means end of extension */
                break;
            while (--c >= 0)
                {
                if (fgetc(gif_file) == READ_ERROR)
                    goto TRUNCOUT;
                }
            }
        }
    }
if (fread(&gim, 1, sizeof(gim), gif_file) < sizeof(gim))
    goto TRUNCOUT;
gif_width = (gim.whi<<8) + gim.wlo;
gif_height = (gim.hhi<<8) + gim.hlo;

gif_mg = mgNew(gif_width, gif_height);

/* Gif files can have color maps in two places.  Let
 * the gim color map overwrite the one in the gif header
 * here. */
if (gim.flags&COLTAB)
    {
    int size;
    gif_colors = (1<<((gim.flags&PIXMASK)+1));
    size = gif_colors*3;
    if (fread(gif_cmap, 1, size, gif_file) < size)
        goto TRUNCOUT;
    }
if (gif_colors > 0)
    {
    if (gif_colors > 256)
       errAbort("Too many colors in %s", name);
    memcpy(gif_mg->colorMap, gif_cmap, 3*gif_colors);
    }

switch (gif_decoder(gif_width))
    {
    case READ_ERROR:
    case BAD_CODE_SIZE:
        goto TRUNCOUT;
    case OUT_OF_MEMORY:
	errAbort("out of memory");
        goto BADOUT;
    default:
        break;
    }
carefulClose(&gif_file);
return(gif_mg);

TRUNCOUT:
errAbort("%s is truncated", name);
BADOUT:
carefulClose(&gif_file);
mgFree(&gif_mg);
return(NULL);
}


