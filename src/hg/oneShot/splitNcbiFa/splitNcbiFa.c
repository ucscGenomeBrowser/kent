/* splitNcbiFa - Split up NCBI format fa file into UCSC formatted ones.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "splitNcbiFa - Split up NCBI format fa file into UCSC formatted ones.\n"
  "usage:\n"
  "   splitNcbiFa in.fa outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void splitNcbiFa(char *ncbiIn, char *outDir)
/* splitNcbiFa - Split up NCBI format fa file into UCSC formatted ones.. */
{
struct lineFile *lf = lineFileOpen(ncbiIn, TRUE);
static struct dnaSeq seq;
ZeroVar(&seq);

makeDir(outDir);
while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    FILE *f;
    char fileName[512];
    char *row[5];
    int wordCount;
    char ourName[129];
    char cloneName[128];

    wordCount = chopByChar(seq.name, '|', row, ArraySize(row));
    if (wordCount != 5)
        errAbort("Expecting 5 | separated fields line %d of %s", lf->lineIx, lf->fileName);
    strcpy(cloneName, row[3]);
    chopSuffix(cloneName);
    sprintf(fileName, "%s/%s.fa", outDir, cloneName);
    sprintf(ourName, "%s_1", row[3]);
    faWrite(fileName, ourName, seq.dna, seq.size);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
splitNcbiFa(argv[1],argv[2]);
return 0;
}
