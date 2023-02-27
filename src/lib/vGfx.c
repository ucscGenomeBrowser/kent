/* vGfx - interface to polymorphic graphic object
 * that currently can either be a memory buffer or
 * a postScript file. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "vGfx.h"

/* Most of the implementation of this is in macros in vGfx.h. */

void vgClose(struct vGfx **pVg)
/* Close down virtual graphics object, and finish writing it to file. */
{
struct vGfx *vg = *pVg;
if (vg != NULL)
    {
    vg->close(&vg->data);
    freez(pVg);
    }
}

struct vGfx *vgHalfInit(int width, int height)
/* Close down virtual graphics object, and finish writing it to file. */
{
struct vGfx *vg;
AllocVar(vg);
vg->width = width;
vg->height = height;
return vg;
}

int vgFindRgb(struct vGfx *vg, struct rgbColor *rgb)
/* Find color index corresponding to rgba color. */
{
return vgFindAlphaColorIx(vg, rgb->r, rgb->g, rgb->b, rgb->a);
}

Color vgContrastingColor(struct vGfx *vg, int backgroundIx)
/* Return black or white whichever would be more visible over
 * background. Note: ignores alpha. */
{
struct rgbColor c = vgColorIxToRgb(vg, backgroundIx);
int val = (int)c.r + c.g + c.g + c.b;
if (val > 512)
    return MG_BLACK;
else
    return MG_WHITE;
}

