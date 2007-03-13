/* orfEnum - Enumerate all orfs in a cDNA.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "fa.h"

static char const rcsid[] = "$Id: orfEnum.c,v 1.1 2007/03/13 17:09:24 kent Exp $";

int minSize=24;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "orfEnum - Enumerate all orfs in a cDNA.\n"
  "usage:\n"
  "   orfEnum in.fa out.orf\n"
  "options:\n"
  "   -minSize=N - minimum size to output (in bases).  Default %d\n"
  , minSize
  );
}

static struct optionSpec options[] = {
   {"minSize", OPTION_INT},
   {NULL, 0},
};


void outputIfBigEnough(char *name, int start, int stop, FILE *f)
/* Output if orf is big enough. */
{
if (stop-start >= minSize)
    fprintf(f, "%s\t%d\t%d\n", name, start, stop);
}

void saveOrf(char *name, DNA *dna, int start, int end, FILE *f)
/* Save ORF starting at start, and extending to stop codon
 * or end, whatever comes first. */
{
int stop;
int lastPos = end-3;
for (stop = start; stop<=lastPos; stop += 3)
    {
    if (isStopCodon(dna+stop))
         {
	 outputIfBigEnough(name, start, stop+3, f);
	 return;
	 }
    }
int aaSize = (end-start)/3;
outputIfBigEnough(name, start, start + 3*aaSize, f);
}

void orfInFrame(char *name, DNA *dna, int start, int end, FILE *f)
/* Output all ORFs in this frame. */
{
int endPos = end - 3;
/* Since we may be a fragment, we always begin with an ORF. */
saveOrf(name, dna, start, end, f);
int pos;
for (pos = start+3; pos <= endPos; pos += 3)
    {
    if (startsWith("atg", dna+pos))
        saveOrf(name, dna, pos, end, f);
    }
}

void orfEnum(char *inFa, char *outOrf)
/* orfEnum - Enumerate all orfs in a cDNA. */
{
struct lineFile *lf = lineFileOpen(inFa, TRUE);
FILE *f = mustOpen(outOrf, "w");
DNA *dna;
char *name;
int size;
while (faSpeedReadNext(lf, &dna, &size, &name))
    {
    orfInFrame(name, dna, 0, size, f);
    orfInFrame(name, dna, 1, size, f);
    orfInFrame(name, dna, 2, size, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
minSize = optionInt("minSize", minSize);
dnaUtilOpen();
orfEnum(argv[1], argv[2]);
return 0;
}
