/* gapSplit - split sequence on gaps of size >= N. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fa.h"
#include "dnautil.h"
#include "dystring.h"

#define DEFAULT_GAP	1000

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gapSplit - split sequence on gaps of size >= N\n"
  "usage:\n"
  "   gapSplit -minGap=N <input.fa> <output.fa>\n"
  "options:\n"
  "   -minGap=N - minimum gap, default %d\n", DEFAULT_GAP
  );
}

static struct optionSpec options[] = {
   {"minGap", OPTION_INT},
   {NULL, 0},
};

static int minGap = DEFAULT_GAP;

void gapSplit(char *input, char *output)
/* gapSplit - split sequence on gaps of size N. */
{
struct lineFile *lf = lineFileOpen(input,TRUE);
FILE *f = mustOpen(output, "w");
struct dnaSeq seq;

ZeroVar(&seq);
while (faMixedSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    struct dyString *seqName = dyStringNew(0);
    int pos = 0;
    int pieceCount = 0;
    int charLineCount = 0;

    dyStringPrintf(seqName,"%s_%d", seq.name, pieceCount++);
    fprintf(f,">%s\n", seqName->string);
    while (pos < seq.size)
	{
	int gapSize = 0;
	int gapStart = 0;
	fprintf(f,"%c",seq.dna[pos]);
	++charLineCount;
	if (!(charLineCount % 60)) { fprintf(f,"\n"); charLineCount = 0; }
	++pos;
	gapStart = pos;	/*	remember where possible gap starts	*/
	/* see if gap is entered */
	if ('n' == seq.dna[pos] || 'N' == seq.dna[pos])
	    {
	    /*	enter gap, size it	*/
	    while (pos < seq.size &&
		('n' == seq.dna[pos] || 'N' == seq.dna[pos]))
		{
		++gapSize;
		++pos;
		}
	    }
	/*	valid gap size to split here ?	*/
	if ((gapSize >= minGap) && (pos < seq.size))
	    {
	    if (charLineCount % 60) { fprintf(f,"\n"); charLineCount = 0; }
	    dyStringClear(seqName);
	    dyStringPrintf(seqName,"%s_%d", seq.name, pieceCount++);
	    fprintf(f,">%s\n", seqName->string);
	    }
	else if (gapSize < minGap)
	    {
	    while (gapSize-- > 0)
		{
		fprintf(f,"%c",seq.dna[gapStart++]);
		++charLineCount;
		if (!(charLineCount % 60)) {fprintf(f,"\n"); charLineCount = 0;}
		}
	    }
	}
    if (charLineCount % 60) { fprintf(f,"\n"); }
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();

minGap = optionInt("minGap", DEFAULT_GAP);

verbose(2,"# minGap = %d\n", minGap);
gapSplit(argv[1], argv[2]);
return 0;
}
