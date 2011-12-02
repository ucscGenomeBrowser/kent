/* writegif.c - stuff to write out a GIF file.  See also comprs.c */

#ifndef USE_PNG
#include "common.h"
#include "memgfx.h"
#include "gifcodes.h"


static char gifsig[] = "GIF87a";

// GIF Graphic Control Extension, for making the background color transparent:
static struct gif_gce
    {
    unsigned char extensionIntroducer, graphicControlLabel, blockSize;
    unsigned char flags;
    unsigned char delayTimeLo,delayTimeHi;
    unsigned char transparentColorIndex;
    unsigned char blockTerminator;
    } gce = {0x21, 0xF9, 0x04, // fixed bytes from spec
	     0x01,             // transparency on (no user input or disposal options)
	     0x00, 0x00,       // no animation delay time
	     0x00,             // color index 0 (white) is transparent
	     0x00};

boolean mgSaveToGif(FILE *gif_file, struct memGfx *screen, boolean useTransparency)
/* Save GIF to an already open file.
 * If useTransparency, then the first color in memgfx's colormap/palette is
 * assumed to be the image background color, and pixels of that color
 * are made transparent. */
{
int i;
struct gif_header gif;
struct gif_image gim;
long gif_wcount;

gif_wcount = (long)screen->width * screen->height;
zeroBytes(&gif, sizeof(gif));
strncpy(gif.giftype, gifsig, sizeof(gif.giftype));
gif.wlo = gim.wlo = ((screen->width)&0xff);
gif.whi = gim.whi = ((screen->width>>8)&0xff);
gif.hlo = gim.hlo = ((screen->height)&0xff);
gif.hhi = gim.hhi = ((screen->height>>8)&0xff);
gim.xlo = gim.xhi = gim.ylo = gim.yhi = gim.flags = 0;
gif.colpix = COLPIXVGA13;
if (fwrite(&gif, sizeof(gif), 1, gif_file ) < 1)
    goto TRUNCOUT;
/* write global color map */
if (fwrite(screen->colorMap, 3, 256, gif_file) < 256)
    goto TRUNCOUT;

if (useTransparency)
    if (fwrite(&gce, sizeof(gce), 1, gif_file ) < 1)
	goto TRUNCOUT;

if (fputc(',', gif_file) < 0) /* comma to start image */
    goto TRUNCOUT;
if (fwrite(&gim, sizeof(gim), 1, gif_file) < 1)
    goto TRUNCOUT;
fputc(8,gif_file);
fflush(gif_file);
i = gif_compress_data(8, (unsigned char*)screen->pixels, gif_wcount, gif_file);
switch (i)
    {
    case 0:
        break;
    case -2:
	warn("Out of memory writing GIF");
	goto BADOUT;
    case -3:
	goto TRUNCOUT;
    default:
	warn("Error code %d writing gif", i);
	goto BADOUT;
    }
fputc(';', gif_file); /* end of file for gif */
return(TRUE);
TRUNCOUT:
warn("Disk full writing GIF");
BADOUT:
return(FALSE);
}

void mgSaveGif(struct memGfx *screen, char *name, boolean useTransparency)
/* Save memory bitmap as a gif.
 * If useTransparency, then the first color in memgfx's colormap/palette is
 * assumed to be the image background color, and pixels of that color
 * are made transparent. */
{
FILE *gifFile = mustOpen(name, "wb");
if (!mgSaveToGif(gifFile, screen, useTransparency))
    {
    remove(name);
    errAbort("Couldn't save %s", name);
    }
if (fclose(gifFile) != 0)
    errnoAbort("fclose failed");
}
#endif
