/* seqOut - stuff to output sequences and alignments in web
 * or ascii viewable form.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "obscure.h"
#include "dnautil.h"
#include "fuzzyFind.h"
#include "seqOut.h"
#include "htmshell.h"
#include "axt.h"


struct cfm *cfmNew(int wordLen, int lineLen,
	boolean lineNumbers, boolean countDown, FILE *out, int numOff)
/* Set up colored sequence formatting for html. */
{
struct cfm *cfm;
AllocVar(cfm);
cfm->inWord = cfm->inLine = cfm->charCount = 0;
cfm->color = 0;
cfm->wordLen = wordLen;
cfm->lineLen = lineLen;
cfm->lineNumbers = lineNumbers;
cfm->countDown = countDown;
cfm->out = out;
cfm->numOff = numOff;
cfm->bold = cfm->underline = cfm->italic = FALSE;
return cfm;
}

static void cfmPopFormat(struct cfm *cfm)
/* Restore format to default. */
{
if (cfm->color != 0)
    fprintf(cfm->out, "</span>");
if (cfm->underline)
    fprintf(cfm->out, "</span>");
if (cfm->bold)
  fprintf(cfm->out, "</B>");
if (cfm->italic)
  fprintf(cfm->out, "</I>");
}

static void cfmPushFormat(struct cfm *cfm)
/* Set format. */
{
if (cfm->italic)
  fprintf(cfm->out, "<I>");
if (cfm->bold)
  fprintf(cfm->out, "<B>");
if (cfm->underline)
    fprintf(cfm->out, "<span style='text-decoration:underline;text-decoration-skip-ink:none'>");
if (cfm->color != 0)
    fprintf(cfm->out, "<span style='color:#%06X;'>", cfm->color);
}

void cfmOutExt(struct cfm *cfm, char c, int color, boolean underline, boolean bold, boolean italic)
/* Write out a byte, and formatting extras  */
{
if (color != cfm->color || underline != cfm->underline
   || bold != cfm->bold || italic != cfm->italic)
   {
   cfmPopFormat(cfm);
   cfm->color = color;
   cfm->underline = underline;
   cfm->bold = bold;
   cfm->italic = italic;
   cfmPushFormat(cfm);
   }

++cfm->charCount;
fputc(c, cfm->out);
if (cfm->wordLen)
    {
    if (++cfm->inWord >= cfm->wordLen)
	{
	cfmPopFormat(cfm);
	fputc(' ', cfm->out);
	cfmPushFormat(cfm);
	cfm->inWord = 0;
	}
    }
if (cfm->lineLen)
    {
    if (++cfm->inLine >= cfm->lineLen)
	{
	if (cfm->lineNumbers)
	    {
	    int pos = cfm->charCount;
	    if (cfm->countDown)
		{
	        pos = 1-pos;
		}
	    pos += cfm->numOff;
	    cfmPopFormat(cfm);
	    fprintf(cfm->out, " %d", pos);
	    cfmPushFormat(cfm);
	    }
	fprintf(cfm->out, "\n");
	cfm->inLine = 0;
	}
    }
}

void cfmOut(struct cfm *cfm, char c, int color)
/* Write out a byte, and depending on color formatting extras  */
{
cfmOutExt(cfm, c, color, FALSE, FALSE, FALSE);
}

void cfmFree(struct cfm **pCfm)
/* Finish up cfm formatting job. */
{
struct cfm *cfm = *pCfm;
if (cfm != NULL)
    {
    cfmPopFormat(cfm);
    freez(pCfm);
    }
}

int seqOutColorLookup[] =
    {
    0x000000,
    0x3300FF,
    0x22CCEE,
    0xFF0033,
    0xFFcc22,
    0x00aa00,
    0xFF0000,
    };


/* State for bafLabelBlocks().  It lives here rather than in struct baf so that the struct
 * keeps its layout for objects compiled against the older header. */
static boolean labelBlocks = FALSE;		/* Set by bafLabelBlocks(). */
static char *genomeChromLabel = NULL;		/* Set by bafLabelBlocks(). */
static char *genomeLinkPrefix = NULL;		/* Set by bafLabelBlocks(). */
static struct bafBlock *blocksDrawn = NULL;	/* Blocks drawn while labelling, last first. */
static struct bafBlock *lastBlock = NULL;	/* Last block of the current alignment. */
static struct baf *openBaf = NULL;		/* Formatter whose out buffers an open block. */
static FILE *pageOut = NULL;			/* Where the page goes while a block is open. */
static char *blockBuf = NULL;			/* Memory stream behind openBaf->out. */
static size_t blockBufSize = 0;

