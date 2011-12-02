/* cda.c - cDNA Alignment structure.  This stores all the info except
 * the bases themselves on an cDNA alignment. 
 * 
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "dnautil.h"
#include "memgfx.h"
#include "sig.h"
#include "fuzzyFind.h"
#include "cda.h"


char *cdaLoadString(FILE *f)
/* Load in a string from CDA file. */
{
UBYTE size;
char *s;
if (!readOne(f, size))
    return NULL;
s = needMem(size+1);
mustRead(f, s, size);
return s;
}

static void cdaWriteString(FILE *f, char *s)
/* Write string out to CDA file. */
{
int size;
UBYTE usize;
if (s == NULL)
    s = "";
size = strlen(s);
usize = (UBYTE)size;
assert(size < 256);
writeOne(f, usize);
mustWrite(f, s, size);
}

void cdaReadBlock(FILE *f, struct cdaBlock *block)
/* Read one block from cda file. */
{
mustReadOne(f, block->nStart);
mustReadOne(f, block->nEnd);
mustReadOne(f, block->hStart);
block->hEnd = block->hStart + (block->nEnd - block->nStart);
mustReadOne(f, block->startGood);
mustReadOne(f, block->endGood);
mustReadOne(f, block->midScore);
}

static void cdaWriteBlock(FILE *f, struct cdaBlock *block)
/* Write one block to cda file. */
{
writeOne(f, block->nStart);
writeOne(f, block->nEnd);
writeOne(f, block->hStart);
writeOne(f, block->startGood);
writeOne(f, block->endGood);
writeOne(f, block->midScore);
}

void cdaFixChromStartEnd(struct cdaAli *cda)
/* Loop through blocks and figure out and fill in chromStart
 * and chromEnd. */
{
int start = 0x7fffffff;
int end = -start;
int count = cda->blockCount;
struct cdaBlock *block = cda->blocks;

while (--count >= 0)
    {
    if (start > block->hStart)
        start = block->hStart;
    if (end < block->hEnd)
        end = block->hEnd;
    ++block;
    }
cda->chromStart = start;
cda->chromEnd = end;
}

FILE *cdaOpenVerify(char *fileName)
/* Call this to open file and verify signature, then call cdaLoadOne
 * which returns NULL at EOF. */
{
FILE *aliFile;
bits32 sig;

aliFile = mustOpen(fileName, "rb");
mustReadOne(aliFile, sig);
if (sig != aliSig)
    errAbort("Bad signature %x on %s", sig, fileName);
return aliFile;
}

static FILE *cdaCreate(char *fileName)
/* Open file to write and put out signature. */
{
bits32 sig = aliSig;
FILE *f = mustOpen(fileName, "wb");
writeOne(f, sig);
return f;
}

struct cdaAli *cdaLoadOne(FILE *f)
/* Load one cdaAli from file.  Assumes file pointer is correctly positioned. */
{
struct cdaAli *ca;
struct cdaBlock *blocks;
int count;
int i;
char *name;
UBYTE chromIx;

if ((name = cdaLoadString(f)) == NULL)
    return NULL;
AllocVar(ca);
ca->name = name;
mustReadOne(f, ca->isEmbryonic);
mustReadOne(f, ca->baseCount);
mustReadOne(f, ca->milliScore);
mustReadOne(f, chromIx);
ca->chromIx = chromIx;
mustReadOne(f, ca->strand);
mustReadOne(f, ca->direction);
mustReadOne(f, ca->blockCount);
count = ca->blockCount;
ca->blocks = blocks = needMem(count * sizeof(blocks[0]));
for (i=0; i<count; ++i)
    cdaReadBlock(f, &blocks[i]);
cdaFixChromStartEnd(ca);
return ca;
}

static void cdaWriteOne(FILE *f, struct cdaAli *ca)
/* Write one cdaAli to file. */
{
int count;
int i;
struct cdaBlock *blocks;
UBYTE chromIx;

cdaWriteString(f, ca->name);
writeOne(f, ca->isEmbryonic);
writeOne(f, ca->baseCount);
writeOne(f, ca->milliScore);
if (ca->chromIx > 255)
    errAbort("chromIx too big in cdaWriteOne.");
chromIx = (UBYTE)ca->chromIx;
writeOne(f, chromIx);
writeOne(f, ca->strand);
writeOne(f, ca->direction);
writeOne(f, ca->blockCount);
count = ca->blockCount;
blocks = ca->blocks;
for (i=0; i<count; ++i)
    cdaWriteBlock(f, &blocks[i]);
}

void cdaWrite(char *fileName, struct cdaAli *cdaList)
/* Write out a cdaList to a cda file. */
{
FILE *f = cdaCreate(fileName);
struct cdaAli *ca;
for (ca = cdaList; ca != NULL; ca = ca->next)
    cdaWriteOne(f, ca);
fclose(f);
}

