/* hCytoBand - stuff to help draw chromosomes where we have
 * banding data. */

/* Copyright (C) 2008 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef HCYTOBAND_H
#define HCYTOBAND_H

#ifndef HVGFX_H
#include "hvGfx.h"
#endif

#define hCytoBandDbIsDmel(db) (startsWith("dm", db))
/* We have to treat drosophila differently in some places. */

Color hCytoBandColor(struct cytoBand *band, struct hvGfx *hvg, boolean isDmel,
	Color aColor, Color bColor, Color *shades, int maxShade);
/* Return appropriate color for band. */

void hCytoBandDrawAt(struct cytoBand *band, struct hvGfx *hvg,
	int x, int y, int width, int height, boolean isDmel,
	MgFont *font, int fontPixelHeight, Color aColor, Color bColor,
	Color *shades, int maxShade);
/* Draw a single band in appropriate color at given position.  If there's
 * room put in band label. */

char *hCytoBandName(struct cytoBand *band, boolean isDmel);
/* Return name of band.  Returns a static buffer, so don't free result. */

Color hCytoBandCentromereColor(struct hvGfx *hvg);
/* Get the color used traditionally to draw centromere */

void hCytoBandDrawCentromere(struct hvGfx *hvg, int x, int y, 
	int width, int height, Color bgColor, Color fgColor);
/* Draw the centromere. */

#endif /* HCYTOBAND_H */
