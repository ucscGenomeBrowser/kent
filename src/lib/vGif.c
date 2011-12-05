/* vGif - a virtual graphic object wrapper around
 * an in-memory buffer destined to become a 
 * 256-color GIF file. */

#include "common.h"
#include "memgfx.h"
#include "vGfx.h"
#include "vGfxPrivate.h"


#ifndef COLOR32

struct memGif
/* Something that handles a gif. */
    {
    struct memGfx mg;	/* Memory form.  This needs to be first field. */
    char *fileName;	/* Gif file name. */
    boolean useTransparency;
    };

void memGifClose(struct memGif **pG)
/* Write out and close and free. */
{
struct memGif *g = *pG;
if (g != NULL)
    {
    struct memGfx *mg = (struct memGfx *)g;
    mgSaveGif(mg, g->fileName, g->useTransparency);
    freez(&g->fileName);
    mgFree(&mg);
    *pG = NULL;
    }
}

struct vGfx *vgOpenGif(int width, int height, char *fileName, boolean useTransparency)
/* Open up something that will someday be a PostScript file. */
{
struct memGif *gif;
struct memGfx *mg;
struct vGfx *vg;

/* Set up virtual graphics with memory methods. */
vg = vgHalfInit(width, height);
vgMgMethods(vg);
vg->close = (vg_close)memGifClose;

/* Get our mg + fileName structure.  We're forcing
 * inheritence from mg essentially. */
AllocVar(gif);
gif->fileName = cloneString(fileName);
gif->useTransparency = useTransparency;

/* Fill in the mg part of this structure with normal memGfx. */
mg = mgNew(width, height);
mgClearPixels(mg);
gif->mg = *mg;
freez(&mg);	/* We don't need this copy any more. */

vg->data = gif;
return vg;
}

#endif
