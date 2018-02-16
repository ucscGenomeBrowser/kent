/* cairoGfx - a version of memGfx using Cairo. Using the same struct, but with different functions
 *
 * license is hereby granted for all use - public, private or commercial. */

#ifdef USE_CAIRO

#include "common.h"
#include "localmem.h"
#include "vGfx.h"
#include "vGfxPrivate.h"
#include "cairoGfx.h"

#include "cairo/cairo.h" // 'apt-get install libcairo2-dev' / 'yum install cairo-devel'

#include "gemfont.h"

cairo_t *lastContext;

void mgcSetWriteMode(struct memGfx *mg, unsigned int writeMode)
/* Set write mode */
{
//mg->writeMode = writeMode;
printf("mgcSetWriteMode not implemented<br>");
}

void mgcSetClip(struct memGfx *mg, int x, int y, int width, int height)
/* Set clipping rectangle. */
{
//cairo_reset_clip (mg->cr);
cairo_rectangle(mg->cr, x, y, width, height);
cairo_clip(mg->cr);
}

void mgcUnclip(struct memGfx *mg)
/* Set clipping rect cover full thing. */
{
//mgcSetClip(mg, 0,0,mg->width, mg->height);
cairo_reset_clip (mg->cr);
}

struct memGfx *mgcNew(int width, int height, char *outFname)
/* Return new memGfx. */
{
struct memGfx *mgc;

mgc = needMem(sizeof(*mgc));
mgc->width = width;
mgc->height = height;
mgc->pixels = NULL;

cairo_surface_t *surface;
surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
cairo_t *cr;
cr = cairo_create(surface);
cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
mgc->cr = cr;
lastContext = cr;
return mgc;
}

void mgcClearPixels(struct memGfx *mg)
/* Set all pixels to background. */
{
printf("mgcClearPixels not implemented<br>");
}

void mgcClearPixelsTrans(struct memGfx *mg)
/* Set all pixels to transparent. */
{
printf("mgcClearPixelsTrans not implemented<br>");
}

Color mgcFindColor(struct memGfx *mg, unsigned char r, unsigned char g, unsigned char b)
/* Returns closest color in color map to rgb values.  If it doesn't
 * already exist in color map and there's room, it will create
 * exact color in map. */
{
return MAKECOLOR_32(r,g,b);
}


struct rgbColor mgcColorIxToRgb(struct memGfx *mg, int colorIx)
/* Return rgb value at color index. */
{
static struct rgbColor rgb;
rgb.r = (colorIx >> 0) & 0xff;
rgb.g = (colorIx >> 8) & 0xff;
rgb.b = (colorIx >> 16) & 0xff;
return rgb;
}

Color mgcClosestColor(struct memGfx *mg, unsigned char r, unsigned char g, unsigned char b)
/* Returns closest color in color map to r,g,b */
{
return MAKECOLOR_32(r,g,b);
}


Color mgcAddColor(struct memGfx *mg, unsigned char r, unsigned char g, unsigned char b)
/* Adds color to end of color map if there's room. */
{
return MAKECOLOR_32(r,g,b);
}

int mgcColorsFree(struct memGfx *mg)
/* Returns # of unused colors in color map. */
{
return 1 << 23;
}

void mgcFree(struct memGfx **pmg)
{
struct memGfx *mg = *pmg;
if (mg==NULL)
    return;
cairo_destroy(mg->cr);
freeMem(mg);
*pmg = NULL;
}

void mgcVerticalSmear(struct memGfx *mg,
	int xOff, int yOff, int width, int height, 
	Color *dots, boolean zeroClear)
/* Put a series of one 'pixel' width vertical lines. 
 * Uses 1-pixel-sized rectangles. While this is possibly not optimal,
 * it definitely is compatible. */
{
int x, i;
Color c;
for (i=0; i<width; ++i)
    {
    x = xOff + i;
    c = dots[i];
    if (c != MG_WHITE || !zeroClear)
        mgcDrawBox(mg, x, yOff, 1, height, c);
    }
}

static void setSourceRgb(cairo_t *cr, Color color)
/* set the rgb color */
{
    struct rgbColor col = mgcColorIxToRgb(NULL, color);
    cairo_set_source_rgb(cr, col.r/255.0, col.g/255.0, col.b/255.0);
}

void mgcDrawBox(struct memGfx *mgc, int x, int y, int width, int height, Color color)
{
cairo_t *cr = mgc->cr;
cairo_set_line_width(cr, 1);
setSourceRgb(cr, color);
cairo_rectangle(cr, x, y, width, height);
//cairo_stroke_preserve(cr);
cairo_fill(cr);
}

void mgcDrawLine(struct memGfx *mg, int x1, int y1, int x2, int y2, Color color)
/* Draw a line from one point to another. */
{
cairo_t *cr = mg->cr;
setSourceRgb(cr, color);
cairo_set_line_width(cr, 1);
cairo_move_to(cr, x1, y1);
cairo_line_to(cr, x2, y2);
cairo_stroke(cr);
}

