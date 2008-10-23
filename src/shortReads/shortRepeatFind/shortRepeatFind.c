/* shortRepeatFind - Figure out short pieces of DNA that happen a lot in genome.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "dnaLoad.h"
#include "splix.h"

static char const rcsid[] = "$Id: shortRepeatFind.c,v 1.1 2008/10/23 20:04:20 kent Exp $";

int readSize = 25;
int minCount = 5;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "shortRepeatFind - Figure out short pieces of DNA that happen a lot in genome.\n"
  "usage:\n"
  "   shortRepeatFind input.splix output.srf\n"
  "options:\n"
  "   -readSize=N Size of basic read size(default %d)\n"
  "   -minCount=N Minimum count of repeats to report (default %d)\n"
  , readSize, minCount);
}

static struct optionSpec options[] = {
   {"readSize", OPTION_INT},
   {"minCount", OPTION_INT},
   {NULL, 0},
};

long totalSeqSize(struct dnaSeq *seqList)
/* Return total size of DNA in list. */
{
long total = 0;
struct dnaSeq *seq;
for (seq = seqList; seq != NULL; seq = seq->next)
    total += seq->size;
return total;
}

struct dnaSeq *dnaSeqOtherStrand(struct dnaSeq *seq)
/* Return other strand of seq.  Do dnaSeqFree on this when done. */
{
struct dnaSeq *rc = cloneDnaSeq(seq);
reverseComplement(rc->dna, rc->size);
return rc;
}

void addSuffixes(char **suffixPt, char *s, int size)
/* Add all suffixes to array starting at suffixPt. */
{
while (--size >= 0)
    *suffixPt++ = s++;
}


int compareStrings(const void *va, const void *vb)
/* Compare two strings. */
{
char **a = (char **)va, **b = (char **)vb;
return cmpDnaStrings(*a,*b);
}

int hexesSame(bits16 *a, int size)
/* Return number of hexes same as the first one. */
{
bits16 first = *a;
int i;
for (i=1; i<size; ++i)
    if (a[i] != first)
        break;
return i;
}

int aftersSame(char **a, int size, int prefixSize)
/* Return number of strings same as the first one in the first prefixSize chars. */
{
char *first = a[0];
int i;
for (i=1; i<size; ++i)
    if (strncmp(a[i], first, prefixSize) != 0)
        break;
return i;
}

static void dumpSixmer(int hexamer, FILE *f)
/* Write out hexamer */
{
fputc(toupper(valToNt[(hexamer>>10)&3]), f);
fputc(toupper(valToNt[(hexamer>>8)&3]), f);
fputc(toupper(valToNt[(hexamer>>6)&3]), f);
fputc(toupper(valToNt[(hexamer>>4)&3]), f);
fputc(toupper(valToNt[(hexamer>>2)&3]), f);
fputc(toupper(valToNt[hexamer&3]), f);
}

static void dumpTwelvemer(int twelvemer, FILE *f)
/* Write out twelvemer as letters to file. */
{
fputc(toupper(valToNt[(twelvemer>>22)&3]), f);
fputc(toupper(valToNt[(twelvemer>>20)&3]), f);
fputc(toupper(valToNt[(twelvemer>>18)&3]), f);
fputc(toupper(valToNt[(twelvemer>>16)&3]), f);
fputc(toupper(valToNt[(twelvemer>>14)&3]), f);
fputc(toupper(valToNt[(twelvemer>>12)&3]), f);
dumpSixmer(twelvemer, f);
}


boolean allDna(char *s, int size)
/* Return TRUE is next size characters in s are all DNA characters. */
{
int i;
for (i=0; i<size; ++i)
    {
    char c = s[i];
    if (ntVal[(int)c] < 0)
        return FALSE;
    }
return TRUE;
}


void shortRepeatFind(char *splixFile, char *outDir)
/* shortRepeatFind - Figure out short pieces of DNA that happen a lot in genome.. */
{
uglyTime(NULL);

/* Load all DNA. */
struct splix *splix = splixRead(splixFile, FALSE);
uglyTime("Loaded %s", splixFile);

char **sortBuf;
int sortBufSize = 1024;
AllocArray(sortBuf, sortBufSize);
int suffixSize = readSize-18;

FILE *f = mustOpen(outDir, "w");
int i;
for (i=0; i<splixSlotCount; ++i)
    {
    bits32 slotSize = splix->slotSizes[i];
    char *slot = splix->slots[i];
    bits16 *hexes = (bits16*)slot;
    bits16 *hexesAfter = hexes+2*slotSize;
    bits32 *offsets = (bits32*)(hexes + 4*slotSize);
    bits32 *offsetsAfter = offsets+2*slotSize;

    int hexIx, hexCount = 0;
    for (hexIx = 0; hexIx < slotSize; hexIx += hexCount)
        {
	hexCount = hexesSame(hexesAfter + hexIx, slotSize - hexIx);
	if (hexCount >= minCount)
	    {
	    if (hexCount > sortBufSize)
	        {
		ExpandArray(sortBuf, sortBufSize, hexCount);
		sortBufSize = hexCount;
		}
	    int j;
	    for (j=0; j<hexCount; ++j)
		sortBuf[j] = splix->allDna + offsetsAfter[hexIx+j] + 12;
	    qsort(sortBuf, hexCount, sizeof(sortBuf[0]), compareStrings);
	    int afterIx, afterCount = 0;
	    for (afterIx = 0; afterIx < hexCount; afterIx += afterCount)
		{
		afterCount = aftersSame(sortBuf+afterIx, hexCount - afterIx, suffixSize);
		if (afterCount >= minCount)
		    {
		    char *suffix = sortBuf[afterIx];
		    if (allDna(suffix, suffixSize))
			{
			dumpTwelvemer(i, f);
			dumpSixmer(hexesAfter[hexIx], f);
			mustWrite(f, sortBuf[afterIx], suffixSize);
			fprintf(f, " %d\n", afterCount);
			}
		    }
		}
	    }
	}
    };

}

int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
optionInit(&argc, argv, options);
readSize = optionInt("readSize", readSize);
minCount = optionInt("minCount", minCount);
if (argc != 3)
    usage();
shortRepeatFind(argv[1], argv[2]);
return 0;
}