boolean cdaCloneIsReverse(struct cdaAli *cda)
/* Returns TRUE if clone (.3/.5 pair) aligns on reverse strand. */
{
boolean isReverse = (cda->direction == '-');
if (cda->strand == '-')
    isReverse = !isReverse;
return isReverse;
}

char cdaCloneStrand(struct cdaAli *cda)
/* Return '+' or '-' depending on the strand that clone (.3/.5 pair) aligns on. */
{
return cdaCloneIsReverse(cda) ? '-' : '+';
}

char cdaDirChar(struct cdaAli *cda, char chromStrand)
/* Return '>' or '<' or ' ' depending whether cDNA is going same, opposite, or
 * unknown alignment as the chromosome strand. */
{
boolean isReverse = cdaCloneIsReverse(cda);
if (chromStrand == '-')
    isReverse = !isReverse;
return (isReverse ? '<' : '>');    
}

void cdaRcOne(struct cdaAli *cda, int dnaStart, int baseCount)
/* Reverse complement one cda. DnaStart is typically display window start. */
{
struct cdaBlock *block, *endBlock;

for (block = cda->blocks, endBlock = block+cda->blockCount; block < endBlock; ++block)
    {
    int temp;
    UBYTE uc;
    temp = reverseOffset(block->hStart-dnaStart, baseCount) + dnaStart + 1;
    block->hStart = reverseOffset(block->hEnd-dnaStart, baseCount) + dnaStart + 1;
    block->hEnd = temp;
    temp = reverseOffset(block->nStart, cda->baseCount);
    block->nStart = reverseOffset(block->nEnd, cda->baseCount);
    block->nEnd = temp;
    uc = block->startGood;
    block->startGood = block->endGood;
    block->endGood = uc;
    }
block = cda->blocks;
endBlock -= 1;
for (;block < endBlock; ++block, --endBlock)
    {
    struct cdaBlock temp;
    temp = *block;
    *block = *endBlock;
    *endBlock = temp;
    }
}

void cdaRcList(struct cdaAli *cdaList, int dnaStart, int baseCount)
/* Reverse complement cda list. */
{
struct cdaAli *cda;
for (cda = cdaList; cda != NULL; cda = cda->next)
    cdaRcOne(cda, dnaStart, baseCount);
}


void cdaFreeAli(struct cdaAli *ca)
/* Free a single cdaAli. */
{
freeMem(ca->blocks);
freeMem(ca->name);
freeMem(ca);
}

void cdaFreeAliList(struct cdaAli **pList)
/* Free list of cdaAli. */
{
struct cdaAli *ca, *next;
next = *pList;
while ((ca = next) != NULL)
    {
    next = ca->next;
    cdaFreeAli(ca);
    }
*pList = NULL;
}


static void cdaCoalesceOne(struct cdaAli *ca, boolean updateScore)
/* Coalesce blocks separated by small amounts of noise. */
{
struct cdaBlock *left = ca->blocks;
struct cdaBlock *block = left+1;
struct cdaBlock *writeBlock = block;
int readCount = ca->blockCount;
int i;

/* Implicitly have read and written first one. */
for (i=1; i<readCount; ++i)
    {
    int hGap = intAbs(block->hStart - left->hEnd);
    int nGap = intAbs(block->nStart - left->nEnd);
    if (hGap > 2 || nGap > 2)
        {
        left = writeBlock;
        *writeBlock++ = *block;
        }
    else
        {
        int leftMatch, blockMatch;
        int totalMatch;

        /* Update score. */
        if (updateScore)
            {
            leftMatch = roundingScale(left->midScore, left->nEnd-left->nStart, 255);
            blockMatch = roundingScale(block->midScore, block->nEnd-block->nStart, 255);
            totalMatch = leftMatch + blockMatch - nGap - hGap;
            left->midScore = roundingScale(totalMatch, 255, block->nEnd-left->nStart);
            }

        /* Update ends. */
        left->hEnd = block->hEnd;
        left->nEnd = block->nEnd;
        left->endGood = block->endGood;  
        }
    ++block;
    }
ca->blockCount = writeBlock - ca->blocks;
}

void cdaCoalesceAll(struct cdaAli *ca, boolean updateScore)
/* Coalesce blocks separated by small amounts of noise. */
{
for (;ca != NULL; ca = ca->next)
    cdaCoalesceOne(ca, updateScore);
}

void cdaCoalesceBlocks(struct cdaAli *ca)
/* Coalesce blocks separated by small amounts of noise. */
{
cdaCoalesceAll(ca, TRUE);
}

void cdaCoalesceFast(struct cdaAli *ca)
/* Coalesce blocks as above, but don't update the score. */
{
cdaCoalesceAll(ca, FALSE);
}