void mgcDrawPoly(struct memGfx *mgc, struct gfxPoly *poly, Color color, boolean filled)
{
cairo_t *cr = mgc->cr;
setSourceRgb(cr, color);

struct gfxPoint *p = poly->ptList;
struct gfxPoint *end = p;

if (p != NULL)
    {
    cairo_move_to(cr, p->x, p->y);
    p = p->next;
    }

while (p != end) 
    {
    cairo_line_to(cr, p->x, p->y);
    p = p->next;
    }

cairo_close_path(cr);

if (filled) 
    {
    cairo_stroke_preserve(cr);
    cairo_fill(cr);
    }
}

void mgcFillUnder(struct memGfx *mg, int x1, int y1, int x2, int y2, 
	int bottom, Color color)
/* Draw a 4 sided filled figure that has line x1/y1 to x2/y2 at
 * it's top, a horizontal line at bottom as it's bottom, and
 * vertical lines from the bottom to y1 on the left and bottom to
 * y2 on the right. */
{
printf("mgcFillUnder not implemented<br>");
//gBrezy(mg, x1, y1, x2, y2, color, bottom, TRUE);
}

void mgcPutSeg(struct memGfx *mg, int x, int y, int width, Color *dots)
/* Put a series of dots starting at x, y and going to right width pixels. */
{
printf("mgcPutSeg not implemented<br>");
}

void mgcPutSegZeroClear(struct memGfx *mg, int x, int y, int width, Color *dots)
/* Put a series of dots starting at x, y and going to right width pixels.
 * Don't put zero dots though. */
{
printf("mgcPutSegZeroClearnot implemented<br>");
}


void mgcDrawHorizontalLine(struct memGfx *mgc, int y1, Color color)
/*special case of mgDrawLine, for horizontal line across entire window 
  at y-value y1.*/
{
mgcDrawLine( mgc, mgc->clipMinX, y1, mgc->clipMaxX, y1, color);
}

void mgcLineH(struct memGfx *mg, int y, int x1, int x2, Color color)
/* Draw horizizontal line width pixels long starting at x/y in color */
{
printf("mgcLineH not implemented<br>");
}


boolean mgcClipForBlit(int *w, int *h, int *sx, int *sy,
	struct memGfx *dest, int *dx, int *dy)
{
printf("mgcClipForBlit not implemented<br>");
return TRUE;
}

void mgcTextBlit(int width, int height, int bitX, int bitY,
	unsigned char *bitData, int bitDataRowBytes, 
	struct memGfx *dest, int destX, int destY, 
	Color color, Color backgroundColor)
{
printf("mgcTextBlit not implemented<br>");
}

void mgcTextBlitSolid(int width, int height, int bitX, int bitY,
	unsigned char *bitData, int bitDataRowBytes, 
	struct memGfx *dest, int destX, int destY, 
	Color color, Color backgroundColor)
{
printf("mgcTextBlitSolid not implemented<br>");
}


void mgcText(struct memGfx *mgc, int x, int y, Color color, 
	MgFont *font, char *text)
/* Draw a line of text with upper left corner x,y. */
{
cairo_t *cr = mgc->cr;
// For cairo, the only data we use from the MgFont object is the size
int fontSize = font->size;
cairo_set_font_size(cr, fontSize);
setSourceRgb(mgc->cr, color);
// XX currenly only supporting the Sans font - should we extend this? Guess no one ever changes the font
cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
cairo_move_to(cr, x, y+fontSize);
cairo_show_text(cr, text);
}

void mgcTextCentered(struct memGfx *mgc, int x, int y, int width, int height, 
	Color color, MgFont *font, char *text)
/* Draw a line of text centered in box defined by x/y/width/height */
{
int fWidth, fHeight;
int xoff, yoff;
fWidth = mgcFontStringWidth(font, text);
fHeight = mgcFontPixelHeight(font);
xoff = x + (width - fWidth)/2;
yoff = y + (height - fHeight)/2;
if (font == mgSmallFont())
    {
    xoff += 1;
    yoff += 1;
    }
mgcText(mgc, xoff, yoff, color, font, text);
}

void mgcTextRight(struct memGfx *mg, int x, int y, int width, int height, 
	Color color, MgFont *font, char *text)
/* Draw a line of text right justified in box defined by x/y/width/height */
{
int fWidth, fHeight;
int xoff, yoff;
fWidth = mgcFontStringWidth(font, text);
fHeight = mgcFontPixelHeight(font);
xoff = x + width - fWidth - 1;
yoff = y + (height - fHeight)/2;
if (font == mgSmallFont())
    {
    xoff += 1;
    yoff += 1;
    }
mgcText(mg, xoff, yoff, color, font, text);
}

