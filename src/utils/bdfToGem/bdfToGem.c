/* bdfToGem - convert font bdf files to Gem C source font definitions */

#include	"common.h"
#include	"memgfx.h"
#include	"sqlNum.h"
#include	"options.h"
#include	"linefile.h"
#include	"gemfont.h"

static char const rcsid[] = "$Id: bdfToGem.c,v 1.3 2005/02/17 00:55:37 hiram Exp $";

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
"    -name=name - the name of the font to place into the .c file\n"
"    -verbose=4 - to see all glyphs as they are processed\n"
"    -verbose=2 - to see processing statistics"
);
}

static void outputGem(char *out, struct font_hdr *font)
{
FILE *f = mustOpen(out,"w");

if ((char *)NULL == name)
	name = cloneString("name");	/*	default name of font */

fprintf(f, "\n/* %s.c - compiled data for font %s */\n\n", name,font->facename);

fprintf(f, "#include \"common.h\"\n");
fprintf(f, "#include \"memgfx.h\"\n");
fprintf(f, "#include \"gemfont.h\"\n\n");

fprintf(f, "UBYTE %s_data[] = {\n", name);
fprintf(f, "};\n\n");

fprintf(f, "static WORD %s_ch_ofst[257] = {\n", name);
fprintf(f, "};\n\n");

fprintf(f, "struct font_hdr %s_font = {\n", name);
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

fprintf(f, "MgFont *mgName()\n");
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
int asciiHi = 0;	/*	lowest and highest, should be: [0-255]	*/
int validGlyphs = 0;	/*	a count of chars found in range: [0-255] */
boolean skipToNext = FALSE;	/* ignore characters not in range [0-255] */
boolean readingBitmap = FALSE;	/* working on the bitmap lines	*/
int maxW = 0;		/*	a record of the maximum extents found */
int minW = BIGNUM;	/*	for individual char bounding box	*/
int maxH = 0;		/*	dimensions, smallest and largest box,	*/
int minH = BIGNUM;	/*	for sanity checking purposes.	*/
int BBw = 0;		/*	the current character bounding box	*/
int BBh = 0;		/*	width, height, X,Y offsets	*/
int BBxoff = 0;
int BByoff = 0;
int glyphRow = 0;	/*	to count bitmap rows read for current char */
int asciiValue = 0;;	/*	the current character's ascii value	*/
int maxX = 0;		/*	largest bounding box possible	*/
int maxY = 0;		/* 	extracted from FOUNTBOUNDINGBOX line */
unsigned char **bitmapRows = (unsigned char **)NULL;
			/*	a single bitmap large enough for all */
			/*	glyphs, allocated when we know how big */
			/*	to make it: bitmapRows[ROW][BYTE]	*/
int byteWidth = 0;	/*	width of a bitmap row in bytes	*/
int combinedWidth = 0;	/*	sum of all glyph widths	*/

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
	line, lineCount, asciiValue);
	for (i = 0; i < len; ++i)
	    {
	    uc <<= 4;
	    switch (*c)
		{
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		    uc |= (*c++ - '0') & 0xf;
		    break;
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		    uc |= (*c++ - 'A' + 10) & 0xf;
		    break;
		default:
		    errAbort("unrecognized hex digit: %c at line %d",
			*c, lineCount);
		}
	    if (i & 0x1)
		{
		bitmapRows[glyphRow][j++] = uc;
		uc = 0;
		}
	    }
	if (len & 0x1) /* odd length ?  can that be, no, shouldn't happen */
	    {
	    uc <<= 4;
	    bitmapRows[glyphRow][j] = uc;
	    errAbort("odd length of %d at line %d\n", lineCount);
	    }
	for (i = 0; i < j; ++i)
	    verbose(4, "%02X", bitmapRows[glyphRow][i]);
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
	int i;
	wordCount = chopByWhite(line, words, ArraySize(words));
	if (wordCount != 5)
	    errAbort("can not find five words on FONTBOUNDINGBOX line");
	maxX = sqlSigned(words[1]);
	maxY = sqlSigned(words[2]);
	byteWidth = (maxX + 3)/4;
	bitmapRows = (unsigned char **)
		needMem((size_t) (maxY * sizeof(unsigned char *)));
	for (i = 0; i < maxY; ++i)
	    bitmapRows[i] = (unsigned char *) needMem((size_t) byteWidth);
	verbose(2,"allocated largest bitmap: %d x %d bytes\n", byteWidth, maxY);
	}
    else if (startsWith("STARTCHAR ", line))
	{
	int i, j;
	if ((0 == fontHeader.size) || (0 == maxX) || (0 == maxY))
	    errAbort("at a STARTCHAR but haven't seen proper sizes yet");
	/*	starting a character definition, zero out the bitmap	*/
	for (i = 0; i < maxY; ++i)
	    for (j = 0; j < byteWidth; ++j)
		bitmapRows[i][j] = (unsigned char) NULL;
	}
    else if (startsWith("ENDCHAR", line))
	{
	if (glyphRow != BBh)
	    errAbort("not the correct number of bitmap rows for char: %d\n",
		asciiValue);
	/*	finished with a character definition	*/
	glyphRow = 0;
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
	asciiValue = sqlSigned(words[1]);
	/* ignore all values outside the standard ascii */
	if ((asciiValue < 0) || (asciiValue > 255))
	    {
	    skipToNext = TRUE;
	    }
	else
	    {
	    if (asciiValue < asciiLo) asciiLo = asciiValue;
	    if (asciiValue > asciiHi) asciiHi = asciiValue;
	    ++validGlyphs;
	    }
	}
    else if (startsWith("BBX ", line))
	{
	wordCount = chopByWhite(line, words, ArraySize(words));
	if (wordCount != 5)
	    errAbort("can not find five words on BBX line");
	BBh = sqlSigned(words[2]);
	/*	I don't know why there are glyphs defined that are
 	 *	larger than the font, I'm going to skip them
	 */
	if (BBh > fontHeader.size)
	    {
	    skipToNext = TRUE;
	    --validGlyphs;
	    verbose(2,"BBh == %d > %d for character: %d - skipping this char\n",
		BBh, fontHeader.size, asciiValue);
	    }
	else
	    {
	    BBw = sqlSigned(words[1]);
	    BBxoff = sqlSigned(words[3]);
	    BByoff = sqlSigned(words[4]);
	    if (BBw < minW) minW = BBw;
	    if (BBw > maxW) maxW = BBw;
	    if (BBh < minH) minH = BBh;
	    if (BBh > maxH) maxH = BBh;
	    combinedWidth += BBw;
	    }
	}
    }
lineFileClose(&lf);
verbose(2, "read %d lines from file %s\n", lineCount, bdfFile);
verbose(2, "facename: %s\n", fontHeader.facename);
verbose(2, "ascii range: %d - %d, valid glyphs: %d\n", asciiLo, asciiHi,
	validGlyphs);
verbose(2, "W,H range (%d-%d) (%d-%d), combinedWidth: %d\n", minW, minH,
	maxW, maxH, combinedWidth);
fontHeader.frm_wdt = combinedWidth;
fontHeader.frm_hgt = maxH;
fontHeader.ADE_lo = asciiLo;
fontHeader.ADE_hi = asciiHi;


outputGem(outputFile, &fontHeader);
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
