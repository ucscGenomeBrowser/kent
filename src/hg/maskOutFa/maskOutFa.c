/* maskOutFa - Produce a masked .fa file given an unmasked .fa and a RepeatMasker .out file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "fa.h"
#include "repMask.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "maskOutFa - Produce a masked .fa file given an unmasked .fa and a RepeatMasker .out file\n"
  "usage:\n"
  "   maskOutFa in.fa in.fa.out out.fa.masked\n");
}

void maskOutFa(char *inFa, char *maskFile, char *outFa)
/* maskOutFa - Produce a masked .fa file given an unmasked .fa and a RepeatMasker .out file. */
{
struct lineFile *lf;
struct hash *hash = newHash(0);
struct dnaSeq *seqList = NULL, *seq;
char *line;
int lineSize;
struct repeatMaskOut rmo;
struct hashEl *hel;
boolean ok;
FILE *f;

/* Read DNA and hash sequence names. */
seqList = faReadAllDna(inFa);
for (seq = seqList; seq != NULL; seq = seq->next)
    hashAdd(hash, seq->name, seq);

/* Read RepeatMasker file and set masked sequence areas to N. */
lf = lineFileOpen(maskFile, TRUE);
ok = lineFileNext(lf, &line, &lineSize);
if (!ok)
    errAbort("Empty mask file %s\n", maskFile);
else 
    {
    if (!startsWith("There were no", line))
	{
	if (!startsWith("   SW", line))
	    errAbort("%s isn't a RepeatMasker .out file.", maskFile);
	if (!lineFileNext(lf, &line, &lineSize) || !startsWith("score", line))
	    errAbort("%s isn't a RepeatMasker .out file.", maskFile);
	lineFileNext(lf, &line, &lineSize);  /* Blank line. */
	while (lineFileNext(lf, &line, &lineSize))
	    {
	    char *words[32];
	    int wordCount;
	    int seqSize;
	    int repSize;
	    wordCount = chopLine(line, words);
	    if (wordCount < 14)
		errAbort("%s line %d - error in repeat mask .out file\n", maskFile, lf->lineIx);
	    repeatMaskOutStaticLoad(words, &rmo);
	    if ((seq = hashFindVal(hash, rmo.qName)) == NULL)
		errAbort("%s is in %s but not %s, files out of sync?\n", rmo.qName, maskFile, inFa);
	    seqSize = seq->size;
	    if (rmo.qStart <= 0 || rmo.qStart > seqSize || rmo.qEnd <= 0 || rmo.qEnd > seqSize || rmo.qStart > rmo.qEnd)
		{
		warn("Repeat mask sequence out of range (%d-%d of %d in %s)\n",
		    rmo.qStart, rmo.qEnd, seqSize, rmo.qName);
		if (rmo.qStart <= 0)
		    rmo.qStart = 1;
		if (rmo.qEnd > seqSize)
		    rmo.qEnd = seqSize;
		}
	    repSize = rmo.qEnd - rmo.qStart + 1;
	    if (repSize > 0)
		memset(seq->dna + rmo.qStart - 1, 'N', repSize);
	    }
	}
    }
lineFileClose(&lf);

/* Write out masked file. */
f = mustOpen(outFa, "w");
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    toUpperN(seq->dna, seq->size);
    faWriteNext(f, seq->name, seq->dna, seq->size);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
maskOutFa(argv[1], argv[2], argv[3]);
return 0;
}