int mgcFontPixelHeight(MgFont *font)
/* How high in pixels is font? */
{
int size = font_cel_height(font);
return size;
}

int mgcGetFontPixelHeight(struct memGfx *mg, MgFont *font)
/* How high in pixels is font? */
{
return mgcFontPixelHeight(font);
}

int mgcFontLineHeight(MgFont *font)
/* How many pixels to next line ideally? */
{
printf("mgcFontLineHeight not implemented<br>");
return font_line_height(font);
}

int mgcFontWidth(MgFont *font, char *chars, int charCount)
/* How wide are a couple of letters? */
/* The implementation of this function requires the cairo canvas but for some reason, 
 * the original kent implementation of this has no pointer to the drawing engine. We therefore
 * use a static variable for now. This assumes that mgcFontWidth always refers
 * to the last canvas that was created with mgcNew, this is not optimal, but compatible with old code. */
{
cairo_text_extents_t cte;
cairo_text_extents(lastContext, chars, &cte);
return cte.width;
}

int mgcFontStringWidth(MgFont *font, char *string)
/* How wide is a string? */
{
return mgcFontWidth(font, string, strlen(string));
}

int mgcGetFontStringWidth(struct memGfx *mg, MgFont *font, char *string)
/* How wide is a string? */
{
return mgcFontStringWidth(font, string);
}

int mgcFontCharWidth(MgFont *font, char c)
/* How wide is a character? */
{
return mgcFontWidth(font, &c, 1);
}

MgFont *mgcFontForSizeAndStyle(char *textSize, char *fontType)
/* Get a font of given size and style. For cairo, the only attribute that we use in the font is the size. */
{
textSize = mgFontSizeBackwardsCompatible(textSize);
MgFont *dummyFont;
dummyFont = needMem(sizeof(dummyFont));
dummyFont->size =atoi(textSize);
return dummyFont;
}

MgFont *mgcFontForSize(char *textSize)
/* Get a font of given size and style.  Abort with error message if not found.
 * The textSize should be 6,8,10,12,14,18,24 or 34.  For backwards compatibility
 * textSizes of "tiny" "small", "medium", "large" and "huge" are also ok. */
{
return mgcFontForSizeAndStyle(textSize, "medium");
}


void mgcSlowDot(struct memGfx *mg, int x, int y, int colorIx)
/* Draw a dot when a macro won't do. */
{
printf("mgcSlowDot not implemented<br>");
}

struct memGfx *mgcRotate90(struct memGfx *in)
/* Create a copy of input that is rotated 90 degrees clockwise. */
{
printf("mgcRotate not implemented<br>");
return NULL;
}

int mgcSlowGetDot(struct memGfx *mg, int x, int y)
/* Fetch a dot when a macro won't do. */
{
printf("mgcSlowGetDot not implemented<br>");
return 0;
}

void mgcSetHint(char *hint)
/* dummy function */
{
return;
}

char *mgcGetHint(char *hint)
/* dummy function */
{
return "";
}

void mgcSavePng(struct memGfx *mg, char *filename, boolean useTransparency)
/* Save cairo struct to filename as a PNG. */
{
cairo_t *cr = mg->cr;
cairo_surface_t *surface = cairo_get_target(cr);
cairo_surface_write_to_png(surface, filename);
}

void vgMgcMethods(struct vGfx *vg)
/* Fill in virtual graphics methods for Cairo based drawing. */
{
vg->pixelBased = TRUE;
vg->close = (vg_close)mgcFree;
vg->dot = (vg_dot)mgcSlowDot;
vg->getDot = (vg_getDot)mgcSlowGetDot;
vg->box = (vg_box)mgcDrawBox;
vg->line = (vg_line)mgcDrawLine;
vg->text = (vg_text)mgcText;
vg->textRight = (vg_textRight)mgcTextRight;
vg->textCentered = (vg_textCentered)mgcTextCentered;
vg->findColorIx = (vg_findColorIx)mgcFindColor;
vg->colorIxToRgb = (vg_colorIxToRgb)mgcColorIxToRgb;
vg->setWriteMode = (vg_setWriteMode)mgcSetWriteMode;
vg->setClip = (vg_setClip)mgcSetClip;
vg->unclip = (vg_unclip)mgcUnclip;
vg->verticalSmear = (vg_verticalSmear)mgcVerticalSmear;
vg->fillUnder = (vg_fillUnder)mgcFillUnder;
vg->drawPoly = (vg_drawPoly)mgcDrawPoly;
vg->setHint = (vg_setHint)mgcSetHint;
vg->getHint = (vg_getHint)mgcGetHint;
vg->getFontPixelHeight = (vg_getFontPixelHeight)mgcGetFontPixelHeight;
vg->getFontStringWidth = (vg_getFontStringWidth)mgcGetFontStringWidth;
}

#endif
