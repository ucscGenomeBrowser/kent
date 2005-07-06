/* vGfx - interface to polymorphic graphic object
 * that currently can either be a memory buffer or
 * a postScript file. */

#include "common.h"
#include "vGfx.h"

static char const rcsid[] = "$Id: vGfx.c,v 1.5 2005/07/06 00:06:03 hiram Exp $";


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
