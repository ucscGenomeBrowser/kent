/* bdfToGem - convert font bdf files to Gem C source font definitions */

#include	"common.h"
#include	"memgfx.h"
#include	"sqlNum.h"
#include	"options.h"
#include	"linefile.h"
#include	"gemfont.h"

static char const rcsid[] = "$Id: bdfToGem.c,v 1.5 2005/02/18 01:16:58 hiram Exp $";

static char *name = (char *)NULL;	/* to name the font in the .c file */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
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
"    -verbose=4 - to see all glyphs as they are processed\n"
"    -verbose=2 - to see processing statistics"
);
}

/*	lower and upper limit of character values to accept	*/
#define LO_LMT	((int)' ')
#define HI_LMT	(127)
/*	given w pixels, round up to number of bytes needed	*/
#define BYTEWIDTH(w)	(((w) + 3)/4)
#define DEFAULT_FONT	"Small"

/*	a structure to store the incoming glyphs from the bdf file */
struct bdfGlyph
{
struct bdfGlyph *next;
int encoding;		/*	ascii value of character	*/
int w;
int h;
int xOff;
int yOff;
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
    errAbort("allocRows: w or h are zero: %d x %d", w, h);

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

static int encodeCmp(const void *va, const void *vb)
/* Compare to sort based on encoding value */
{
const struct bdfGlyph *a = *((struct bdfGlyph **)va);
const struct bdfGlyph *b = *((struct bdfGlyph **)vb);
return (a->encoding - b->encoding);
}

static void outputGem(char *out, struct font_hdr *font,
    struct bdfGlyph *glyphs)
/*	all input is done, now do the output file	*/
{
unsigned char **bitmap = (unsigned char **)NULL; /* all glyphs together */
FILE *f = mustOpen(out,"w");
struct bdfGlyph *glyph;
int glyphCount = 0;
int maxGlyphCount = HI_LMT - LO_LMT + 1;
int encoding = 0;
int missing = 0;
int offset = 0;
int minXoff = BIGNUM;
int maxXoff = -BIGNUM;
int minYoff = BIGNUM;
int maxYoff = -BIGNUM;
int maxYextent = 0;
int maxXextent = 0;
int maxDwidth = 0;
int bytesOut = 0;
int row;

bitmap = allocRows(font->frm_wdt, font->frm_hgt);
verbose(2,"#\tallocated result bitmap: %d x %d\n",
	font->frm_wdt, font->frm_hgt);

slSort(&glyphs, encodeCmp);	/*	order glyphs by encoding value */

/*	for our purposes here, the lowest limit must exist (space)
 *	which will also be used to fill missing glyphs
 */
if (glyphs->encoding != LO_LMT)
    errAbort("first character is not space: %d, we need a space\n",
	glyphs->encoding);

/*	Survey the individual glyph bounding boxes, find max,min extents
 *	Sanity check against given maximums for the whole bitmap
 *	as it will be put together.
 */
encoding = glyphs->encoding - 1;	/* to check for missing glyphs */
for (glyph = glyphs; glyph; glyph=glyph->next)
    {
    int yExtent;
    if (glyph->encoding != (encoding + 1))
	{
	verbose(2, "#\tmissing glyph between encodings: %d - %d\n", 
		encoding, glyph->encoding);
	missing += glyph->encoding - encoding;
	}
    encoding = glyph->encoding;
    if (glyph->xOff < minXoff) minXoff = glyph->xOff;
    if (glyph->xOff > maxXoff) maxXoff = glyph->xOff;
    if (glyph->yOff < minYoff) minYoff = glyph->yOff;
    if (glyph->yOff > maxYoff) maxYoff = glyph->yOff;
    yExtent = glyph->h;
    if (yExtent < 0)
	errAbort("negative yExtent for glyph: %d, h: %d, yOff: %d",
		glyph->encoding, glyph->h, glyph->yOff);
    if (yExtent > font->frm_hgt)
	errAbort("yExtent larger than possible for glyph: %d, h: %d, yOff: %d",
 		glyph->encoding, glyph->h, glyph->yOff);
    if (glyph->w > maxXextent) maxXextent = glyph->w;
    if (glyph->dWidth > maxDwidth) maxDwidth = glyph->dWidth;
    if (yExtent > maxYextent) maxYextent = yExtent;
    ++glyphCount;
    }


if (glyphCount > maxGlyphCount)
    errAbort("found more glyphs than allowed: %d vs. %d\n",
	glyphCount, maxGlyphCount);

verbose(2, "#\thave %d glyphs to merge, missing: %d\n", glyphCount, missing);
verbose(2, "#\tmin,max X offsets: %d, %d, maxXextent: %d\n",
	minXoff, maxXoff, maxXextent);
verbose(2, "#\tmin,max Y offsets: %d, %d, maxYextent: %d\n",
	minYoff, maxYoff, maxYextent);

//#       min,max X offsets: -1, 2, maxXextent: 16
//#       min,max Y offsets: -4, 11, maxYextent: 18
/*	Now that we know our and minYoff, this becomes our
 *	reference to coordinate Y=0 in the global bitmap.
 *	To copy an individual glyph into its place in the global bitmap,
 *	the combination of the individual glyph's yOff with this minYoff
 *	will move it to the correct row in the global bitmap.
 */

offset = 0;		/*	means X=byte position in global bitmap */
glyphCount = 0;
encoding = glyphs->encoding - 1;
for (glyph = glyphs; glyph; glyph=glyph->next)
    {
    int byteWidth;

    if (glyph->encoding != (encoding + 1))
	{
	verbose(2, "#\tmissing glyph between encodings: %d - %d\n", 
		encoding, glyph->encoding);
	missing += glyph->encoding - encoding;
	errAbort("need to fill in this code to fill in the space with blank");
	}
    byteWidth = BYTEWIDTH(glyph->dWidth);
    /* adjust starting row here by yOff and minYoff	TBD */
    for (row = 0; row < glyph->h; ++row)
	{
	int col;
	for (col = 0; col < byteWidth; ++col)
	    {
	    bitmap[row][offset+col] = glyph->bitmap[row][col];
	    }
	}
    encoding = glyph->encoding;
    offset += byteWidth;	/* next glyph x position in global bitmap */
    ++glyphCount;
    }

if ((char *)NULL == name)
	name = cloneString(DEFAULT_FONT);	/*	default name of font */

fprintf(f, "\n/* %s.c - compiled data for font %s */\n\n", name,font->facename);

fprintf(f, "#include \"common.h\"\n");
fprintf(f, "#include \"memgfx.h\"\n");
fprintf(f, "#include \"gemfont.h\"\n\n");

fprintf(f, "static UBYTE %s_data[%d] = {\n", name,
	font->frm_hgt * BYTEWIDTH(font->frm_wdt));

bytesOut = 0;
for (row = 0; row < font->frm_hgt ; ++row)
    {
    int byteWidth = BYTEWIDTH(font->frm_wdt);
    int col;
    for (col = 0; col < byteWidth; ++col)
	{
	fprintf(f, "%#0x,", bitmap[row][col]);
	++bytesOut;
	if (0 == (bytesOut % 10))	/* every eight produces new line */
	    fprintf(f,"\n");
	}
    }
if (0 != (bytesOut % 10))  /* if not already a new line for the last one */
    fprintf(f,"\n");

fprintf(f, "};\n\n");

fprintf(f, "static WORD %s_ch_ofst[%d] = {\n", name, glyphCount);

glyphCount = 0;
encoding = glyphs->encoding - 1;
offset = 0;
for (glyph = glyphs; glyph; glyph=glyph->next)
    {
    int byteWidth;

    if (glyph->encoding != (encoding + 1))
	fprintf(f,"0,");	/*	missing chars are space */
    encoding = glyph->encoding;
    fprintf(f,"%d,",offset);
    byteWidth = BYTEWIDTH(glyph->dWidth);
    offset += byteWidth;
    ++glyphCount;
    if (0 == (glyphCount % 8))	/*	every eight produces new line */
	fprintf(f,"\n");
    }

if (0 != (glyphCount % 8))	/* if not already a new line for the last one */
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
fprintf(f, "%hd, %hd, /* x/y offset */\n", font->xOff, font->yOff);
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
int BBxoff = 0;
int BByoff = 0;
int glyphRow = 0;	/*	to count bitmap rows read for current char */
int encoding = 0;	/*	the current character's ascii value	*/
int maxX = 0;		/*	largest bounding box possible	*/
int maxY = 0;		/* 	extracted from FOUNTBOUNDINGBOX line */
int offX = 0;		/*	offset to X=0 in the FONTBOUNDINGBOX */
int offY = 0;		/*	offset to Y=0 in the FONTBOUNDINGBOX */
int combinedWidth = 0;	/*	sum of all glyph dWidths	*/
struct bdfGlyph *curGlyph = NULL;    /* to accumulate the current glyph data */
struct bdfGlyph *glyphList = NULL;   /* list of all glyphs read in */

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
fontHeader.hz_ofst = (char *)NULL;
fontHeader.ch_ofst = (WORD *)NULL;
fontHeader.fnt_dta = (UBYTE *)NULL;
fontHeader.frm_wdt = 0;	/* will be sum of all glyph widths */
fontHeader.frm_hgt = 0;	/* will be pixel height for tallest glyph */
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
verbose(4, "processin bitmap line: %s at line %d, char: %d ... ",
	line, lineCount, encoding);
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
	    errAbort("odd length of %d at line %d\n", lineCount);
	    }
	for (i = 0; i < j; ++i)
	    verbose(4, "%02X", curGlyph->bitmap[glyphRow][i]);
	verbose(4,"\n");
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
	if (BBh > fontHeader.size)
	    {
	    skipToNext = TRUE;
	    --validGlyphs;
	    verbose(2,"BBh == %d > %d for character: %d - skipping this char\n",
		BBh, fontHeader.size, encoding);
	    }
	else
	    {
	    BBw = sqlSigned(words[1]);
	    BBxoff = sqlSigned(words[3]);
	    BByoff = sqlSigned(words[4]);
	    if (BBw < minW) minW = BBw;
	    if (BBw > maxDwidth) maxDwidth = BBw;
	    if (BBh < minH) minH = BBh;
	    if (BBh > maxH) maxH = BBh;
	    curGlyph->w = BBw;
	    curGlyph->h = BBh;
	    curGlyph->xOff = BBxoff;
	    curGlyph->yOff = BByoff;
	    if ((curGlyph->xOff < offX) || (curGlyph->yOff < offY))
		errAbort("a glyph's x,y offsets are out of range: %d,%d vs limits %d,%d\n", curGlyph->xOff, curGlyph->yOff, offX, offY);
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
fontHeader.frm_wdt = combinedWidth;
fontHeader.frm_hgt = maxH;
fontHeader.ADE_lo = asciiLo;
fontHeader.ADE_hi = asciiHi;


outputGem(outputFile, &fontHeader, glyphList);
}

int main( int argc, char *argv[] )
{
optionInit(&argc, argv, optionSpecs);

if (argc < 3)
    usage();

name = optionVal("name", NULL);

bedToGem(argv[1], argv[2]);
return(0);
}
