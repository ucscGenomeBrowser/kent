
#include "freeType.h"

#ifndef USE_FREETYPE
int ftInitialize()
{
errAbort("FreeType not enabled. Install FreeType and recompile, or add freeType=off to hg.conf");
return 0;
}

int ftWidth(MgFont *font, unsigned char *chars, int charCount)
{
errAbort("FreeType not enabled. Install FreeType and recompile, or add freeType=off to hg.conf");
return 0;
}

void ftText(struct memGfx *mg, int x, int y, Color color, 
	MgFont *font, char *text)
{
errAbort("FreeType not enabled. Install FreeType and recompile, or add freeType=off to hg.conf");
}

#else 
FT_Library    library;
FT_Face       face;

int ftInitialize(char *fontFile)
{
FT_Error error;
error = FT_Init_FreeType( &library );              /* initialize library */

if (error !=0)
    return error;

error = FT_New_Face( library, fontFile, 0, &face );
if ((error !=0) || (face == NULL))
    errAbort("Cannot open font file '%s'.  Does it exist?", fontFile);

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
        {
        // have to combine any alpha in the color with the partial intensity
        // specified in the bitmap, since mixDot assumes you're specifying alpha
        // separately
        double alpha = (src/255.0) * (COLOR_32_ALPHA(color)/255.0);
        mixDot(mg, i, j, alpha, color); 
        }
    }
  }
}

void getFontCorrection(unsigned int requestSize, unsigned int *actualSize, unsigned int *baseline)
{
switch(requestSize)
    {
    // the constants herein were determine empirically by comparing the freeType font drawing
    // with the GEM font drawing for each text size in the hgTracks configure menu
    case 6:  // text size number from menu: 6
        *actualSize = 6;
        *baseline = 5;
        break;
    case 7:  // text size number from menu: 8
        *actualSize = 8;
        *baseline = 5;
        break;
    case 13:  // text size number from menu: 10
        *actualSize = 11;
        *baseline = 11;
        break;
    case 15:  // text size number from menu: 12
        *actualSize = 13;
        *baseline = 12;
        break;
    case 17:  // text size number from menu: 14
        *actualSize = 15;
        *baseline = 14;
        break;
    case 22:  // text size number from menu: 18
        *actualSize = 19;
        *baseline = 18;
        break;
    case 29:  // text size number from menu: 24
        *actualSize = 26;
        *baseline = 24;
        break;
    case 38:  // text size number from menu: 34
        *actualSize = 34;
        *baseline = 31;
        break;
    default:  // some other use not from configure memory
        *actualSize = (double)requestSize * 0.9;
        *baseline = (double)requestSize / 1.2;
        break;
    }
}



void ftTextHelper(struct memGfx *mg, int x, int y, int baseline, Color color,
        MgFont *font, char *text)
{
int length = strlen(text);
int n;
FT_Error error;
unsigned long offset = 0;
y +=  baseline;

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

void ftTextInBox(struct memGfx *mg, int x, int y, int width, int height, Color color, 
	MgFont *font, char *text)
/* Draw a line of text with upper left corner x,y. */
{
FT_Matrix     matrix;
FT_Vector     pen;  

if (height > 0)
    {
    matrix.xx = (FT_Fixed)(1 * 0x10000L);
    matrix.xy = (FT_Fixed)0;
    matrix.yx = (FT_Fixed)0;
    matrix.yy = (FT_Fixed)(1 * 0x10000L) ;
    pen.x = 0;
    pen.y = 0;
    FT_Set_Transform( face, &matrix, &pen );
    FT_Set_Pixel_Sizes( face, 1.3*width, 1.3 * height);
    ftTextHelper(mg, x, y, height, color, font, text);
    }
else
    {
    matrix.xx = (FT_Fixed)(1 * 0x10000L);
    matrix.xy = (FT_Fixed)0;
    matrix.yx = (FT_Fixed)0;
    matrix.yy = (FT_Fixed)(-1 * 0x10000L) ;
    pen.x = 0;
    pen.y = -height * 64;
    FT_Set_Transform( face, &matrix, &pen );

    height = -height;
    FT_Set_Pixel_Sizes( face, 1.3*width, 1.3 * height);
    ftTextHelper(mg, x, y, height, color, font, text);

    matrix.yy = (FT_Fixed)(1 * 0x10000L) ;
    pen.x = 0;
    pen.y = 0;
    FT_Set_Transform( face, &matrix, &pen );
    }
}

void ftText(struct memGfx *mg, int x, int y, Color color, 
	MgFont *font, char *text)
/* Draw a line of text with upper left corner x,y. */
{
unsigned int fontHeight;
unsigned int baseline;
getFontCorrection(mgFontPixelHeight(font), &fontHeight, &baseline);
FT_Set_Pixel_Sizes( face, fontHeight, fontHeight);
ftTextHelper(mg, x, y, baseline,color, font, text);
}

int ftWidth(MgFont *font, unsigned char *chars, int charCount)
{
unsigned int fontHeight;
unsigned int baseline;
getFontCorrection(mgFontPixelHeight(font), &fontHeight, &baseline);
FT_Set_Pixel_Sizes( face, fontHeight, fontHeight);
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
