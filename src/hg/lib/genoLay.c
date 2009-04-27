/* genoLay - genome layout. Arranges chromosomes so that they
 * tend to fit together nicely on a single page. */

#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "vGfx.h"
#include "cytoBand.h"
#include "hCytoBand.h"
#include "genoLay.h"

static char const rcsid[] = "$Id: genoLay.c,v 1.10 2008/09/17 18:10:13 kent Exp $";

void genoLayDump(struct genoLay *gl)
/* Print out info on genoLay */
{
struct genoLayChrom *chrom;
struct slRef *left, *right, *ref;
int total;
printf("gl: lineCount %d, leftLabelWidth %d, rightLabelWidth %d, basesPerPixel %f<BR>\n",
	gl->lineCount, gl->leftLabelWidth, gl->rightLabelWidth, gl->basesPerPixel);
for (left = gl->leftList, right = gl->rightList; left != NULL || right != NULL;)
    {
    total=0;
    if (left != NULL)
	{
	chrom = left->val;
	printf("%s@%d,%d[%d] %d ----  ", chrom->fullName, chrom->x, chrom->y, chrom->width, chrom->size);
	total += chrom->size;
        left = left->next;
	}
    if (right != NULL)
	{
	chrom = right->val;
	printf("%d  %s@%d,%d[%d]", chrom->size, chrom->fullName, chrom->x, chrom->y, chrom->width);
	total += chrom->size;
        right = right->next;
	}
    printf(" : %d<BR>", total);
    }
total=0;
for (ref = gl->bottomList; ref != NULL; ref = ref->next)
    {
    chrom = ref->val;
    total += chrom->size;
    printf("%s@%d,%d[%d] %d ...  ", chrom->fullName, chrom->x, chrom->y, chrom->width, chrom->size);
    }
printf(" : %d<BR>", total);
}

int genoLayChromCmpName(const void *va, const void *vb)
/* Compare two chromosome names so as to sort numerical part
 * by number.. */
{
const struct genoLayChrom *a = *((struct genoLayChrom **)va);
const struct genoLayChrom *b = *((struct genoLayChrom **)vb);
char *aName = a->shortName, *bName = b->shortName;
if (isdigit(aName[0]))
    {
    if (isdigit(bName[0]))
	{
	int diff = atoi(aName) - atoi(bName);
	if (diff == 0)
	    diff = strcmp(skipNumeric(aName), skipNumeric(bName));
	return diff;
	}
    else
        return -1;
    }
else if (isdigit(bName[0]))
    return 1;
else
    return strcmp(aName, bName);
}

struct genoLayChrom *genoLayDbChroms(struct sqlConnection *conn, 
	boolean withRandom)
/* Get chrom info list. */
{
struct sqlResult *sr;
char **row;
struct genoLayChrom *chrom, *chromList = NULL;
int count = sqlQuickNum(conn, "select count(*) from chromInfo");
if (count > 500)
    errAbort("Sorry, can only do genome layout on assemblies mapped to chromosomes. This one has %d contigs. Please select another organism or assembly.", count);
sr = sqlGetResult(conn, "select chrom,size from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    if (withRandom || (startsWith("chr", name) && 
    	!strchr(name, '_') && !sameString("chrM", name) 
	))
        {
	AllocVar(chrom);
	chrom->fullName = cloneString(name);
	chrom->shortName = chrom->fullName+3;
	chrom->size = sqlUnsigned(row[1]);
	slAddHead(&chromList, chrom);
	}
    }
if (slCount(chromList) > 1)
    {
    struct genoLayChrom *chrUn = slNameFind(chromList, "chrUn");
    if (chrUn)
	slRemoveEl(chromList, chrUn);
    }
if (chromList == NULL)
    errAbort("No chromosomes starting with chr in chromInfo.");
slReverse(&chromList);
slSort(&chromList, genoLayChromCmpName);
return chromList;
}

static void separateSexChroms(struct slRef *in,
	struct slRef **retAutoList, struct slRef **retSexList)
