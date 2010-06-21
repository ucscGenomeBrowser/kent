/* vGfx private - stuff that the implementers of 
 * a vGfx need to know about, but not the clients. */


struct vGfx *vgHalfInit(int width, int height);
/* Return a partially initialized vGfx structure. 
 * Generally not called by clients.*/

void vgMgMethods(struct vGfx *vg);
/* Fill in virtual graphics methods for memory based drawing. */

/* A bunch of things to make the type-casting easier.
 * This is a price you pay for object oriented
 * polymorphism in C... */

typedef void (*vg_close)(void **pV);
typedef void (*vg_dot)(void *v, int x, int y, int colorIx);
typedef int (*vg_getDot)(void *v, int x, int y);
typedef void (*vg_box)(void *v, int x, int y, 
	int width, int height, int colorIx);
typedef void (*vg_line)(void *v, 
	int x1, int y1, int x2, int y2, int colorIx);
typedef void (*vg_text)(void *v, int x, int y, int colorIx, void *font,
	char *text);
typedef void (*vg_textRight)(void *v, int x, int y, int width, int height,
	int colorIx, void *font, char *text);
typedef void (*vg_textCentered)(void *v, int x, int y, int width, int height,
	int colorIx, void *font, char *text);
typedef int (*vg_findColorIx)(void *v, int r, int g, int b);
typedef struct rgbColor (*vg_colorIxToRgb)(void *v, int colorIx);
typedef void (*vg_setClip)(void *v, int x, int y, int width, int height);
typedef void (*vg_setWriteMode)(void *v, unsigned int writeMode);
typedef void (*vg_unclip)(void *v);
typedef void (*vg_verticalSmear)(void *v,
	    int xOff, int yOff, int width, int height, 
	    Color *dots, boolean zeroClear);
typedef void (*vg_fillUnder)(void *v, int x1, int y1, 
	int x2, int y2, int bottom, Color color);
typedef void (*vg_drawPoly)(void *v, struct gfxPoly *poly, Color color, boolean filled);
typedef void (*vg_setHint)(void *v, char *hint, char *value);
typedef char * (*vg_getHint)(void *v, char *hint);
typedef int (*vg_getFontPixelHeight)(void *v, void *font);
typedef int (*vg_getFontStringWidth)(void *v, void *font, char *string);

