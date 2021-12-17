
#include "common.h"
#include "memgfx.h"

#ifdef USE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H

extern FT_Library    library;
extern FT_Face       face;
#endif /*USE_FREETYPE*/

int ftInitialize();

void ftText(struct memGfx *mg, int x, int y, Color color, 
	MgFont *font, char *text);

void ftTextInBox(struct memGfx *mg, int x, int y, int width, int height, Color color, 
	MgFont *font, char *text);

int ftWidth(MgFont *font, unsigned char *chars, int charCount);