void bafInit(struct baf *baf, DNA *needle, int nNumOff,  boolean nCountDown,
        DNA *haystack, int hNumOff, boolean hCountDown, FILE *out,
        int lineSize, boolean isTrans )
/* Initialize block alignment formatter. */
{
baf->cix = 0;
baf->needle = needle;
baf->nCountDown = nCountDown;
baf->haystack = haystack;
baf->nNumOff = nNumOff;
baf->hNumOff = hNumOff;
baf->hCountDown = hCountDown;
baf->out = out;
baf->lineSize = lineSize;
baf->isTrans = isTrans;
baf->nCurPos = baf->hCurPos = 0;
baf->nLineStart = baf->hLineStart = 0;
lastBlock = NULL;	/* a new alignment: its first block has no block before it */
}

static boolean bafInBlock(struct baf *baf)
/* True if baf is writing a labelled block into the buffer. */
{
return (baf == openBaf);
}

void bafLabelBlocks(boolean on, char *genomeUrl, char *genomeChrom)
/* While on, side-by-side alignments start each block with a line giving its number, its
 * position and its identity, put a line saying how big the gap is between two blocks where
 * there would be an <HR>, color mismatched bases and merged gaps, and keep a list of the
 * blocks for bafBlocksDrawn().  Each block gets an anchor named sbs<num>.  If genomeUrl is
 * not NULL, the genome position in the header links to genomeUrl followed by "start-end";
 * it goes into an href as it is, so it must already be encoded.  genomeChrom, if not NULL, is
 * shown before the genome position and must already be HTML-safe.  Off by default. */
{
labelBlocks = on;
freez(&genomeLinkPrefix);
freez(&genomeChromLabel);
if (on)
    {
    slFreeList(&blocksDrawn);
    genomeLinkPrefix = cloneString(genomeUrl);
    genomeChromLabel = cloneString(genomeChrom);
    }
}

struct bafBlock *bafBlocksDrawn()
/* Return the blocks drawn since bafLabelBlocks() was turned on and forget them.  Free with
 * slFreeList. */
{
struct bafBlock *list = blocksDrawn;
blocksDrawn = NULL;
slReverse(&list);
return list;
}

static int bafNeedlePos(struct baf *baf, int pos)
/* A translated alignment counts needle positions in bases, three per amino acid.  Labelled
 * blocks number a protein needle in amino acids, so the numbers match the protein. */
{
return (baf->isTrans && labelBlocks) ? pos / 3 : pos;
}

static int bafStartNum(int numOff, boolean countDown, int pos)
/* The number bafWriteLine prints at the start of a line that begins at pos. */
{
return countDown ? numOff - pos : 1 + numOff + pos;
}

static int bafEndNum(int numOff, boolean countDown, int pos)
/* The number bafWriteLine prints at the end of a line that stops before pos. */
{
return countDown ? 1 + numOff - pos : numOff + pos;
}

static int bafGap(int lastEnd, int nextStart, boolean countDown)
/* Number of positions between two blocks, negative if they overlap. */
{
return countDown ? lastEnd - nextStart - 1 : nextStart - lastEnd - 1;
}

static void bafOpenBlock(struct baf *baf)
/* Start a labelled block: write the gap line after the block before, then buffer the
 * block's lines until bafCloseBlock knows enough to write its header. */
{
struct bafBlock *prev = lastBlock, *block;
AllocVar(block);
block->num = (prev == NULL ? 1 : prev->num + 1);
block->nStart = bafStartNum(baf->nNumOff, baf->nCountDown, bafNeedlePos(baf, baf->nCurPos));
block->hStart = bafStartNum(baf->hNumOff, baf->hCountDown, baf->hCurPos);
if (prev != NULL)
    {
    prev->nGapAfter = bafGap(prev->nEnd, block->nStart, baf->nCountDown);
    prev->hGapAfter = bafGap(prev->hEnd, block->hStart, baf->hCountDown);
    if (prev->nGapAfter < 0 || prev->hGapAfter < 0)
        fprintf(baf->out, "<span class='bafSep'>Blocks %d and %d overlap</span>",
                prev->num, block->num);
    else
        {
        char hGap[32], nGap[32];
        sprintLongWithCommas(hGap, prev->hGapAfter);
        sprintLongWithCommas(nGap, prev->nGapAfter);
        fprintf(baf->out, "<span class='bafSep'>Gap: %s bases in genome, %s in query</span>",
                hGap, nGap);
        }
    }
slAddHead(&blocksDrawn, block);
lastBlock = block;
pageOut = baf->out;
baf->out = open_memstream(&blockBuf, &blockBufSize);
if (baf->out == NULL)
    errnoAbort("bafOpenBlock: open_memstream failed");
openBaf = baf;
}

