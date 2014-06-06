/* bdfToGem - convert font bdf files to Gem C source font definitions */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include	"common.h"
#include	"memgfx.h"
#include	"sqlNum.h"
#include	"options.h"
#include	"linefile.h"
#include	"gemfont.h"


static char *name = (char *)NULL;	/* to name the font in the .c file */
static boolean noHeader = FALSE;  /* do not output the C header, data only */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"noHeader", OPTION_BOOLEAN},
    {"name", OPTION_STRING},
    {NULL, 0}
};

static void usage()
{
errAbort(
"bdfToGem - convert font bdf files to Gem C source font definitions\n"
"usage: bdfToGem [options] <file.bdf> <gem_definition.c>\n"
"options:\n"
"    -name=Small - the name of the font to place into the .c file\n"
"               - should be one of: Tiny Small Smallish Medium Large\n"
"    -noHeader  - do not output the C include lines at the beginning\n"
"               - to be used to concatinate source into one file\n"
"    -verbose=2 - to see processing statistics\n"
"    -verbose=5 - to see all missing glyphs"
);
}

/*	lower and upper limit of character values to accept
 *	Tried going down to zero since there was a glyph for it, but of
 *	course you can't print an ascii value of zero from a string
 *	since that is the end of string indication in C
 *	LO_LMT of 32 is assuming normal ASCII business which is the
 *	lowest printable ascii character 'space'
 *	There may be a case someday where encodings below 32 may be appropriate
 */
#define LO_LMT	32
#define HI_LMT	(255)
#define FILL_CHAR	((int)' ')
/*	given w pixels, round up to number of bytes needed	*/
#define BYTEWIDTH(w)	(((w) + 7)/8)
#define DEFAULT_FONT	"Small"

/*	a structure to store the incoming glyphs from the bdf file */
struct bdfGlyph
{
struct bdfGlyph *next;
int encoding;		/*	ascii value of character	*/
int w;			/*	width of actual bits, nothing extra	*/
int h;			/*	height of actual bits, nothing extra	*/
int xOff;		/*	from x=0 to left side of bits	*/
int yOff;		/*	from y=0 to bottom side of bits	*/
int dWidth;		/*	width including space around bits	*/
unsigned char **bitmap;
};

static unsigned char **allocRows(int w, int h)
/*	allocate a bitmap of pixel size width = w, height = h	*/
{
int byteWidth = 0;
unsigned char **bitmap = 0;
int row;

if ((0 == w) || (h == 0))
    errAbort("allocRows: w or h is zero: %d x %d", w, h);

byteWidth = BYTEWIDTH(w);
/*	first allocate the row pointers	*/
bitmap = (unsigned char **) needMem((size_t) (h * sizeof(unsigned char *)));
/*	then allocate a pointer for each row	*/
for (row = 0; row < h; ++row)
    {
    int col;
    bitmap[row] = (unsigned char *) needMem((size_t) byteWidth);
    for (col = 0; col < byteWidth; ++col)  /* and make sure it is all zero */
	bitmap[row][col] = (unsigned char) NULL;
    }
return (bitmap);
}

static struct bdfGlyph *allocGlyph(int w, int h)
/*	allocate a bdfGlyph structure, including its bitmap	*/
{
struct bdfGlyph *glyph;

AllocVar(glyph);
glyph->w = w;
glyph->h = h;
glyph->xOff = 0;
glyph->yOff = 0;
glyph->dWidth = 0;
glyph->bitmap = allocRows(w, h);
return (glyph);
}

static void freeGlyph(struct bdfGlyph **glyph)
/*	release all storage related to a bdfGlyph structure	*/
{
int row;

if ((struct bdfGlyph **)NULL == glyph) return;
if ((struct bdfGlyph *)NULL == *glyph) return;

for (row = 0; row < (*glyph)->h; ++row)
    freeMem((*glyph)->bitmap[row]);
freez(glyph);
}

