/* axtSwap - Swap source and query in an axt file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "axt.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtSwap - Swap source and query in an axt file\n"
  "usage:\n"
  "   axtSwap source.axt target.sizes query.sizes dest.axt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct hash *loadIntHash(char *fileName)
/* Load up file with lines of name<space>size into hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
struct hash *hash = hashNew(0);

while (lineFileRow(lf, row))
    hashAddInt(hash, row[0], lineFileNeedNum(lf, row, 1));
return hash;
}


void axtSwap(struct axt *axt, int tSize, int qSize)
/* Flip target and query on one axt. */
{
struct axt old = *axt;

/* Copy non-strand dependent stuff */
axt->qName = old.tName;
axt->tName = old.qName;
axt->qSym = old.tSym;
axt->tSym = old.qSym;
axt->qStart = old.tStart;
axt->qEnd = old.tEnd;
axt->tStart = old.qStart;
axt->tEnd = old.qEnd;

/* Copy strand dependent stuff. */
assert(axt->tStrand != '-');

if (axt->qStrand == '-')
    {
    /* axt's are really set up so that the target is on the
     * + strand and the query is on the minus strand.
     * Therefore we need to reverse complement both 
     * strands while swapping to preserve this. */
    reverseIntRange(&axt->tStart, &axt->tEnd, qSize);
    reverseIntRange(&axt->qStart, &axt->qEnd, tSize);
    reverseComplement(axt->qSym, axt->symCount);
    reverseComplement(axt->tSym, axt->symCount);
    }
}

void axtSwapFile(char *source, char *targetSizes, char *querySizes, char *dest)
/* axtSwapFile - Swap source and query in an axt file. */
{
struct hash *tHash = loadIntHash(targetSizes);
struct hash *qHash = loadIntHash(querySizes);
struct lineFile *lf = lineFileOpen(source, TRUE);
FILE *f = mustOpen(dest, "w");
struct axt *axt;

while ((axt = axtRead(lf)) != NULL)
    {
    axtSwap(axt, hashIntVal(tHash, axt->tName), hashIntVal(qHash, axt->qName));
    axtWrite(axt, f);
    axtFree(&axt);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
axtSwapFile(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
