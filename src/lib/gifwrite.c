/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/

/* writegif.c - stuff to write out a GIF file.  See also comprs.c */

#include "common.h"
#include "memgfx.h"
#include "gifcodes.h"

static char gifsig[] = "GIF87a";


boolean mgSaveToGif(FILE *gif_file, struct memGfx *screen)
/* Save GIF to an already open file. */
{
int i;
struct gif_header gif;
struct gif_image gim;
long gif_wcount;

gif_wcount = (long)screen->width * screen->height;
zeroBytes(&gif, sizeof(gif));
strcpy(gif.giftype, gifsig);
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
if (fputc(',', gif_file) < 0) /* comma to start image */
    goto TRUNCOUT;
if (fwrite(&gim, sizeof(gim), 1, gif_file) < 1)
    goto TRUNCOUT;
fputc(8,gif_file);
fflush(gif_file);
i = gif_compress_data(8, screen->pixels, gif_wcount, gif_file);
switch (i)
    {
    case -2:
	warn("Out of memory writing GIF");
	goto BADOUT;
    case -3:
	goto TRUNCOUT;
    default:
	break;
    }
fputc(';', gif_file); /* end of file for gif */
return(TRUE);
TRUNCOUT:
warn("Disk full writing GIF");
BADOUT:
return(FALSE);
}

void mgSaveGif(struct memGfx *screen, char *name)
{
FILE *gifFile = mustOpen(name, "wb");
if (!mgSaveToGif(gifFile, screen))
    {
    remove(name);
    errAbort("Couldn't save %s", name);
    }
fclose(gifFile);
}