static void freeGlyphList(struct bdfGlyph **glyph)
{
struct bdfGlyph *gl, *next;

if ((struct bdfGlyph **)NULL == glyph) return;
if ((struct bdfGlyph *)NULL == *glyph) return;

for (gl = *glyph; (struct bdfGlyph *)NULL !=  gl; gl = next)
    {
    next = gl->next;
    freeGlyph(&gl);
    }
*glyph = NULL;
return;
}

static int encodeCmp(const void *va, const void *vb)
/* Compare to sort based on encoding value */
{
const struct bdfGlyph *a = *((struct bdfGlyph **)va);
const struct bdfGlyph *b = *((struct bdfGlyph **)vb);
return (a->encoding - b->encoding);
}

static unsigned char leftMasks[8] =
    {
    0x0, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe
    };

static int previousLastOffset = -1;

/*  This bitsCopy is a specific limited form of a blit.  The limitation
 *	is allowed because the source bitmap always has the characteristic
 *	that it is upper left justified in its bitmap space.  Also, the
 *	destination bitmap is always being worked on from left to right,
 *	and thus only the bits to the left need to be preserved.  Any
 *	bits to the right in the destination bitmap can be left blank.
 *
 *	The calculation below to determine startRow takes the various Y
 *	coordinates into account to get the glyph properly copied in
 *	it's vertical position into the destination.
 */
static int bitsCopy(unsigned char **bitmap, int offset, int maxYextent,
    int minYoff, struct bdfGlyph *glyph)
/*	a cheap blit of a single glyph, returns pixel columns copied */
{
int startRow = maxYextent -
		(((glyph->h + glyph->yOff) - 1) - minYoff) - 1;
int destRow = startRow;
int srcRow = 0;
int bitsOnLeft = (offset + glyph->xOff) & 0x7;	/* range: [0-7]	*/
int destColumn = (offset + glyph->xOff) >> 3;

for (destRow = startRow; (srcRow < glyph->h) && (destRow < maxYextent);
	++destRow, ++srcRow)
{
    int col;

    destColumn = (offset + glyph->xOff) >> 3;

    if (0 == bitsOnLeft)
	{
	for (col = 0; col < BYTEWIDTH(glyph->w); ++col, ++destColumn)
	    bitmap[destRow][destColumn] = glyph->bitmap[srcRow][col];
	}
    else
	{
	int bitsOnRight = 8 - bitsOnLeft;
	unsigned char maskLeft = leftMasks[bitsOnLeft];
	unsigned char maskRight = ~maskLeft;
	for (col = 0; col < BYTEWIDTH(glyph->w); ++col, ++destColumn)
	    {
	    unsigned char dest = bitmap[destRow][destColumn];
	    unsigned char src = glyph->bitmap[srcRow][col];
	    bitmap[destRow][destColumn] =
		(dest & maskLeft) | ((src >> bitsOnLeft) & maskRight);
	    bitmap[destRow][destColumn+1] = ((src << bitsOnRight) & maskLeft);
	    }
	}
}
previousLastOffset = offset + glyph->xOff + glyph->w;

/*	The maximum of glyph->dWidth or (glyph->xOff + glyph->w)
 *	prevents overlaps between characters in the all char bitmap
 */
return(glyph->dWidth > (glyph->xOff + glyph->w) ? glyph->dWidth :
	(glyph->xOff + glyph->w));
}

static void outputGem(char *out, struct font_hdr *font,
    struct bdfGlyph *glyphs, char *inputFileName)
