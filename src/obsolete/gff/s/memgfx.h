/* Memgfx - stuff to do graphics in memory buffers.
 * Typically will just write these out as .gif files.
 * This stuff is byte-a-pixel for simplicity.
 * It can do 256 colors.
 */

#define MG_WHITE 0
#define MG_BLACK 1
#define MG_RED 2
#define MG_GREEN 3
#define MG_BLUE 4

typedef unsigned char Color;

struct rgbColor
    {
    unsigned char r, g, b;
    };

struct memGfx
    {
    Color *pixels;
    int width, height;
    struct rgbColor colorMap[256];
    int colorsUsed;
    };

/* struct memGfx *mgNew(int width, int height); */
extern struct memGfx *mgNew();

/* void mgFree(struct memGfx **pmg) */
extern void mgFree();

/* Set all pixels to background. */
extern void mgClearPixels();

/* Unclipped plot a dot */
#define _mgPutDot(mg, x, y, color) ((mg)->pixels[(mg)->width*(y) + (x)] = (color))

/* Clipped put dot */
#define mgPutDot(mg,x,y,color) \
    if ((x)>=0 && (x) < (mg->width) && (y)>=0  && (y) < (mg->height)) \
	_mgPutDot(mg,x,y,color)

/* Draw a (horizontal) box */
/* mgDrawBox(mg, x, y, width, height, color); */
extern void mgDrawBox();


/* Draw a line from one point to another. */
/* mgDrawLine(mg, x1, y1, x2, y2, color); */
extern void mgDrawLine();

/* mgSaveGif(memGfx, name); */
extern boolean mgSaveGif();
