/* faTrimPolyA - trim Poly-A tail*/
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"

static char const rcsid[] = "$Id: faTrimPolyA.c,v 1.1 2003/07/20 18:33:22 baertsch Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faTrimPolyA - trim Poly A tail\n"
  "usage:\n"
  "   faTrimPolyA in.fa out.fa \n"
  "options:\n"
  );
}

void faTrimPolyA(char *inFile, char *outFile)
/* faTrimPolyA - trim Poly-A tail*/
{
struct dnaSeq seq;
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
int seqCount = 0, passCount = 0;
int nCount;
ZeroVar(&seq);

while (faSomeSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name, FALSE))
    {
    int i, aSize = 0;
    seqCount += 1;
    for (i=seq.size-1; i>=0; --i)
        {
        DNA b = seq.dna[i];
        if (b == 'a' || b == 'A')
            ++aSize;
        else
            break;
        }
    if (aSize >= 4)
        {
        memset(seq.dna + seq.size - aSize, 'n', aSize);
        seq.size -= aSize;
        seq.dna[seq.size-aSize] = 0;
        }
    faWriteNext(f, seq.name, seq.dna, seq.size);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3 )
    usage();
faTrimPolyA(argv[1], argv[2]);
return 0;
}
