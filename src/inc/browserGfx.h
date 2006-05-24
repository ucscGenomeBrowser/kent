/* Browser graphics - stuff for drawing graphics displays that
 * are reasonably browser (human and intronerator) specific. */

#ifndef BROWSERGFX_H
#define BROWSERGFX_H

#ifndef VGFX_H
#include "vGfx.h"
#endif

void vgBarbedHorizontalLine(struct vGfx *vg, int x, int y, 
	int width, int barbHeight, int barbSpacing, int barbDir, Color color,
	boolean needDrawMiddle);
/* Draw a horizontal line starting at xOff, yOff of given width.  Will
 * put barbs (successive arrowheads) to indicate direction of line.  
 * BarbDir of 1 points barbs to right, of -1 points them to left. */

void vgDrawRulerBumpText(struct vGfx *vg, int xOff, int yOff, 
	int height, int width,
        Color color, MgFont *font,
        int startNum, int range, int bumpX, int bumpY);
/* Draw a ruler inside the indicated part of mg with numbers that start at
 * startNum and span range.  Bump text positions slightly. */

void vgDrawRuler(struct vGfx *vg, int xOff, int yOff, int height, int width,
        Color color, MgFont *font,
        int startNum, int range);
/* Draw a ruler inside the indicated part of mg with numbers that start at
 * startNum and span range.  */

void vgNextItemButton(struct vGfx *vg, int x, int y, int w, int h, 
		      Color color, Color bgColor, boolean nextItem);
/* Draw a button that looks like a fast-forward or rewind button on */
/* a remote control. If nextItem is TRUE, it points right, otherwise */
/* left. color is the outline color, and bgColor is the fill color. */

#endif /* BROWSERGFX_H */
