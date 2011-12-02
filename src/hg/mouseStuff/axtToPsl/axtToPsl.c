/* axtToPsl - Convert axt to psl format. 
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "axt.h"
#include "psl.h"
#include "dnautil.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtToPsl - Convert axt to psl format\n"
  "usage:\n"
  "   axtToPsl in.axt tSizes qSizes out.psl\n"
  "Where tSizes and qSizes are tab-delimited files with\n"
  "       <seqName><size>\n"
  "columns.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct hash *readSizes(char *fileName)
/* Read tab-separated file into hash with
 * name key size value. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);
char *row[2];
while (lineFileRow(lf, row))
    {
    char *name = row[0];
    int size = lineFileNeedNum(lf, row, 1);
    
    /* trust the user to not have duplicated names in the lengths file */
    hashAdd(hash, name, intToPt(size));
    }
lineFileClose(&lf);
return hash;
}

int findSize(struct hash *hash, char *name)
/* Find size of name in hash or die trying. */
{
void *val = hashMustFindVal(hash, name);
return ptToInt(val);
}

void axtToPsl(char *inName, char *tSizeFile, char *qSizeFile, char *outName)
/* axtToPsl - Convert axt to psl format. */
{
struct hash *tSizeHash = readSizes(tSizeFile);
struct hash *qSizeHash = readSizes(qSizeFile);
struct lineFile *lf = lineFileOpen(inName, TRUE);
char strand[2];
FILE *f = mustOpen(outName, "w");
struct psl* psl;
struct axt *axt;
strand[1] = '\0';

while ((axt = axtRead(lf)) != NULL)
    {
    int qSize = findSize(qSizeHash, axt->qName);
    int qStart =  axt->qStart;
    int qEnd = axt->qEnd;
    if (axt->qStrand == '-')
        reverseIntRange(&qStart, &qEnd, qSize);
    strand[0] = axt->qStrand;
    psl = pslFromAlign(axt->qName, qSize, qStart, qEnd, axt->qSym, 
                       axt->tName, findSize(tSizeHash, axt->tName),
                       axt->tStart, axt->tEnd, axt->tSym, strand,
                       PSL_IS_SOFTMASK);
    if (psl != NULL)
	{
	pslTabOut(psl, f);
	pslFree(&psl);
	}
    axtFree(&axt);
    }
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
axtToPsl(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
