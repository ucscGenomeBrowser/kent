
#include "freeType.h"
#include "iconv.h"

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

error = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
if ((error !=0) || (face == NULL))
    errAbort("Cannot select Unicode character map");

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

static iconv_t ourIconv; // convert context UTF-8 > UNICOD

static size_t utf8ToUnicode(char *text, unsigned short *buffer, size_t nLength)
/* Convert UTF-8 string to Unicode string. */
{
if (ourIconv == NULL)
    {
    ourIconv = iconv_open("UNICODE", "UTF-8");
    if (ourIconv == NULL)
        errAbort("iconv problem errno %d\n",errno);
    }

size_t length = strlen(text);
char *newText = (char *)buffer;

int ret = iconv(ourIconv, &text, &length, &newText,  &nLength);
while((ret = iconv(ourIconv, &text, &length, &newText,  &nLength)) < 0)
    {
    if (errno == EILSEQ)  // this means we hit an illegal character.  Skip it and put a '?' there, then go back into iconv
        {
        text++;
        
        // stash the Unicode question mark in the translated string
        *newText++ = '?';
        *newText++ = 0;
        }
    else
        errAbort("iconv problem errno %d\n",errno);
    }

return (newText - (char *)buffer) / sizeof(unsigned short);
}

// Drawing many long labels was dominated by FreeType reinterpreting each glyph
// outline (FT_Load_Char) every time, both to measure widths (ftWidth) and to
// render (ftTextHelper).  A glyph's advance and rendered bitmap are constant for
// a given face and pixel size, so cache them per (pixel size, character) and
// load each glyph only once.
//
// The cache hangs off face->generic.data, FreeType's per-face client-data slot,
// so it is automatically private to each FT_Face.  The multi-threaded draw gives
// every thread its own face, so a thread only ever touches its own face's cache
// and no locking is needed.  Caching covers single-byte chars on the default
// (identity-transform) text path; everything else falls back to a direct
// FT_Load_Char.
#define ftCacheChars 256   // we only cache single-byte (ASCII/Latin-1) chars

struct ftGlyph
/* One rendered glyph: its GRAY coverage bitmap plus placement and advance. */
    {
    boolean rendered;           /* Has this glyph been rendered yet? */
    unsigned int width;         /* Bitmap width in pixels. */
    unsigned int rows;          /* Bitmap height in pixels. */
    int left;                   /* slot->bitmap_left. */
    int top;                    /* slot->bitmap_top. */
    long advance;               /* advance.x in 26.6 fixed point. */
    unsigned char *buffer;      /* width*rows GRAY pixels, NULL if empty glyph. */
    };

struct ftSizeTable
/* Cached advances and rendered glyphs for one face at one pixel size. */
    {
    struct ftSizeTable *next;             /* Next pixel size for this face. */
    unsigned int pixelSize;               /* FT pixel size this table is for. */
    long advance[ftCacheChars];           /* advance.x in 26.6 fixed point. */
    boolean advanceCached[ftCacheChars];  /* Has advance[ch] been loaded yet? */
    struct ftGlyph glyph[ftCacheChars];   /* Rendered glyphs (lazy). */
    };

static void ftCacheFree(void *data)
/* face->generic.finalizer: free the per-face cache when FreeType destroys the
 * face (FT_Done_Face). */
{
struct ftSizeTable *tbl, *next;
for (tbl = (struct ftSizeTable *)data; tbl != NULL; tbl = next)
    {
    next = tbl->next;
    int i;
    for (i = 0; i < ftCacheChars; i++)
        freeMem(tbl->glyph[i].buffer);
    freeMem(tbl);
    }
}

static struct ftSizeTable *ftSizeTableFor(unsigned int pixelSize)
/* Return the cache table for the current face at pixelSize, creating it on the
 * first use.  The table list lives on face->generic.data, private to this
 * (per-thread) face, so no locking is needed. */
{
struct ftSizeTable *tbl;
for (tbl = (struct ftSizeTable *)face->generic.data; tbl != NULL; tbl = tbl->next)
    if (tbl->pixelSize == pixelSize)
        return tbl;
AllocVar(tbl);
tbl->pixelSize = pixelSize;
tbl->next = (struct ftSizeTable *)face->generic.data;
face->generic.data = tbl;
face->generic.finalizer = ftCacheFree;
return tbl;
}

