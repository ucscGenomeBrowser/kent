/* hCytoBand - stuff to help draw chromosomes where we have
 * banding data. */

#ifndef HCYTOBAND_H
#define HCYTOBAND_H

#ifndef MEMGFX_H
#include "memgfx.h"
#endif

#ifndef VGFX_H
#include "vGfx.h"
#endif

#define hCytoBandDbIsDmel(db) (startsWith("dm", db))
/* We have to treat drosophila differently in some places. */

#define hCytoBandIsDmel() (hCytoBandDbIsDmel(hGetDb()))
/* We have to treat drosophila differently in some places. */

Color hCytoBandColor(struct cytoBand *band, struct vGfx *vg, boolean isDmel,
	Color aColor, Color bColor, Color *shades, int maxShade);
/* Return appropriate color for band. */

void hCytoBandDrawAt(struct cytoBand *band, struct vGfx *vg,
	int x, int y, int width, int height, boolean isDmel,
	MgFont *font, int fontPixelHeight, Color aColor, Color bColor,
	Color *shades, int maxShade);
/* Draw a single band in appropriate color at given position.  If there's
 * room put in band label. */

char *hCytoBandName(struct cytoBand *band, boolean isDmel);
/* Return name of band.  Returns a static buffer, so don't free result. */

Color hCytoBandCentromereColor(struct vGfx *vg);
/* Get the color used traditionally to draw centromere */

void hCytoBandDrawCentromere(struct vGfx *vg, int x, int y, 
	int width, int height, Color bgColor, Color fgColor);
/* Draw the centromere. */

#endif /* HCYTOBAND_H */