static void bafCloseBlock(struct baf *baf)
/* Finish a labelled block: write its anchor and header line, then its buffered lines. */
{
struct bafBlock *block = lastBlock;
block->nEnd = bafEndNum(baf->nNumOff, baf->nCountDown, bafNeedlePos(baf, baf->nCurPos));
block->hEnd = bafEndNum(baf->hNumOff, baf->hCountDown, baf->hCurPos);
fclose(baf->out);
baf->out = pageOut;
openBaf = NULL;

char nStart[32], nEnd[32], hStart[32], hEnd[32];
sprintLongWithCommas(nStart, block->nStart);
sprintLongWithCommas(nEnd, block->nEnd);
sprintLongWithCommas(hStart, block->hStart);
sprintLongWithCommas(hEnd, block->hEnd);
int aligned = block->matches + block->mismatches;
char chromPrefix[512] = "";
if (genomeChromLabel != NULL)
    safef(chromPrefix, sizeof chromPrefix, "%s:", genomeChromLabel);
fprintf(baf->out, "<a name=sbs%d></a><span class='bafHead'>Block %d: query %s-%s, genome ",
        block->num, block->num, nStart, nEnd);
if (genomeLinkPrefix != NULL)
    fprintf(baf->out, "<a href='%s%d-%d'>%s%s-%s</a>", genomeLinkPrefix,
            min(block->hStart, block->hEnd), max(block->hStart, block->hEnd), chromPrefix,
            hStart, hEnd);
else
    fprintf(baf->out, "%s%s-%s", chromPrefix, hStart, hEnd);
fprintf(baf->out, ", %.1f%% identity", (aligned > 0 ? 100.0 * block->matches / aligned : 0.0));
if (block->mismatches > 0)
    fprintf(baf->out, ", %d mismatch%s", block->mismatches, (block->mismatches == 1 ? "" : "es"));
if (block->gapCols > 0)
    fprintf(baf->out, ", %d gap position%s", block->gapCols, (block->gapCols == 1 ? "" : "s"));
fprintf(baf->out, "</span>\n");
fwrite(blockBuf, 1, blockBufSize, baf->out);
free(blockBuf);	/* libc free: open_memstream's buffer is malloc'd, not needMem'd */
blockBuf = NULL;
blockBufSize = 0;
}

static boolean bafIsGap(char c)
/* True for the characters the alignment formatters write into a gap. */
{
return (c == '-' || c == '.');
}

static void bafSeqChar(struct baf *baf, char c, char other)
/* Write one letter of a needle or haystack line.  In a labelled block, mark gaps and
 * mismatches against the other sequence's letter in the same column. */
{
if (!bafInBlock(baf) || baf->isTrans)
    fputc(c, baf->out);
else if (bafIsGap(c) || bafIsGap(other))
    fprintf(baf->out, "<span class='bafGap'>%c</span>", (c == '.' ? '-' : c));
else if (toupper(c) != toupper(other))
    fprintf(baf->out, "<span class='bafMis'>%c</span>", c);
else
    fputc(c, baf->out);
}

void bafSetAli(struct baf *baf, struct ffAli *ali)
/* Set up block formatter around an ffAli block. */
{
baf->nCurPos = ali->nStart - baf->needle;
baf->hCurPos = ali->hStart - baf->haystack;
}

void bafSetPos(struct baf *baf, int nStart, int hStart)
/* Set up block formatter starting at nStart/hStart. */
{
if (baf->isTrans)
    nStart *= 3;
baf->nCurPos = nStart;
baf->hCurPos = hStart;
}

void bafStartLine(struct baf *baf)
/* Set up block formatter to start new line at current position. */
{
baf->nLineStart = baf->nCurPos;
baf->hLineStart = baf->hCurPos;
if (labelBlocks && !bafInBlock(baf))	/* the alignment formatters start each block with this */
    bafOpenBlock(baf);
}

static int maxDigits(int x, int y)
{
int xDigits = digitsBaseTen(x);
int yDigits = digitsBaseTen(y);
return (xDigits > yDigits ? xDigits : yDigits);
}

