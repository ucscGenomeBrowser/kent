
#include "freeType.h"

#define FONTFACTOR 0.90

#ifndef USE_FREETYPE
int ftInitialize()
{
errAbort("FreeType not enabled");
return 0;
}

int ftWidth(MgFont *font, unsigned char *chars, int charCount)
{
errAbort("FreeType not enabled");
return 0;
}

void ftText(struct memGfx *mg, int x, int y, Color color, 
	MgFont *font, char *text)
{
errAbort("FreeType not enabled");
}

#else 

int ftInitialize(char *fontFile)
{
FT_Error error;
error = FT_Init_FreeType( &library );              /* initialize library */

if (error !=0)
    return error;

error = FT_New_Face( library, fontFile, 0, &face );
if (error !=0)
    return error;

return 0;
}

void
draw_bitmap( struct memGfx *mg, FT_Bitmap*  bitmap, Color color,
             FT_Int      x,
             FT_Int      y,
            Color *image, int width)
{
  FT_Int  i, j, p, q;
  FT_Int  x_max = x + bitmap->width;
  FT_Int  y_max = y + bitmap->rows;


  /* for simplicity, we assume that `bitmap->pixel_mode' */
  /* is `FT_PIXEL_MODE_GRAY' (i.e., not a bitmap font)   */

  for ( i = x, p = 0; i < x_max; i++, p++ )
  {
    for ( j = y, q = 0; j < y_max; j++, q++ )
    {
    unsigned long src =  bitmap->buffer[q * bitmap->width + p];
    if (src)
        mixDot(mg, i, j, (double)src / 255, color); 
    }
  }
}

static double freeTypeCorrection(unsigned int size)
{
switch (size)
    {
    case 6: return 8;
    case 7: return 9;
    }
return size;
}

void ftText(struct memGfx *mg, int x, int y, Color color, 
	MgFont *font, char *text)
/* Draw a line of text with upper left corner x,y. */
{
unsigned int fontHeight = FONTFACTOR * freeTypeCorrection(mgFontPixelHeight(font));
unsigned int fontWidth = FONTFACTOR * freeTypeCorrection(mgFontPixelHeight(font));
FT_Set_Pixel_Sizes( face, fontWidth, fontHeight);
int length = strlen(text);
int n;
FT_Error error;
unsigned long offset = 0;
y +=  (double) fontHeight / 1.2 ;

for(n = 0; n < length; n++)
    {
    int dx;
    dx = x + offset / 64;
    error = FT_Load_Char( face, text[n], FT_LOAD_RENDER );
    FT_GlyphSlot  slot;
    slot = face->glyph;
    if (!error)
        draw_bitmap( mg,  &slot->bitmap, color,
         dx + slot->bitmap_left, y - slot->bitmap_top, mg->pixels, mg->width ); 
         //offset + slot->bitmap_left, y - slot->bitmap_top, mg->pixels, mg->width ); 
    offset += slot->advance.x;
    }
}

int ftWidth(MgFont *font, unsigned char *chars, int charCount)
{
unsigned int fontHeight = FONTFACTOR * freeTypeCorrection(mgFontPixelHeight(font));
unsigned int fontWidth = FONTFACTOR * freeTypeCorrection(mgFontPixelHeight(font));
FT_Set_Pixel_Sizes( face, fontWidth, fontHeight);
int n;
unsigned long offset = 0;
for(n = 0; n < charCount; n++)
    {
    FT_Load_Char( face, chars[n], 0 );
    FT_GlyphSlot  slot;
    slot = face->glyph;
    offset += slot->advance.x;
    }
offset /= 64;
return offset;
}
#endif /*USE_FREETYPE */