/* Separate input chromosome list into sex and non-sex chromosomes. */
{
struct slRef *autoList = NULL, *sexList = NULL, *ref, *next;

for (ref = in; ref != NULL; ref = next)
    {
    struct genoLayChrom *chrom = ref->val;
    char *name = chrom->shortName;
    next = ref->next;
    if (sameWord(name, "X") || sameWord(name, "Y") || sameWord(name, "Z")
    	|| sameWord(name, "W"))
	{
	slAddHead(&sexList, ref);
	}
    else
        {
	slAddHead(&autoList, ref);
	}
    }
slReverse(&sexList);
slReverse(&autoList);
*retAutoList = autoList;
*retSexList = sexList;
}

struct genoLay *genoLayNew(struct genoLayChrom *chromList,
	MgFont *font, int picWidth, int betweenChromHeight,
	int minLeftLabelWidth, int minRightLabelWidth,
	char *how)
/* Figure out layout.  For human and most mammals this will be
 * two columns with sex chromosomes on bottom.  This is complicated
 * by the platypus having a bunch of sex chromosomes. */
{
int margin = 3;
struct slRef *refList = NULL, *ref, *left, *right;
struct genoLayChrom *chrom;
struct genoLay *gl;
int autoCount, halfCount, bases, chromInLine;
int leftLabelWidth=0, rightLabelWidth=0, labelWidth;
int spaceWidth = mgFontCharWidth(font, ' ');
int extraLabelPadding = 0;
int autosomeOtherPixels=0, sexOtherPixels=0;
int autosomeBasesInLine=0;	/* Maximum bases in a line for autosome. */
int sexBasesInLine=0;		/* Bases in line for sex chromsome. */
double sexBasesPerPixel, autosomeBasesPerPixel, basesPerPixel;
int pos = margin;
int y = 0;
int fontHeight = mgFontLineHeight(font);
int chromHeight = fontHeight;
int lineHeight = chromHeight + betweenChromHeight;
boolean allOneLine = FALSE;

refList = refListFromSlList(chromList);

/* Allocate genoLay object and fill in simple fields. */
AllocVar(gl);
gl->chromList = chromList;
gl->chromHash = hashNew(0);
gl->font = font;
gl->picWidth = picWidth;
gl->margin = margin;
gl->spaceWidth = spaceWidth;
gl->lineHeight = lineHeight;
gl->betweenChromHeight = betweenChromHeight;
gl->betweenChromOffsetY = 0;
gl->chromHeight = chromHeight;
gl->chromOffsetY = lineHeight - chromHeight;


/* Save chromosomes in hash too, for easy access */
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    hashAdd(gl->chromHash, chrom->fullName, chrom);

if (sameString(how, genoLayOnePerLine))
    {
    gl->leftList = refList;
    }
else if (sameString(how, genoLayAllOneLine))
    {
    gl->bottomList = refList;
    allOneLine = TRUE;
    }
else
    {
    /* Put sex chromosomes on bottom, and rest on left. */
    separateSexChroms(refList, &refList, &gl->bottomList);
    autoCount = slCount(refList);
    gl->leftList = refList;

    /* If there are a lot of chromosomes, then move later
     * (and smaller) chromosomes to a new right column */
    if (autoCount > 12)
	{
	halfCount = (autoCount+1)/2;
	ref = slElementFromIx(refList, halfCount-1);
	gl->rightList = ref->next;
	ref->next = NULL;
	slReverse(&gl->rightList);
	}
    }

if (allOneLine)
    {
    unsigned long totalBases = 0, bStart=0, bEnd;
    int chromCount = 0, chromIx=0;
    for (ref = gl->bottomList; ref != NULL; ref = ref->next)
        {
	chrom = ref->val;
	totalBases += chrom->size;
	chromCount += 1;
	}
    int availablePixels = picWidth - minLeftLabelWidth - minRightLabelWidth
       - 2*margin - (chromCount-1);
    double basesPerPixel = (double)totalBases/availablePixels;
    gl->picHeight = 2*margin + lineHeight + fontHeight;
    for (ref = gl->bottomList; ref != NULL; ref = ref->next)
        {
	chrom = ref->val;
	bEnd = bStart + chrom->size;
	int pixStart = round(bStart / basesPerPixel);
	int pixEnd = round(bEnd / basesPerPixel);
	chrom->width = pixEnd - pixStart;
	chrom->height = lineHeight;
	chrom->x = pixStart + margin + chromIx + minLeftLabelWidth;
	chrom->y = 0;
	chromIx += 1;
	bStart = bEnd;
	}
    gl->lineCount = 1;
    gl->picHeight = 2*margin + lineHeight + fontHeight + 1;
    gl->allOneLine = TRUE;
    gl->leftLabelWidth = minLeftLabelWidth;
    gl->rightLabelWidth = minRightLabelWidth;
    gl->basesPerPixel = basesPerPixel;
    gl->pixelsPerBase = 1.0/basesPerPixel;
    }
else
    {
    /* Figure out space needed for autosomes. */
    left = gl->leftList;
    right = gl->rightList;
    while (left || right)
	{
	bases = 0;
	chromInLine = 0;
	if (left)
	    {
	    chrom = left->val;
	    labelWidth = mgFontStringWidth(font, chrom->shortName) + spaceWidth;
	    if (leftLabelWidth < labelWidth)
		leftLabelWidth = labelWidth;
	    bases = chrom->size;
	    left = left->next;
	    }
	if (right)
	    {
	    chrom = right->val;
	    labelWidth = mgFontStringWidth(font, chrom->shortName) + spaceWidth;
	    if (rightLabelWidth < labelWidth)
		rightLabelWidth = labelWidth;
	    bases += chrom->size;
	    right = right->next;
	    }
	if (autosomeBasesInLine < bases)
	    autosomeBasesInLine = bases;
	gl->lineCount += 1;
	}

    /* Figure out space needed for bottom chromosomes. */
    if (gl->bottomList)
	{
	gl->lineCount += 1;
	sexOtherPixels = spaceWidth + 2*margin;
	for (ref = gl->bottomList; ref != NULL; ref = ref->next)
	    {
	    chrom = ref->val;
	    sexBasesInLine += chrom->size;
	    labelWidth = mgFontStringWidth(font, chrom->shortName) + spaceWidth;
	    if (ref == gl->bottomList )
		{
		if (leftLabelWidth < labelWidth)
		    leftLabelWidth  = labelWidth;
		sexOtherPixels = leftLabelWidth;
		}
	    else if (ref->next == NULL)
		{
		if (rightLabelWidth < labelWidth)
		    rightLabelWidth  = labelWidth;
		sexOtherPixels += rightLabelWidth + spaceWidth;
		}
	    else
		{
		sexOtherPixels += labelWidth + spaceWidth;
		}
	    }
	}

    /* Do some adjustments if side labels are bigger than needed for
     * chromosome names. */
    if (leftLabelWidth < minLeftLabelWidth)
	{
	extraLabelPadding += (minLeftLabelWidth - leftLabelWidth);
	leftLabelWidth = minLeftLabelWidth;
	}
    if (rightLabelWidth < minRightLabelWidth)
	{
	extraLabelPadding += (minRightLabelWidth - rightLabelWidth);
	rightLabelWidth = minRightLabelWidth;
	}
    sexOtherPixels += extraLabelPadding;

    /* Figure out the number of bases needed per pixel. */
    autosomeOtherPixels = 2*margin + spaceWidth + leftLabelWidth + rightLabelWidth;
    basesPerPixel = autosomeBasesPerPixel 
	    = autosomeBasesInLine/(picWidth-autosomeOtherPixels);
    if (gl->bottomList)
	{
	sexBasesPerPixel = sexBasesInLine/(picWidth-sexOtherPixels);
	if (sexBasesPerPixel > basesPerPixel)
	    basesPerPixel = sexBasesPerPixel;
	}

    /* Save positions and sizes of some things in layout structure. */
    gl->leftLabelWidth = leftLabelWidth;
    gl->rightLabelWidth = rightLabelWidth;
    gl->basesPerPixel = basesPerPixel;
    gl->pixelsPerBase = 1.0/basesPerPixel;

    /* Set pixel positions for left autosomes */
    for (ref = gl->leftList; ref != NULL; ref = ref->next)
	{
	chrom = ref->val;
	chrom->x = leftLabelWidth + margin;
	chrom->y = y;
	chrom->width = round(chrom->size/basesPerPixel);
	chrom->height = lineHeight;
	y += lineHeight;
	}

    /* Set pixel positions for right autosomes */
    y = 0;
    for (ref = gl->rightList; ref != NULL; ref = ref->next)
	{
	chrom = ref->val;
	chrom->width = round(chrom->size/basesPerPixel);
	chrom->height = lineHeight;
	chrom->x = picWidth - margin - rightLabelWidth - chrom->width;
	chrom->y = y;
	y += lineHeight;
	}
    gl->picHeight = 2*margin + lineHeight * gl->lineCount;
    y = gl->picHeight - margin - lineHeight;

    /* Set pixel positions for sex chromosomes */
    for (ref = gl->bottomList; ref != NULL; ref = ref->next)
	{
	chrom = ref->val;
	chrom->y = y;
	chrom->width = round(chrom->size/basesPerPixel);
	chrom->height = lineHeight;
	if (ref == gl->bottomList)
	    chrom->x = leftLabelWidth + margin;
	else if (ref->next == NULL)
	    chrom->x = picWidth - margin - rightLabelWidth - chrom->width;
	else
	    chrom->x = 2*spaceWidth+mgFontStringWidth(font,chrom->shortName) + pos;
	pos = chrom->x + chrom->width;
	}
    }
return gl;
}

