/* bdfToGem - convert font bdf files to Gem C source font definitions */

#include	"common.h"
#include	"memgfx.h"
#include	"sqlNum.h"
#include	"options.h"
#include	"linefile.h"
#include	"gemfont.h"

static char const rcsid[] = "$Id: bdfToGem.c,v 1.2 2005/02/16 21:11:50 hiram Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

static void usage()
{
errAbort(
"bdfToGem - convert font bdf files to Gem C source font definitions\n"
"usage: bdfToGem [options] <file.bdf> <gem_definition.c>\n"
"options: -verbose=2 to see processing stats"
);
}

static void bedToGem(char *bdfFile, char *outputFile)
{
struct lineFile *lf = lineFileOpen(bdfFile, TRUE);
char *line = (char *)NULL;
unsigned lineCount = 0;
struct font_hdr fontHeader;
char *words[128];
int wordCount = 0;
int asciiLo = BIGNUM;	/*	a record of ASCII values encountered	*/
int asciiHi = 0;	/*	lowest and highest, should be: [0-127]	*/
int validGlyphs = 0;	/*	a count of chars found in range: [0-127] */
boolean skipToNext = FALSE;	/* ignore characters not in range [0-127] */
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
			/*	to make it	*/

fontHeader.id = 0;
fontHeader.size = 0;
fontHeader.flags = 0;
fontHeader.nxt_fnt = (struct font_hdr *)NULL;

while (lineFileNext(lf, &line, NULL))
    {
    ++lineCount;
    if (skipToNext && !startsWith("STARTCHAR ", line))
	continue;
    skipToNext = FALSE;
    if (readingBitmap && !startsWith("ENDCHAR ", line))
	{
	++glyphRow;
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
	int byteWidth = 0;
	wordCount = chopByWhite(line, words, ArraySize(words));
	if (wordCount != 5)
	    errAbort("can not find five words on FONTBOUNDINGBOX line");
	maxX = sqlSigned(words[1]);
	maxY = sqlSigned(words[2]);
	byteWidth = (maxX + 3)/4;
	verbose(2,"have largest bitmap, %d x %d bytes\n", byteWidth, maxY);
	for (i = 0; i < maxY; ++i)
	    bitmapRows[i] = (char *) needMem((size_t) byteWidth);
	}
    else if (startsWith("STARTCHAR ", line))
	{
	/*	starting a character definition	*/
	if ((0 == fontHeader.size) || (0 == maxX) || (0 == maxY))
	    errAbort("at a STARTCHAR but haven't seen proper sizes yet");
	}
    else if (startsWith("ENDCHAR ", line))
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
	if ((asciiValue < 0) || (asciiValue > 127))
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
	BBw = sqlSigned(words[1]);
	BBh = sqlSigned(words[2]);
	BBxoff = sqlSigned(words[3]);
	BByoff = sqlSigned(words[4]);
	if (BBw < minW) minW = BBw;
	if (BBw > maxW) maxW = BBw;
	if (BBh < minH) minH = BBh;
	if (BBh > maxH) maxH = BBh;
	if (BBh > 18)
	    verbose(2, "BBh == %d for character: %d\n", BBh, asciiValue);
	}
    }
lineFileClose(&lf);
verbose(2, "read %d lines from file %s\n", lineCount, bdfFile);
verbose(2, "facename: %s\n", fontHeader.facename);
verbose(2, "ascii range: %d - %d, valid glyphs: %d\n", asciiLo, asciiHi,
	validGlyphs);
verbose(2, "W,H range (%d-%d) (%d-%d)\n", minW, minH, maxW, maxH);
fontHeader.ADE_lo = asciiLo;
fontHeader.ADE_hi = asciiHi;
}

int main( int argc, char *argv[] )
{
optionInit(&argc, argv, optionSpecs);

if (argc < 3)
    usage();

bedToGem(argv[1], argv[2]);
return(0);
}