void bafWriteLine(struct baf *baf)
/* Write out a line of an alignment (which takes up
 * three lines on the screen. */
{
int i;
int count = baf->cix;
int nStart = baf->nLineStart + 1 + baf->nNumOff;
int hStart = baf->hLineStart + 1 + baf->hNumOff;
int nEnd = baf->nCurPos + baf->nNumOff;
int hEnd = baf->hCurPos + baf->hNumOff;
int startDigits = maxDigits(nStart, hStart);
int endDigits = maxDigits(nEnd, hEnd);
int hStartNum, hEndNum;
int nStartNum, nEndNum;
static struct axtScoreScheme *ss = 0;  /* Scoring scheme. */
struct cfm cfm;
extern char blosumText[];
extern struct axtScoreScheme *axtScoreSchemeFromProteinText(char *text, char *fileName);
boolean revArrows = (baf->nCountDown ^ baf->hCountDown);
char arrowChar = (revArrows ? '<' : '>');

ZeroVar(&cfm);
cfm.out = baf->out;
if (ss == 0)
    ss = axtScoreSchemeFromProteinText(blosumText, "fake");

nStartNum = bafStartNum(baf->nNumOff, baf->nCountDown, bafNeedlePos(baf, baf->nLineStart));
nEndNum = bafEndNum(baf->nNumOff, baf->nCountDown, bafNeedlePos(baf, baf->nCurPos));
fprintf(baf->out, "%0*d ", startDigits, nStartNum);
for (i=0; i<count; ++i)
    bafSeqChar(baf, baf->nChars[i], baf->hChars[i]);
fprintf(baf->out, " %0*d\n", endDigits, nEndNum);

for (i=0; i<startDigits; ++i)
    fputc(arrowChar, baf->out);
fputc(' ', baf->out);
for (i=0; i<count; ++i)
    {
    char n,h,c =  ' ';

    n = baf->nChars[i];
    h = baf->hChars[i];
    if (baf->isTrans)
        {
	if (n != ' ')
	    {
	    DNA codon[4];
	    codon[0] = baf->hChars[i-1];
	    codon[1] = h;
	    codon[2] = baf->hChars[i+1];
	    codon[3] = 0;
	    tolowers(codon);
	    c  = lookupCodon(codon);
	    cfmPushFormat(&cfm);
	    if (bafInBlock(baf))
		{
		if (toupper(n) == c)
		    lastBlock->matches++;
		else
		    lastBlock->mismatches++;
		}
	    if (toupper(n) == c)
		cfmOut(&cfm, '|', seqOutColorLookup[0]);
            else
                {
                int color;

                if (c == 0)
                    c = 'X';
                if (ss->matrix[(int)toupper(n)][(int)c] > 0)
		    color = 5;
		else
		    color = 6;
		cfmOut(&cfm, c, seqOutColorLookup[color]);
		}
	    cfmPopFormat(&cfm);
	    }
	else
	    {
	    fputc(c, baf->out);
	    }
	}
    else
        {
	if (toupper(n) == toupper(h))
	     c = '|';
	fputc(c, baf->out);
	if (bafInBlock(baf))
	    {
	    if (bafIsGap(n) || bafIsGap(h))
		lastBlock->gapCols++;
	    else if (c == '|')
		lastBlock->matches++;
	    else
		lastBlock->mismatches++;
	    }
	}
    }
fputc(' ', baf->out);
for (i=0; i<endDigits; ++i)
    fputc(arrowChar, baf->out);
fprintf(baf->out, "\n");

if (baf->hCountDown)
    {
    hStartNum = baf->hNumOff - baf->hLineStart;
    hEndNum = 1+baf->hNumOff - baf->hCurPos;
    }
else
    {
    hStartNum = 1+baf->hNumOff + baf->hLineStart;
    hEndNum = baf->hNumOff + baf->hCurPos;
    }
fprintf(baf->out, "%0*d ", startDigits, hStartNum);
for (i=0; i<count; ++i)
    bafSeqChar(baf, baf->hChars[i], baf->nChars[i]);
fprintf(baf->out, " %0*d\n\n", endDigits, hEndNum);
}

void bafOut(struct baf *baf, char n, char h)
/* Write a pair of character to block alignment. */
{
baf->nChars[baf->cix] = n;
baf->hChars[baf->cix] = h;
if (n != '.' && n != '-')
    baf->nCurPos += 1;
if (h != '.' && h != '-')
    baf->hCurPos += 1;
if (++(baf->cix) >= baf->lineSize)
    {
    bafWriteLine(baf);
    baf->cix = 0;
    baf->nLineStart = baf->nCurPos;
    baf->hLineStart = baf->hCurPos;
    }
}

void bafFlushLineNoHr(struct baf *baf)
/* Write out alignment line if it has any characters in it (no <HR>). */
{
if (baf->cix > 0)
    bafWriteLine(baf);
fflush(baf->out);
baf->cix = 0;
if (bafInBlock(baf))	/* the alignment formatters end each block with this */
    bafCloseBlock(baf);
}

void bafFlushLine(struct baf *baf)
/* Write out alignment line if it has any characters in it, and an <HR>.  A labelled block
 * gets no <HR>: the gap line written when the next block starts separates them. */
{
boolean labelled = bafInBlock(baf);
bafFlushLineNoHr(baf);
if (!labelled)
    fprintf(baf->out, "<HR ALIGN=\"CENTER\">");
}