static void leftLabel(struct hvGfx *hvg, struct genoLay *gl,
	struct genoLayChrom *chrom, int yOffset, int fontHeight,
	int color)
/* Draw a chromosome with label on left. */
{
hvGfxTextRight(hvg, gl->margin, chrom->y + yOffset, 
    chrom->x - gl->margin - gl->spaceWidth, fontHeight, color,
    gl->font, chrom->shortName);
}

static void rightLabel(struct hvGfx *hvg, struct genoLay *gl,
	struct genoLayChrom *chrom, int yOffset, int fontHeight, 
	int color)
/* Draw a chromosome with label on left. */
{
hvGfxText(hvg, chrom->x + chrom->width + gl->spaceWidth,
	chrom->y + yOffset, 
	color, gl->font, chrom->shortName);
}

static void midLabel(struct hvGfx *hvg, struct genoLay *gl,
	struct genoLayChrom *chrom, int yOffset, int fontHeight, 
	int color)
/* Draw a chromosome with label on left. */
{
MgFont *font = gl->font;
int textWidth = mgFontStringWidth(font, chrom->shortName);
hvGfxTextRight(hvg, chrom->x - textWidth - gl->spaceWidth, 
    chrom->y + yOffset, 
    textWidth, fontHeight, color,
    font, chrom->shortName);
}

