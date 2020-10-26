
#include "common.h"
#include "memgfx.h"

#ifdef USE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H

FT_Library    library;
FT_Face       face;
#endif /*USE_FREETYPE*/

int ftInitialize();

void ftText(struct memGfx *mg, int x, int y, Color color, 
	MgFont *font, char *text);

int ftWidth(MgFont *font, unsigned char *chars, int charCount);
