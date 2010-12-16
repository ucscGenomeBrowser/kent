/* vGfx - interface to polymorphic graphic object
 * that currently can either be a memory buffer or
 * a postScript file. */

#ifndef VGFX_H
#define VGFX_H

#ifndef MEMGFX_H
#include "memgfx.h"
#endif

struct vGfx
/* Virtual graphic object - mostly a bunch of function
 * pointers. */
    {
    struct vGfx *next;	/* Next in list. */
    void *data;		/* Type specific data. */
    boolean pixelBased; /* Real pixels, not PostScript or something. */
    int width, height;  /* Virtual pixel dimensions. */

    void (*close)(void **pV);
    /* Finish writing out and free structure. */

    void (*dot)(void *v, int x, int y, int colorIx);
    /* Draw a single pixel.  Try to work at a higher level
     * when possible! */

    int (*getDot)(void *v, int x, int y);
    /* Fetch a single pixel.  Please do not use this, this is special
     * for verticalText only. */

    void (*box)(void *v, int x, int y, 
	    int width, int height, int colorIx);
    /* Draw a box. */

    void (*line)(void *v, 
	    int x1, int y1, int x2, int y2, int colorIx);
    /* Draw a line from one point to another. */

    void (*text)(void *v, int x, int y, int colorIx, void *font, char *text);
    /* Draw a line of text with upper left corner x,y. */

    void (*textRight)(void *v, int x, int y, int width, int height,
    	int colorIx, void *font, char *text);
   /* Draw a line of text right justified in box defined by x/y/width/height */

    void (*textCentered)(void *v, int x, int y, int width, int height,
    	int colorIx, void *font, char *text);
    /* Draw a line of text in middle of box. */

    int (*findColorIx)(void *v, int r, int g, int b);
    /* Find color in map if possible, otherwise create new color or
     * in a pinch a close color. */

    struct rgbColor (*colorIxToRgb)(void *v, int colorIx);
    /* Return rgb values for given color index. */

    void (*setWriteMode)(void *v, unsigned int writeMode);
    /* Set write mode. */

    void (*setClip)(void *v, int x, int y, int width, int height);
    /* Set clipping rectangle. */

    void (*unclip)(void *v);
    /* Set clipping rect cover full thing. */

    void (*verticalSmear)(void *v,
	    int xOff, int yOff, int width, int height, 
	    Color *dots, boolean zeroClear);
    /* Put a series of one 'pixel' width vertical lines. */

    void (*fillUnder)(void *v, int x1, int y1, int x2, int y2, 
	    int bottom, Color color);
    /* Draw a 4 sided filled figure that has line x1/y1 to x2/y2 at
     * it's top, a horizontal line at bottom at it's bottom,  and
     * vertical lines from the bottom to y1 on the left and bottom to
     * y2 on the right. */

    void (*drawPoly)(void *v, struct gfxPoly *poly, Color color, 
    	boolean filled);
    /* Draw polygon, possibly filled in color. */

    void (*setHint)(void *v, char *hint, char *value);
    /* Set hint */

    char * (*getHint)(void *v, char *hint);
    /* Get hint */

    int (*getFontPixelHeight)(void *v, void *font);
    /* How high in pixels is font? */

    int (*getFontStringWidth)(void *v, void *font, char *string);
    /* How wide is a string? */
    };

struct vGfx *vgOpenPng(int width, int height, char *fileName, boolean useTransparency);
/* Open up something that will write out a PNG file upon vgClose.  
 * If useTransparency, then the first color in memgfx's colormap/palette is
 * assumed to be the image background color, and pixels of that color
 * are made transparent. */

struct vGfx *vgOpenPostScript(int width, int height, char *fileName);
/* Open up something that will someday be a PostScript file. */

void vgClose(struct vGfx **pVg);
/* Close down virtual graphics object, and finish writing it to file. */

#define vgDot(v,x,y, color) v->dot(v->data,x,y,color)
/* Draw a single pixel.  Try to work at a higher level
 * when possible! */

#define vgGetDot(v,x,y) v->getDot(v->data,x,y)
/* Fetch a single pixel.  Please do not use this, this is special for
 * verticalText only */

#define vgBox(v,x,y,width,height,color) v->box(v->data,x,y,width,height,color)
/* Draw a box. */

#define vgLine(v,x1,y1,x2,y2,color) v->line(v->data,x1,y1,x2,y2,color)
/* Draw a line from one point to another. */

#define vgText(v,x,y,color,font,string) v->text(v->data,x,y,color,font,string)
/* Draw a line of text with upper left corner x,y. */

#define vgTextRight(v,x,y,width,height,color,font,string) \
	v->textRight(v->data,x,y,width,height,color,font,string)
/* Draw a line of text right justified in box defined by x/y/width/height */

#define vgTextCentered(v,x,y,width,height,color,font,string) \
	v->textCentered(v->data,x,y,width,height,color,font,string)
/* Draw a line of text in middle of box. */

#define vgFindColorIx(v,r,g,b) v->findColorIx(v->data, r, g, b)
/* Find index of RGB color.  */

#define vgColorIxToRgb(v,colorIx) v->colorIxToRgb(v->data, colorIx)
/* Return rgb values for given color index. */

#define vgSetWriteMode(v, writeMode)    \
	v->setWriteMode(v->data, writeMode)
/* Set write mode. */

#define vgSetClip(v,x,y,width,height) \
	v->setClip(v->data, x, y, width, height)
/* Set clipping rectangle. */

#define vgUnclip(v) v->unclip(v->data);
/* Get rid of clipping rectangle.  Note this is not completely
 * the same in PostScript and the memory images.  PostScript
 * demands that vgSetClip/vgUnclip come in pairs, and that
 * vgSetClip just further restrict a previous vgSetClip call.
 * The only portable way to do this is to strictly pair up
 * the setClip/unclip calls and not to nest them. */

#define vgVerticalSmear(v,x,y,w,h,dots,zeroClear) \
	v->verticalSmear(v->data,x,y,w,h,dots,zeroClear)
/* Take array of dots and smear them vertically. */

#define vgFillUnder(v,x1,y1,x2,y2,bottom,color) \
	v->fillUnder(v->data,x1,y1,x2,y2,bottom,color)
    /* Draw a 4 sided filled figure that has line x1/y1 to x2/y2 at
     * it's top, a horizontal line at bottom at it's bottom,  and
     * vertical lines from the bottom to y1 on the left and bottom to
     * y2 on the right. */

#define vgDrawPoly(v,poly,color,filled) \
	v->drawPoly(v->data,poly,color,filled)
    /* Draw a polygon in color, optionally filled. */

#define vgSetHint(v,hint,value) \
	v->setHint(v->data,hint,value)
    /* Set hint */

#define vgGetHint(v,hint) \
	v->getHint(v->data,hint)
    /* Get hint */

#define vgGetFontPixelHeight(v,font) \
	v->getFontPixelHeight(v->data,font)
    /* How high in pixels is font? */

#define vgGetFontStringWidth(v,font,string) \
	v->getFontStringWidth(v->data,font,string)
    /* How wide is a string? */

int vgFindRgb(struct vGfx *vg, struct rgbColor *rgb);
/* Find color index corresponding to rgb color. */

Color vgContrastingColor(struct vGfx *vg, int backgroundIx);
/* Return black or white whichever would be more visible over
 * background. */

#endif /* VGFX_H */