void cdaShowAlignmentTrack(struct memGfx *mg, 
    int xOff, int yOff, int width, int height,  Color goodColor, Color badColor,
    int dnaSize, int dnaOffset, struct cdaAli *cda, char repeatChar)
/* Draw alignment on a horizontal track of picture. */
{
struct cdaBlock *block = cda->blocks;
int count = cda->blockCount;
int blockIx;
int scaledHeight = roundingScale(height, dnaSize, width);
MgFont *font = NULL;
int repeatCharWidth = 0;

if (repeatChar)
    {
    font = mgSmallFont();
    repeatCharWidth = mgFontCharWidth(font, repeatChar);
    }

for (blockIx = 0; blockIx < count; ++blockIx)
    {
    int x[4];
    int b[4];
    Color c[3];
    int quarterWid;
    int i;
    int w;

    b[0] = block->hStart;
    b[3] = block->hEnd;
    quarterWid = (b[3]-b[0]+2)>>2;
    if (quarterWid > scaledHeight)
        quarterWid = scaledHeight;
    b[1] = b[0] + quarterWid;
    b[2] = b[3] - quarterWid;

    c[0] = ((block->startGood >= 4 && (blockIx == 0 || block[-1].nEnd == block[0].nStart)) 
        ? goodColor : badColor);
    c[1] = ((block->midScore > 229) ? goodColor : badColor);
    c[2] = ((block->endGood >= 4 && (blockIx == count-1 || block[0].nEnd == block[1].nStart))
        ? goodColor : badColor);

    for (i=0; i<4; ++i)
        x[i] = roundingScale(b[i]-dnaOffset, width, dnaSize) + xOff;
    for (i=0; i<3; ++i)
        {
        if ((w = x[i+1] - x[i]) > 0)
            mgDrawBox(mg, x[i], yOff, w, height, c[i]);
        }
    
    if (repeatChar)
        {
        char buf[256];
        int charCount;
        w = x[3] - x[0];
        charCount = w/repeatCharWidth;
        if (charCount >= sizeof(buf))
            charCount = sizeof(buf)-1;
        memset(buf, repeatChar, charCount);
        buf[charCount] = 0;
        mgTextCentered(mg, x[0], yOff, w, height, MG_WHITE, font, buf);
        }
    ++block;
    }
}


static UBYTE leftGood(struct ffAli *ali)
{
DNA *n = ali->nStart;
DNA *h = ali->hStart;
int size = ali->nEnd - ali->nStart;
int matchSize = 0;

while (--size >= 0)
    {
    if (*n++ != *h++)
        break;
    ++matchSize;
    }
if (matchSize > 255)
    matchSize = 255;
return (UBYTE)matchSize;
}

static UBYTE rightGood(struct ffAli *ali)
{
DNA *n = ali->nEnd;
DNA *h = ali->hEnd;
int size = ali->nEnd - ali->nStart;
int matchSize = 0;

while (--size >= 0)
    {
    if (*--n != *--h)
        break;
    ++matchSize;
    }
if (matchSize > 255)
    matchSize = 255;
return (UBYTE)matchSize;
}


static int countAlis(struct ffAli *ali)
{
int count = 0;
while (ali != NULL)
    {
    ++count;
    ali = ali->right;
    }
return count;
}

struct cdaAli *cdaAliFromFfAli(struct ffAli *aliList, 
    DNA *needle, int needleSize, DNA *hay, int haySize, boolean isRc)
/* Convert from ffAli to cdaAli format. */
{
int count = countAlis(aliList);
struct cdaAli *cda;
struct cdaBlock *blocks;
struct ffAli *fa;
int score;
int bases;

if (isRc)
    reverseComplement(needle, needleSize);
AllocVar(cda);
cda->baseCount = needleSize;
cda->strand = (isRc ? '-' : '+');
cda->direction = '+';   /* Actually we don't know. */
cda->orientation = (isRc ? -1 : 1);
cda->blockCount = count;
cda->blocks = blocks = needMem(count * sizeof(blocks[0]));

for (fa = aliList; fa != NULL; fa = fa->right)
    {
    blocks->nStart = fa->nStart - needle;
    blocks->nEnd = fa->nEnd - needle;
    blocks->hStart = fa->hStart - hay;
    blocks->hEnd  = fa->hEnd - hay;
    blocks->startGood = leftGood(fa);
    blocks->endGood = rightGood(fa);
    bases = fa->nEnd - fa->nStart;
    score = dnaScoreMatch(fa->nStart, fa->hStart, bases);
    blocks->midScore = roundingScale(255, score, bases);
    ++blocks;
    }
cdaFixChromStartEnd(cda);
if (isRc)
    {
    reverseComplement(needle, needleSize);
    }
return cda;
}