void genoLayDrawChromLabels(struct genoLay *gl, struct hvGfx *hvg, int color)
/* Draw chromosomes labels in image */
{
struct slRef *ref;
struct genoLayChrom *chrom;
int pixelHeight = mgFontPixelHeight(gl->font);
if (gl->allOneLine)
    {
    int yOffset = gl->chromOffsetY + gl->chromHeight + 1;

    for (ref = gl->bottomList; ref != NULL; ref = ref->next)
	{
	chrom = ref->val;
	hvGfxTextCentered(hvg, chrom->x, yOffset, chrom->width, pixelHeight, color, gl->font,
		chrom->shortName);
	}
    }
else
    {
    int yOffset = gl->chromOffsetY + gl->chromHeight - pixelHeight;

    /* Draw chromosome labels. */
    for (ref = gl->leftList; ref != NULL; ref = ref->next)
	leftLabel(hvg, gl, ref->val, yOffset, pixelHeight, color);
    for (ref = gl->rightList; ref != NULL; ref = ref->next)
	rightLabel(hvg, gl, ref->val, yOffset, pixelHeight, color);
    for (ref = gl->bottomList; ref != NULL; ref = ref->next)
	{
	chrom = ref->val;
	if (ref == gl->bottomList)
	    leftLabel(hvg, gl, chrom, yOffset, pixelHeight, color);
	else if (ref->next == NULL)
	    rightLabel(hvg, gl, chrom, yOffset, pixelHeight, color);
	else
	    midLabel(hvg, gl, chrom, yOffset, pixelHeight, color);
	}
    }
}