/*	all input is done, now do the output file	*/
{
unsigned char **bitmap = (unsigned char **)NULL; /* all glyphs together */
FILE *f = mustOpen(out,"w");
struct bdfGlyph *glyph = (struct bdfGlyph *)NULL;
struct bdfGlyph *fillChar = (struct bdfGlyph *)NULL;
int glyphCount = 0;	/* a count of glyphs to work on */
int maxGlyphCount = HI_LMT - LO_LMT + 1;
int encoding = 0;	/* a character's ascii value */
int lastEncoding = 0;	/* used to find missing glyphs */
int missing = 0;	/* a count of missing glyphs */
int offset = 0;		/* pixel(bit) offset into the all char bitmap */
int minXoff = BIGNUM;	/* lowest x offset found in incoming glyphs */
int maxXoff = -BIGNUM;	/* highest x offset found in incoming glyphs */
int minYoff = BIGNUM;	/* lowest y offset found in incoming glyphs */
int maxYoff = -BIGNUM;	/* highest y offset found in incoming glyphs */
int maxYextent = 0;	/* maxium height of all chars with offset incl */
int maxXextent = 0;	/* maxium width of all chars with offset incl */
int maxDwidth = 0;	/* maximum declared width of all chars */
int bytesOut = 0;	/* a loop counter for output line break calc */
int row;		/* row = 0 at top of a bitmap	*/
int combinedWidth = 0;	/* sum of all character widths */
int *offsets = (int *)NULL;	/* offset array to be filled in and printed */
int widthSpace = 0;	/* the space character is used for missing glyphs */
int maxYcoord = -BIGNUM;/* highest Y coordinate found in incoming glyphs*/
int minYcoord = BIGNUM; /* lowest Y coordinate found in incoming glyphs*/
int maxXcoord = -BIGNUM;/* highest X coordinate found in incoming glyphs*/
int minXcoord = BIGNUM; /* lowest Y coordinate found in incoming glyphs*/


slSort(&glyphs, encodeCmp);	/*	order glyphs by encoding value */

/*	Survey the individual glyph bounding boxes, find max,min extents
 *	Sanity check against given maximums for the whole bitmap
 *	as it will be put together.
 */
encoding = glyphs->encoding - 1;	/* to check for missing glyphs */
for (glyph = glyphs; glyph; glyph=glyph->next)
    {
    int xLeft;
    int xRight;
    int yTop;
    int yBottom;
    if (glyph->encoding != (encoding + 1))
	{
	verbose(5, "#\tmissing glyph for encodings: %d - %d\n", 
		encoding + 1, glyph->encoding - 1);
	missing += glyph->encoding - encoding - 1;
	}
    encoding = glyph->encoding;
    if (encoding == FILL_CHAR)
	fillChar = glyph;
    if (glyph->xOff < minXoff) minXoff = glyph->xOff;
    if (glyph->xOff > maxXoff) maxXoff = glyph->xOff;
    if (glyph->yOff < minYoff) minYoff = glyph->yOff;
    if (glyph->yOff > maxYoff) maxYoff = glyph->yOff;
    yTop = glyph->h + glyph->yOff;
    yBottom = glyph->yOff;
    xLeft = glyph->xOff;
    xRight = glyph->xOff + glyph->w;
    if (xLeft < minXcoord) minXcoord = xLeft;
    if (xRight > maxXcoord) maxXcoord = xRight;
    if (yTop > maxYcoord) maxYcoord = yTop;
    if (yBottom < minYcoord) minYcoord = yBottom;
    if (glyph->dWidth > maxDwidth) maxDwidth = glyph->dWidth;
    if ((maxYcoord - minYcoord) > maxYextent)
	maxYextent = maxYcoord - minYcoord;
    if ((maxXcoord - minXcoord) > maxXextent)
	maxXextent = maxXcoord - minXcoord;
    combinedWidth += BYTEWIDTH(glyph->dWidth) * 8;
    ++glyphCount;
    }
lastEncoding = encoding;

/*	for our purposes here, we must have the fillChar (space)
 *	which will also be used to fill missing glyphs
 */
if ((struct bdfGlyph *)NULL == fillChar)
    errAbort("can not find fill character: %d '%c', we need a space\n",
	FILL_CHAR, FILL_CHAR);

widthSpace = fillChar->dWidth;	/*	we use space (FILL_CHAR) */

verbose(2, "#\thave %d glyphs to merge, missing: %d (maxGlyphCount: %d)\n",
	glyphCount, missing, maxGlyphCount);
verbose(2, "#\tmin,max X offsets: %d, %d, maxXextent: %d\n",
	minXoff, maxXoff, maxXextent);
verbose(2, "#\tmin,max Y offsets: %d, %d, maxYextent: %d\n",
	minYoff, maxYoff, maxYextent);
verbose(2, "#\tadding width of %d*%d = %d to combinedWidth %d for %d missing glyphs\n",
	missing, widthSpace, BYTEWIDTH(missing * widthSpace)*8, combinedWidth, missing);

if ((glyphCount + missing) > maxGlyphCount)
    errAbort("found more glyphs than allowed: (%d + %d) = %d  vs. %d\n",
	glyphCount, missing, glyphCount + missing, maxGlyphCount);
if (font->frm_hgt != maxYextent)
    errAbort("do not find the same maximum height during scan ? %d != %d",
	font->frm_hgt, maxYextent);
if (minYoff > -1)
    errAbort("Expected the lowest y coordinate to be negative ? minYoff = %d",
	minYoff);

combinedWidth += BYTEWIDTH(missing * widthSpace)*8;
font->frm_wdt = BYTEWIDTH(combinedWidth);

/*	The plus one is for the last offset which would be the glyph at
 *	maxGlyphCount+1 - so that the size of the last glyph can be
 *	properly computed by the font code.
 */
offsets = (int *)needMem((sizeof(int) * (maxGlyphCount + 1)));
verbose(2,"#\tallocated int offsets[%d]\n", maxGlyphCount);

bitmap = allocRows(font->frm_wdt * 8, font->frm_hgt);
verbose(2,"#\tallocated bitmap: %d x %d pixels = %d x %d bytes\n",
	font->frm_wdt * 8, font->frm_hgt, font->frm_wdt, font->frm_hgt);

/*	Now that we know our and minYoff, this becomes our
 *	reference to coordinate Y=0 in the global bitmap.
 *	To copy an individual glyph into its place in the global bitmap,
 *	the combination of the individual glyph's yOff with this minYoff
 *	will move it to the correct row in the global bitmap.
 *
 *	The loop goes through all possible encodings from lo to hi
 *	Checking the codings in the glyphs as moving along, when missing
 *	glyphs are found, substitute blank.
 *
 *	Coordinate systems involved here:
 *	In the destination bitmap:
 *	Y coord range is from row 0 (top) to row (maxYextent-1) (bottom)
 *	and the baseline y=0 coordinate is row: (maxYextent-1)+minYoff
 *	Where minYoff was confirmed above to be negative.
 *	This business would not work properly if it wasn't negative.
 *	This is most likely the descender distance in the gemfonts.
 *
 *	In the source bitmap:
 *	Y coord range is from row 0 (top) to row (glyph->h - 1) (bottom)
 *	All these glyph bitmaps are upper left justified.
 *	Their pixel width is glyph->dWidth which includes any necessary
 *	spacing for the next character.  It is the distance from this
 *	glyph's (0,0) origin to the next glyph's (0,0) origin.
 *	This will be the glyph's entire width in the destination bitmap.
 *	The bottom of these glyphs at row (glyph->h-1) is referenced to
 *	the origin y=0 coordinate by glyph->yOff which does not
 *	have to be negative.  An apostrophy, for example, would have a
 *	positive yOff specifying the distance from y=0 to the bottom of
 *	the glyph's bitmap, which would proceed from there upward by
 *	glyph->h pixels.
 *
 */

offset = 0;		/*  offset=bit (pixel) position in global bitmap */
glyphCount = 0;
glyph = glyphs;		/*	the first one is (LO_LMT)	*/
for (encoding = glyphs->encoding; encoding <= lastEncoding ; ++encoding)
    {
    struct bdfGlyph *glyphToUse = glyph;

    if(NULL == glyphToUse)
	errAbort("got lost in glyphs at number: %d, encoding: %d",
		glyphCount, encoding);

    offsets[glyphCount] = offset;

    if (encoding != glyph->encoding)	/*	missing glyph ?	*/
	{
	glyphToUse = fillChar;	/*	use FILL_CHAR (space)	*/
	verbose(5,"#\tmissing glyph at encoding %d '%c'\n", encoding,
	    isprint((char)encoding) ? (char)encoding : ' ');
	}
    else
	glyph = glyph->next;	/*	not missing, OK to go to next */

    offset += bitsCopy(bitmap, offset, maxYextent, minYoff, glyphToUse);
    ++glyphCount;
    }

offsets[glyphCount] = offset;

if ((char *)NULL == name)
	name = cloneString(DEFAULT_FONT);	/*	default name of font */

/*	And now to start the output of the C source code	*/

fprintf(f, "\n/* %s.c - compiled data for font %s */\n", name,font->facename);

fprintf(f, "/* generated source code by utils/bdfToGem, do not edit */\n");
fprintf(f, "/* BDF data file input: %s */\n\n", inputFileName);

if (! noHeader)
    {
    fprintf(f, "#include \"common.h\"\n");
    fprintf(f, "#include \"memgfx.h\"\n");
    fprintf(f, "#include \"../gemfont.h\"\n\n");
    }

fprintf(f, "static UBYTE %s_data[%d] = {\n", name,
	font->frm_hgt * font->frm_wdt);

bytesOut = 0;
for (row = 0; row < font->frm_hgt ; ++row)
    {
    int byteWidth = font->frm_wdt;
    int col;
    for (col = 0; col < byteWidth; ++col)
	{
	fprintf(f, "%#0x,", bitmap[row][col]);
	++bytesOut;
	if (0 == (bytesOut % 10))	/* every ten produces new line */
	    fprintf(f,"\n");
	}
    }
if (0 != (bytesOut % 10))  /* if not already a new line for the last one */
    fprintf(f,"\n");

fprintf(f, "};\n\n");

fprintf(f, "static WORD %s_ch_ofst[%d] = {\n", name, maxGlyphCount+1);

for (glyphCount = 0; glyphCount < maxGlyphCount+1; ++glyphCount)
    {
    fprintf(f,"%d,",offsets[glyphCount]);
    if (0 == ((glyphCount+1) % 10))	/*	every ten produces new line */
	fprintf(f,"\n");
    }
if (0 != (glyphCount % 10))	/* if not already a new line for the last one */
    fprintf(f,"\n");

font->top_dist = font->frm_hgt;
font->asc_dist = font->frm_hgt + minYoff;
font->hlf_dist = font->top_dist / 2;
font->des_dist = - minYoff;
font->bot_dist = - minYoff;
font->wchr_wdt = maxXextent;
font->wcel_wdt = maxDwidth;

fprintf(f, "};\n\n");		/* done with ch_ofst[]	*/

fprintf(f, "static struct font_hdr %s_font = {\n", name);
fprintf(f, "STPROP, %hd, \"%s\", %hd, %hd,\n",
    font->size, font->facename, font->ADE_lo, font->ADE_hi);
fprintf(f, "%hd, %hd, %hd, %hd, %hd,\n", font->top_dist,
	font->asc_dist, font->hlf_dist, font->des_dist, font->bot_dist);
fprintf(f, "%hd, %hd, %hd, %hd,\n",
    font->wchr_wdt, font->wcel_wdt, font->lft_ofst, font->thckning);
fprintf(f, "%hd, %hd, 0x%hx, (WORD)0x%hx,\n", font->rgt_ofst,
	font->undrline, font->lghtng_m, font->skewng_m);
fprintf(f, "0x%hx, NULL,\n", font->flags); /* flags, hz_ofst */
fprintf(f, "%s_ch_ofst, %s_data,\n", name, name); /* ch_ofst, fnt_dta */
fprintf(f, "%hd, %hd,\n", font->frm_wdt, font->frm_hgt);
fprintf(f, "NULL,\n");	/*	nxt_fnt	*/
fprintf(f, "%hd, %hd,   /* x/y offset */\n", font->xOff, font->yOff);
fprintf(f, "%hd,        /* lineHeight */\n", font->lineHeight);
fprintf(f, "};\n\n");

fprintf(f, "MgFont *mg%sFont()\n", name);
fprintf(f, "{\n");
fprintf(f, "return &%s_font;\n", name);
fprintf(f, "}\n");

carefulClose(&f);
}

