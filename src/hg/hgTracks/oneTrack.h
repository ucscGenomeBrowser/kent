/* oneTrack: load and draw a single track.  This is written for use in 
 * GBrowse plugins but could find other uses. */

#ifndef ONETRACK_H
#define ONETRACK_H

#include "hvGfx.h"

void oneTrackInit();
/* Set up global variables using cart settings and initialize libs. */

struct hvGfx *oneTrackMakeTrackHvg(char *trackName, char *gifName);
/* Set up a single track, load it, draw it and return the graphics object: */

unsigned char *oneTrackHvgToUcscGlyph(struct hvGfx *hvg, int *retVecSize);
/* Translate the memGfx implementation of hvGfx into a byte vector which 
 * GBrowse's Bio::Graphics::Glyph::ucsc_glyph will render into the main
 * GBrowse image. */

void oneTrackCloseAndWriteImage(struct hvGfx **pHvg);
/* Write out the graphics object to the filename with which it was created.
 * This also destroys the graphics object! */

#endif /* ONETRACK_H */
