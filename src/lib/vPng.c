/* vPng - a virtual graphic object wrapper around an in-memory buffer destined to become a 256-color PNG file. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


#include "common.h"
#include "memgfx.h"
#include "vGfx.h"
#include "vGfxPrivate.h"

#ifdef USE_CAIRO
#include "cairoGfx.h"
#endif

struct memPng
/* Something that handles a PNG. */
    {
    struct memGfx mg;	/* Memory form.  This needs to be first field. */
    char *fileName;	/* PNG file name. */
    boolean useTransparency;   /* Make background color transparent if TRUE. */
    };

void memPngClose(struct memPng **pG)
/* Write out and close and free. */
{
struct memPng *g = *pG;
if (g != NULL)
    {
    struct memGfx *mg = (struct memGfx *)g;
#ifdef USE_CAIRO
    if (mg->cr != NULL)
    {
        mgcSavePng(mg, g->fileName, g->useTransparency);
        mgcFree(&mg);
    }
    else
#endif
    {
        mgSavePng(mg, g->fileName, g->useTransparency);
        mgFree(&mg);
    }

    freez(&g->fileName);
    *pG = NULL;
    }
}

struct vGfx *vgOpenPng(int width, int height, char *fileName, boolean useTransparency)
/* Open up something that will write out a PNG file upon vgClose.  
 * If useTransparency, then the first color in memgfx's colormap/palette is
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
png->useTransparency = useTransparency;

/* Fill in the mg part of this structure with normal memGfx. */
mg = mgNew(width, height);
if (png->useTransparency)
    mgClearPixelsTrans(mg);
else
    mgClearPixels(mg);
png->mg = *mg;
freez(&mg);	/* We don't need this copy any more. */

vg->data = png;
return vg;
}

#ifdef USE_CAIRO

struct vGfx *vgOpenPngCairo(int width, int height, char *fileName, boolean useTransparency)
/* Open up a Cairo canvas, make it look like a memGfx object */
{
struct memPng *png;
struct memGfx *mg;

AllocVar(png);
mg = mgcNew(width, height, fileName);
png->fileName = cloneString(fileName);
png->mg = *mg;
freez(&mg);	/* We don't need this copy any more. */

struct vGfx *vg;
vg = vgHalfInit(width, height);
vgMgcMethods(vg);
vg->close = (vg_close)memPngClose;
vg->data = png;
return vg;
}

#else

struct vGfx *vgOpenPngCairo(int width, int height, char *fileName, boolean useTransparency)
/* dummy function */
{
printf("Error: Cairo activated, but not compiled with Cairo");
return vgOpenPng(width, height, fileName, useTransparency);
}

#endif