static void bedToGem(char *bdfFile, char *outputFile)
/* read the given bdfFile - convert to gem .c source definition file */
{
struct lineFile *lf = lineFileOpen(bdfFile, TRUE);
char *line = (char *)NULL;
unsigned lineCount = 0;		/* of lines in the bdf file	*/
struct font_hdr fontHeader;  /* structure to be filled in during conversion */
char *words[128];	/*	for splitting lines	*/
int wordCount = 0;	/*	for splitting lines	*/
int asciiLo = BIGNUM;	/*	a record of ASCII values encountered	*/
int asciiHi = 0;	/*	lowest and highest, should be: [LO-HI]	*/
int validGlyphs = 0;	/*	a count of chars found in range: [LO-HI] */
boolean skipToNext = FALSE;	/* ignore characters not in range [LO-HI] */
boolean readingBitmap = FALSE;	/* working on the bitmap lines	*/
int maxDwidth = 0;	/*	a record of the maximum extents found */
int minW = BIGNUM;	/*	for individual char bounding box	*/
int maxH = 0;		/*	dimensions, smallest and largest box,	*/
int minH = BIGNUM;	/*	for sanity checking purposes.	*/
int BBw = 0;		/*	the current character bounding box	*/
int BBh = 0;		/*	width, height, X,Y offsets	*/
int BBxOff = 0;
int BByOff = 0;
int glyphRow = 0;	/*	to count bitmap rows read for current char */
int encoding = 0;	/*	the current character's ascii value	*/
int maxX = 0;		/*	largest bounding box possible	*/
int maxY = 0;		/* 	extracted from FOUNTBOUNDINGBOX line */
int offX = 0;		/*	offset to X=0 in the FONTBOUNDINGBOX */
int offY = 0;		/*	offset to Y=0 in the FONTBOUNDINGBOX */
int combinedWidth = 0;	/*	sum of all glyph dWidths	*/
struct bdfGlyph *curGlyph = NULL;    /* to accumulate the current glyph data */
struct bdfGlyph *glyphList = NULL;   /* list of all glyphs read in */
int minYoff = BIGNUM;
int maxYcoord = 0;
int minYcoord = BIGNUM;
int yRange = 0;

fontHeader.id = STPROP;
fontHeader.size = 0;
fontHeader.facename[0] = (char)NULL;
fontHeader.ADE_lo = 0;
fontHeader.ADE_hi = 0;
fontHeader.top_dist = 0;
fontHeader.asc_dist = 0;
fontHeader.hlf_dist = 0;
fontHeader.des_dist = 0;
fontHeader.bot_dist = 0;
fontHeader.wchr_wdt = 0;
fontHeader.wcel_wdt = 0;
fontHeader.lft_ofst = 0;
fontHeader.rgt_ofst = 0;
fontHeader.thckning = 0;
fontHeader.undrline = 0;
fontHeader.lghtng_m = 0x5555;
fontHeader.skewng_m = 0xaaaa;
fontHeader.flags = 0;
fontHeader.hz_ofst = NULL;
fontHeader.ch_ofst = NULL;
fontHeader.fnt_dta = NULL;
fontHeader.frm_wdt = 0;	/* will be byte width of all glyphs together */
fontHeader.frm_hgt = 0;	/* will be a single glyph height */
fontHeader.nxt_fnt = (struct font_hdr *)NULL;
fontHeader.xOff = 0;
fontHeader.yOff = 0;

while (lineFileNext(lf, &line, NULL))
    {
    ++lineCount;
    if (skipToNext && !startsWith("STARTCHAR ", line))
	continue;
    skipToNext = FALSE;
    if (readingBitmap && !startsWith("ENDCHAR", line))
	{
	unsigned char uc = 0;
	int i, j;
	int len = strlen(line);
	char *c = line;
	j = 0;
	for (i = 0; i < len; ++i)
	    {
	    uc <<= 4;
	    switch (*c)
		{
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		    uc |= (*c++ - '0') & 0x0f;
		    break;
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		    uc |= (*c++ - 'A' + 10) & 0x0f;
		    break;
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		    uc |= (*c++ - 'a' + 10) & 0x0f;
		    break;
		default:
		    errAbort("unrecognized hex digit: %c at line %d",
			*c, lineCount);
		}
	    if (i & 0x1)
		{
		curGlyph->bitmap[glyphRow][j++] = uc;
		uc = 0;
		}
	    }
	if (len & 0x1) /* odd length ?  can that be, no, shouldn't happen */
	    {
	    uc <<= 4;
	    curGlyph->bitmap[glyphRow][j] = uc;
	    errAbort("odd length of %d at line %d\n", len, lineCount);
	    }
	++glyphRow;
	continue;
	}
    readingBitmap = FALSE;
    if (startsWith("COMMENT", line))
	continue;
    else if (startsWith("FONT ", line))
	{
	char *name0;
	char *name1;
	wordCount = chopByWhite(line, words, ArraySize(words));
	if (wordCount < 2)
	    errAbort("can not find at least two words on FONT line");
	name0 = replaceChars(words[1], "-Adobe-Helvetica", "AdobeHelv");
	name1 = replaceChars(name0, "Bold", "B");
	freeMem(name0);
	name0 = replaceChars(name1, "Normal", "N");
	freeMem(name1);
	name1 = replaceChars(name0, "Medium", "M");
	snprintf(fontHeader.facename, sizeof(fontHeader.facename), "%s", name1);
	freeMem(name0);
	freeMem(name1);
	}
    else if (startsWith("SIZE ", line))
	{
	wordCount = chopByWhite(line, words, ArraySize(words));
	if (wordCount != 4)
	    errAbort("can not find four words on SIZE line");
	if (fontHeader.size > 0)
	    errAbort("seeing SIZE a second time at line: %d\n", lineCount);
	fontHeader.size = (WORD) sqlSigned(words[1]);
	}
    else if (startsWith("FONTBOUNDINGBOX ", line))
	{
	wordCount = chopByWhite(line, words, ArraySize(words));
	if (wordCount != 5)
	    errAbort("can not find five words on FONTBOUNDINGBOX line");
	maxX = sqlSigned(words[1]);
	maxY = sqlSigned(words[2]);
	offX = sqlSigned(words[3]);
	offY = sqlSigned(words[4]);
	verbose(2,"#\tlargest font size box: %d x %d, (0,0) offsets: %d, %d\n",
		maxX, maxY, offX, offY);
	}
    else if (startsWith("STARTCHAR ", line))
	{
	if ((0 == fontHeader.size) || (0 == maxX) || (0 == maxY))
	    errAbort("at a STARTCHAR but haven't seen proper sizes yet");
	/*	starting a character, allocate the single sized glyph area */
	curGlyph = allocGlyph(maxX, maxY);
	}
    else if (startsWith("ENDCHAR", line))
	{
	if (glyphRow != BBh)
	    errAbort("not the correct number of bitmap rows for char: %d\n",
		encoding);
	/*	finished with a character definition, add to list	*/
	slAddHead(&glyphList, curGlyph);
	if (curGlyph->yOff < minYoff) minYoff = curGlyph->yOff;
	if ((curGlyph->yOff + curGlyph->h) > maxYcoord)
		maxYcoord = curGlyph->yOff + curGlyph->h;
	if (curGlyph->yOff < minYcoord)
		minYcoord = curGlyph->yOff;
	if (curGlyph->dWidth < 1)
	    errAbort("less than one dWidth for character: %d",
		curGlyph->encoding);
	glyphRow = 0;		/* reset row counter for sanity checking */
	}
    else if (startsWith("BITMAP", line))
	{
	readingBitmap = TRUE;
	}
    else if (startsWith("ENCODING ", line))
	{
	wordCount = chopByWhite(line, words, ArraySize(words));
	if (wordCount != 2)
	    errAbort("can not find two words on ENCODING line");
	encoding = sqlSigned(words[1]);
	/* ignore all values outside the standard ascii */
	if ((encoding < LO_LMT) || (encoding > HI_LMT))
	    {
	    skipToNext = TRUE;
	    }
	else
	    {
	    if (encoding < asciiLo) asciiLo = encoding;
	    if (encoding > asciiHi) asciiHi = encoding;
	    ++validGlyphs;
	    curGlyph->encoding = encoding;
	    }
	}
    else if (startsWith("DWIDTH ", line))
	{
	wordCount = chopByWhite(line, words, ArraySize(words));
	if (wordCount != 3)
	    errAbort("can not find three words on DWIDTH line");
	curGlyph->dWidth = sqlSigned(words[1]);
	combinedWidth += curGlyph->dWidth;
	}
    else if (startsWith("BBX ", line))
	{
	wordCount = chopByWhite(line, words, ArraySize(words));
	if (wordCount != 5)
	    errAbort("can not find five words on BBX line");
	BBh = sqlSigned(words[2]);
	/*	I don't know why there are glyphs defined that are
 	 *	larger than the font size, I'm going to skip them
	 */
	BByOff = sqlSigned(words[4]);
	if (BBh > maxY)
	    {
	    skipToNext = TRUE;
	    --validGlyphs;
	    combinedWidth -= curGlyph->dWidth;
	    verbose(2,"#\t(BBh %d + BByOff %d) == %d > %d for character: %d '%c' - skipping this char\n",
		BBh, BByOff, BBh+BByOff, fontHeader.size, encoding,
		(char)encoding);
	    }
	else
	    {
	    BBw = sqlSigned(words[1]);
	    BBxOff = sqlSigned(words[3]);
	    if (BBw < minW) minW = BBw;
	    if (BBw > maxDwidth) maxDwidth = BBw;
	    if (BBh < minH) minH = BBh;
	    if (BBh > maxH) maxH = BBh;
	    curGlyph->w = BBw;
	    curGlyph->h = BBh;
	    curGlyph->xOff = BBxOff;
	    curGlyph->yOff = BByOff;
	    if ((curGlyph->xOff < offX) || (curGlyph->yOff < offY))
		errAbort("a glyph's x,y offsets are out of range: %d,%d vs limits %d,%d\n", curGlyph->xOff, curGlyph->yOff, offX, offY);
	    if (BBh > yRange) yRange = BBh;
	    }
	}
    }
lineFileClose(&lf);
verbose(2, "#\tread %d lines from file %s\n", lineCount, bdfFile);
verbose(2, "#\tfacename: %s\n", fontHeader.facename);
verbose(2, "#\tascii range: %d - %d, valid glyphs: %d\n", asciiLo, asciiHi,
	validGlyphs);
verbose(2, "#\tW,H range (%d-%d) (%d-%d), combinedWidth: %d\n", minW, minH,
	maxDwidth, maxH, combinedWidth);
verbose(2, "#\tminYoff: %d, minYcoord: %d, maxYcoord: %d, yRange: %d\n",
	minYoff, minYcoord, maxYcoord, yRange);
/*	the wdt needs to be adjusted after everything is scanned in outputGem.
 *	There may be missing glyphs which will affect the combined width.
 */
fontHeader.frm_wdt = combinedWidth;
fontHeader.frm_hgt = maxYcoord - minYcoord;
fontHeader.ADE_lo = asciiLo;
fontHeader.ADE_hi = asciiHi;

outputGem(outputFile, &fontHeader, glyphList, bdfFile);
freeGlyphList(&glyphList);
#ifdef SOON
#endif
}

int main( int argc, char *argv[] )
{
optionInit(&argc, argv, optionSpecs);

if (argc < 3)
    usage();

noHeader = optionExists("noHeader");
name = optionVal("name", NULL);

bedToGem(argv[1], argv[2]);
return(0);
}
