/**************************************************
 * Copyright Jim Kent 2000.  All Rights Reserved. * 
 * Use permitted only by explicit agreement with  *
 * Jim Kent (jim_kent@pacbell.net).               *
 **************************************************/


/* writegif.c - stuff to write out a GIF file.  See also comprs.c */

#include "common.h"
#include "memgfx.h"
#include "gifcodes.h"

static char gifsig[] = "GIF87a";


boolean mgSaveGif(screen, name)
struct memGfx *screen;
char *name;
{
int i;
struct gif_header gif;
struct gif_image gim;
long gif_wcount;
FILE *gif_file;

gif_wcount = (long)screen->width * screen->height;
zeroBytes(&gif, sizeof(gif));
if ((gif_file = fopen(name, "wb")) == NULL)
    {
    fprintf(stderr, "Can't create %s\n", name);
    return FALSE;
    }
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
	fprintf(stderr, "Out of memory writing %s\n", name);
	goto BADOUT;
    case -3:
	goto TRUNCOUT;
    default:
	break;
    }
fputc(';', gif_file); /* end of file for gif */
fclose(gif_file);
return(TRUE);
TRUNCOUT:
fprintf(stderr, "%s truncated! Oops!\n", name);
BADOUT:
fclose(gif_file);
remove(name);
return(FALSE);
}

