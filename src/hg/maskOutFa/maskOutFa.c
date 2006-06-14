/* maskOutFa - Produce a masked .fa file given an unmasked .fa and 
 * a RepeatMasker .out file or a bed file to mask on. */
#include "common.h"
#include "linefile.h"
#include "cheapcgi.h"
#include "hash.h"
#include "fa.h"
#include "bed.h"
#include "repMask.h"

static char const rcsid[] = "$Id: maskOutFa.c,v 1.9 2006/06/14 16:31:30 angie Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "maskOutFa - Produce a masked .fa file given an unmasked .fa and\n"
  "a RepeatMasker .out file, or a .bed file to mask on.\n"
  "usage:\n"
  "   maskOutFa in.fa maskFile out.fa.masked\n"
  "where in.fa and out.fa.masked can be the same file, and\n"
  "maskFile can end in .out (for RepeatMasker) or .bed.\n"
  "MaskFile parameter can also be the word 'hard' in which case \n"
  "lower case letters are converted to N's.\n"
  "options:\n"
  "   -soft - puts masked parts in lower case other in upper.\n"
  "   -softAdd - lower cases masked bits, leaves others unchanged\n"
  "   -clip - clip out of bounds mask records rather than dying.\n");
}

boolean faMixedSpeedReadNext(struct lineFile *lf, DNA **retDna, int *retSize, char **retName);

void maskOutFa(char *inFa, char *maskFile, char *outFa)
/* maskOutFa - Produce a masked .fa file given an unmasked .fa and a RepeatMasker .out file. */
{
struct lineFile *lf;
struct hash *hash = newHash(0);
struct dnaSeq *seqList = NULL, *seq;
char *line;
int lineSize;
boolean ok;
boolean isOut = endsWith(maskFile, ".out");
boolean isBed = endsWith(maskFile, ".bed");
boolean extraHard = sameWord(maskFile, "hard");
FILE *f;
char *words[32];
int wordCount;
boolean clip = cgiBoolean("clip");
boolean soft = cgiBoolean("soft");
boolean softAdd = cgiBoolean("softAdd");

/* Read DNA and hash sequence names. */
seqList = faReadAllMixed(inFa);
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    if (soft)
       toUpperN(seq->dna, seq->size);
    else if (extraHard)
       lowerToN(seq->dna, seq->size);
    hashAdd(hash, seq->name, seq);
    }

if (!extraHard)
    {
    /* Read mask file header if any. */
    lf = lineFileOpen(maskFile, TRUE);
    if (isOut)
	{
	ok = lineFileNext(lf, &line, &lineSize);
	if (!ok)
	    errAbort("Empty mask file %s\n", maskFile);
	if (!startsWith("There were no", line) && !startsWith("there were no", line))
	    {
	    if (!startsWith("   SW", line))
		errAbort("%s isn't a RepeatMasker .out file.", maskFile);
	    lineFileNext(lf, &line, &lineSize);  /* Blank line. */
	    }
	}
    else if (isBed)
	{
	}
    else
	{
	errAbort("Unrecognized file type %s", maskFile);
	}

    /* Read line at a time from mask file and set masked sequence 
     * areas to N. */
    while ((wordCount = lineFileChop(lf, words)) != 0)
	{
	/* Figure out name of sequence and where to set N's. */
	char *seqName = (char *) NULL;
	int start=-1, end=-1;
	int repSize, seqSize;

	if (isOut)
	    {
	    struct repeatMaskOut rmo;
	    repeatMaskOutStaticLoad(words, &rmo);
	    seqName = rmo.qName;
	    start = rmo.qStart - 1;
	    end = rmo.qEnd;
	    }
	else if (isBed)
	    {
	    struct bed bed;
	    bedStaticLoad(words, &bed);
	    seqName = bed.chrom;
	    start = bed.chromStart;
	    end = bed.chromEnd;
	    }

	/* Find sequence and clip bits of mask to fit. */
	seq = hashFindVal(hash, seqName);
	if (seq == NULL)
	    {
	    errAbort("%s is in %s but not %s, files out of sync?\n", 
		    seqName, maskFile, inFa);
	    }
	seqSize = seq->size;
	if (start < 0 || end > seqSize || start > end)
	    {
	    if (!clip)
		errAbort("Mask record out of range line %d of %s\n"
			 "%s size is %d,  range is %d-%d\n",
			 lf->lineIx, lf->fileName, seqName, seqSize, start, end);
	    if (start < 0) start = 0;
	    if (end > seqSize) end = seqSize;
	    if (end < start) end = start;
	    }
	repSize = end - start;
	if (repSize > 0)
	    {
	    if (soft || softAdd)
		toLowerN(seq->dna + start, repSize);
	    else
		memset(seq->dna + start, 'N', repSize);
	    }
	}
    lineFileClose(&lf);
    }

/* Write out masked file. */
f = mustOpen(outFa, "w");
for (seq = seqList; seq != NULL; seq = seq->next)
    faWriteNext(f, seq->name, seq->dna, seq->size);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
maskOutFa(argv[1], argv[2], argv[3]);
return 0;
}