void genoLayDrawSimpleChroms(struct genoLay *gl,
	struct hvGfx *hvg, int color)
/* Draw boxes for all chromosomes in given color */
{
int height = gl->chromHeight;
int yOffset = gl->chromOffsetY;
struct genoLayChrom *chrom;
for (chrom = gl->chromList; chrom != NULL; chrom = chrom->next)
    hvGfxBox(hvg, chrom->x, chrom->y + yOffset, 
	chrom->width, height, color);

}

void genoLayDrawBandedChroms(struct genoLay *gl, struct hvGfx *hvg, char *db,
	struct sqlConnection *conn, Color *shadesOfGray, int maxShade, 
	int defaultColor)
/* Draw chromosomes with centromere and band glyphs. 
 * Get the band data from the database.  If the data isn't
 * there then draw simple chroms in default color instead */
{
char *bandTable = "cytoBandIdeo";
int yOffset = gl->chromOffsetY;
genoLayDrawSimpleChroms(gl, hvg, defaultColor);
if (sqlTableExists(conn, bandTable) && !gl->allOneLine)
    {
    int centromereColor = hCytoBandCentromereColor(hvg);
    double pixelsPerBase = 1.0/gl->basesPerPixel;
    int height = gl->chromHeight;
    int innerHeight = gl->chromHeight-2;
    struct genoLayChrom *chrom;
    boolean isDmel = hCytoBandDbIsDmel(db);
    boolean bColor = hvGfxFindColorIx(hvg, 200, 150, 150);
    int fontPixelHeight = mgFontPixelHeight(gl->font);
    for (chrom = gl->chromList; chrom != NULL; chrom = chrom->next)
	{
	boolean gotAny = FALSE;
	struct sqlResult *sr;
	char **row;
	char query[256];
	int cenX1=BIGNUM, cenX2=0;
	int y = chrom->y + yOffset;

	/* Fetch bands from database and draw them. */
	safef(query, sizeof(query), "select * from %s where chrom='%s'",
		bandTable, chrom->fullName);
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    struct cytoBand band;
	    int x1, x2;
	    cytoBandStaticLoad(row, &band);
	    x1 = pixelsPerBase*band.chromStart;
	    x2 = pixelsPerBase*band.chromEnd;
	    if (sameString(band.gieStain, "acen"))
		{
		/* Centromere is represented as two adjacent bands.
		 * We'll just record the extents of it here, and draw it
		 * in one piece later. */
		if (x1 < cenX1)
		    cenX1 = x1;
		if (x2 > cenX2)
		    cenX2 = x2;
		}
	    else
		{
		/* Draw band */
		hCytoBandDrawAt(&band, hvg, x1+chrom->x, y+1, x2-x1, innerHeight, 
			isDmel, gl->font, fontPixelHeight, MG_BLACK, bColor,
		    shadesOfGray, maxShade);
		gotAny = TRUE;
		}
	    }
	sqlFreeResult(&sr);

	/* Draw box around chromosome */
	hvGfxBox(hvg, chrom->x, y, chrom->width, 1, MG_BLACK);
	hvGfxBox(hvg, chrom->x, y+height-1, chrom->width, 1, MG_BLACK);
	hvGfxBox(hvg, chrom->x, y, 1, height, MG_BLACK);
	hvGfxBox(hvg, chrom->x+chrom->width-1, y, 1, height, MG_BLACK);

	/* Draw centromere if we found one. */
	if (cenX2 > cenX1)
	    {
	    hCytoBandDrawCentromere(hvg, cenX1+chrom->x, y, cenX2-cenX1, height,
	       MG_WHITE, centromereColor);
	    }
	}
    }
}

