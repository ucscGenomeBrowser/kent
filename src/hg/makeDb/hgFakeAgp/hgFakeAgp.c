/* hgFakeAgp - Create fake AGP file by looking at N's. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "fa.h"


static int minContigGap = 25;
static int minScaffoldGap = 50000;
static boolean singleContigs = FALSE;

static void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgFakeAgp - Create fake AGP file by looking at N's\n"
  "usage:\n"
  "   hgFakeAgp input.fa output.agp\n"
  "options:\n"
  "   -minContigGap=N Minimum size for a gap between contigs.  Default %d\n"
  "   -minScaffoldGap=N Min size for a gap between scaffolds. Default %d\n"
  "   -singleContigs - when a full sequence has no gaps, maintain contig\n"
  "\tname without adding index extension.\n"
  , minContigGap, minScaffoldGap
  );
}

static struct optionSpec options[] = {
   {"minContigGap", OPTION_INT},
   {"minScaffoldGap", OPTION_INT},
   {"singleContigs", OPTION_BOOLEAN},
   {NULL, 0},
};

static void agpContigLine(FILE *f, char *name, int seqStart, int seqEnd,
    int lineIx, int contigIx, boolean noIx)
/* Output AGP line for contig. */
{
fprintf(f, "%s\t", name);
fprintf(f, "%d\t", seqStart + 1);
fprintf(f, "%d\t", seqEnd);
fprintf(f, "%d\t", lineIx);
fprintf(f, "%c\t", 'D');
if (noIx)
    fprintf(f, "%s\t", name);
else
    fprintf(f, "%s_%d\t", name, contigIx);
fprintf(f, "1\t");
fprintf(f, "%d\t", seqEnd - seqStart);
fprintf(f, "+\n");
}

static char *strNotChar(char *s, char c)
/* Return pointer to first character in s that is *not* c, or NULL
 * if none such. */
{
char x;
for (;;)
   {
   x = *s;
   if (x == 0)
       return NULL;
   if (x != c)
       return s;
   ++s;
   }
}

static void agpGapLine(FILE *f, char *name, int seqStart, int seqEnd, int gapSize, int lineIx)
/* Write out agp line for gap. */
{
fprintf(f, "%s\t", name);
fprintf(f, "%d\t", seqStart + 1);
fprintf(f, "%d\t", seqEnd);
fprintf(f, "%d\t", lineIx);
fprintf(f, "%c\t", 'N');
fprintf(f, "%d\t", gapSize);
if (gapSize >= minScaffoldGap)
    fprintf(f, "scaffold\tno\n");
else
    fprintf(f, "contig\tyes\n");
}

static void fakeAgpFromSeq(struct dnaSeq *seq, FILE *f)
/* Look through sequence and produce agp file. */
{
static int lineIx = 0;
int contigIx = 0;
char *gapStart, *gapEnd, *contigStart;
char *dna, *seqStart, *seqEnd;
int gapSize;
boolean maintainNames = singleContigs;

dna = seqStart = contigStart = seq->dna;
seqEnd = seqStart + seq->size;
for (;;)
   {
   gapStart = strchr(dna, 'n');
   if (gapStart == NULL)
       {
       agpContigLine(f, seq->name, contigStart - seqStart, seqEnd - seqStart, 
		++lineIx, ++contigIx, maintainNames);
       break;
       }
   maintainNames = FALSE;
   gapEnd = strNotChar(gapStart, 'n');
   if (gapEnd == NULL)
       gapEnd = seqEnd;
   gapSize = gapEnd - gapStart;
   if (gapSize >= minContigGap || gapEnd == seqEnd)
       {
       if (gapStart != contigStart)
           agpContigLine(f, seq->name, contigStart - seqStart,
		gapStart - seqStart, ++lineIx, ++contigIx, FALSE);
       agpGapLine(f, seq->name, gapStart - seqStart, gapEnd - seqStart, gapSize, ++lineIx);
       if (gapEnd == seqEnd)
           break;
       contigStart = gapEnd;
       }
   dna = gapEnd;
   }
}

void hgFakeAgp(char *faIn, char *agpOut)
/* hgFakeAgp - Create fake AGP file by looking at N's. */
{
struct lineFile *lf = lineFileOpen(faIn, TRUE);
FILE *f = mustOpen(agpOut, "w");
struct dnaSeq seq;

while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    fakeAgpFromSeq(&seq, f);
    }

carefulClose(&f);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
minContigGap = optionInt("minContigGap", minContigGap);
minScaffoldGap = optionInt("minScaffoldGap", minScaffoldGap);
singleContigs = optionExists("singleContigs");
hgFakeAgp(argv[1], argv[2]);
return 0;
}