static long ftAdvanceForChar(unsigned int pixelSize, unsigned short ch)
/* Return advance.x (26.6 fixed) for ch at the current face and pixelSize,
 * loading the glyph only on the first miss.  The caller must already have
 * done FT_Set_Pixel_Sizes(face, pixelSize, pixelSize). */
{
struct ftSizeTable *tbl = ftSizeTableFor(pixelSize);
if (ch < ftCacheChars && tbl->advanceCached[ch])
    return tbl->advance[ch];

FT_Load_Char( face, ch, 0 );
long advance = face->glyph->advance.x;
if (ch < ftCacheChars)
    {
    tbl->advance[ch] = advance;
    tbl->advanceCached[ch] = TRUE;
    }
return advance;
}

static struct ftGlyph *ftRenderChar(unsigned int pixelSize, unsigned short ch)
/* Return a cached rendered glyph for ch at the current face and pixelSize,
 * rendering it only on the first miss.  Returns NULL for chars we don't cache
 * (>= ftCacheChars) or that fail to load, so the caller falls back to a direct
 * render.  Caller must have set the pixel size and an identity transform
 * (i.e. the ftText path). */
{
if (ch >= ftCacheChars)
    return NULL;

struct ftSizeTable *tbl = ftSizeTableFor(pixelSize);
struct ftGlyph *g = &tbl->glyph[ch];
if (g->rendered)
    return g;

if (FT_Load_Char( face, ch, FT_LOAD_RENDER ) != 0)
    return NULL;
FT_GlyphSlot slot = face->glyph;
g->width = slot->bitmap.width;
g->rows = slot->bitmap.rows;
g->left = slot->bitmap_left;
g->top = slot->bitmap_top;
g->advance = slot->advance.x;
// Copy the coverage bitmap exactly as draw_bitmap reads it (packed width*rows).
size_t size = (size_t)g->width * g->rows;
if (size > 0)
    {
    g->buffer = needMem(size);
    memcpy(g->buffer, slot->bitmap.buffer, size);
    }
g->rendered = TRUE;
return g;
}

void ftTextHelper(struct memGfx *mg, int x, int y, int baseline, Color color,
        MgFont *font, char *text, unsigned int pixelSize, boolean cacheable)
{
size_t length = strlen(text);
int n;
FT_Error error;
unsigned long offset = 0;
y +=  baseline;

unsigned short buff[length * 4];
length = utf8ToUnicode(text, buff, length * 4);
unsigned short *sBuf = (unsigned short *)buff;

// account for strange short at front of string that tells us byte order
if (*sBuf == 0xfeff)
    {
    length--;
    sBuf++;
    }

for(n = 0; n < length; n++)
    {
    int dx;
    dx = x + offset / 64;
    // Fast path: reuse the cached rendered glyph for default text.
    if (cacheable)
        {
        struct ftGlyph *g = ftRenderChar(pixelSize, sBuf[n]);
        if (g != NULL)
            {
            if (g->buffer != NULL)
                {
                FT_Bitmap bm;
                ZeroVar(&bm);
                bm.width = g->width;
                bm.rows = g->rows;
                bm.buffer = g->buffer;
                draw_bitmap( mg, &bm, color,
                    dx + g->left, y - g->top, mg->pixels, mg->width );
                }
            offset += g->advance;
            continue;
            }
        }
    error = FT_Load_Char( face, sBuf[n], FT_LOAD_RENDER );
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
    ftTextHelper(mg, x, y, height, color, font, text, 0, FALSE);
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
    ftTextHelper(mg, x, y, height, color, font, text, 0, FALSE);

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
ftTextHelper(mg, x, y, baseline,color, font, text, fontHeight, TRUE);
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
    offset += ftAdvanceForChar(fontHeight, chars[n]);
offset /= 64;
return offset;
}
#endif /*USE_FREETYPE */
