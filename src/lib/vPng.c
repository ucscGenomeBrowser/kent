/* vPng - a virtual graphic object wrapper around an in-memory buffer destined to become a 256-color PNG file. */

#ifdef USE_PNG

#include "common.h"
#include "memgfx.h"
#include "vGfx.h"
#include "vGfxPrivate.h"

static char const rcsid[] = "$Id: vPng.c,v 1.1 2009/08/05 23:34:31 angie Exp $";

struct memPng
/* Something that handles a PNG. */
    {
    struct memGfx mg;	/* Memory form.  This needs to be first field. */
    char *fileName;	/* PNG file name. */
    boolean useAlpha;   /* Do we use alpha channel for transparency of background color? */
    };

void memPngClose(struct memPng **pG)
/* Write out and close and free. */
{
struct memPng *g = *pG;
if (g != NULL)
    {
    struct memGfx *mg = (struct memGfx *)g;
    mgSavePng(mg, g->fileName, g->useAlpha);
    freez(&g->fileName);
    mgFree(&mg);
    *pG = NULL;
    }
}

struct vGfx *vgOpenPng(int width, int height, char *fileName, boolean useAlpha)
/* Open up something that will write out a PNG file upon vgClose.  
 * If useAlpha, then the first color in memgfx's colormap/palette is
 * assumed to be the image background color, and pixels of that color
 * are made transparent. */
{
struct memPng *png;
struct memGfx *mg;
struct vGfx *vg;

/* Set up virtual graphics with memory methods. */
vg = vgHalfInit(width, height);
vgMgMethods(vg);
vg->close = (vg_close)memPngClose;

/* Get our mg + fileName structure.  We're forcing
 * inheritence from mg essentially. */
AllocVar(png);
png->fileName = cloneString(fileName);
png->useAlpha = useAlpha;

/* Fill in the mg part of this structure with normal memGfx. */
mg = mgNew(width, height);
mgClearPixels(mg);
png->mg = *mg;
freez(&mg);	/* We don't need this copy any more. */

vg->data = png;
return vg;
}

#endif//def USE_PNG
