/* used for cleaning up self alignments - deletes all overlapping self alignments */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "axt.h"
#include "obscure.h"
#include "hash.h"
#include "dnautil.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
"axtDropOverlap - deletes all overlapping self alignments. \n"
"usage:\n"
"    axtDropOverlap in.axt tSizes qSizes out.axt\n"
"Where tSizes and qSizes are tab-delimited files with \n"
"       <seqName><size>\n"
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
    if (hashLookup(hash, name) != NULL)
        warn("Duplicate %s, ignoring all but first\n", name);
    else
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

void axtDropOverlap(char *inName, char *tSizeFile, char *qSizeFile, char *outName)
/* used for cleaning up self alignments - deletes all overlapping self alignments */
{
struct hash *qSizeHash = readSizes(qSizeFile);
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct axt *axt;
int totMatch = 0;
int totSkip = 0;
int totLines = 0;

while ((axt = axtRead(lf)) != NULL)
    {
    totLines++;
    totMatch += axt->score;
	if (sameString(axt->qName, axt->tName))
        {
        int qs = axt->qStart;
        int qe = axt->qEnd;
        if (axt->qStrand == '-')
            reverseIntRange(&qs, &qe, findSize(qSizeHash, axt->qName));
        if (axt->tStart == qs && axt->tEnd == qe) 
            {
            /*
            printf( "skip %c\t%s\t%d\t%d\t%d\t%s\t%d\t%d\t%d\n",
              axt->qStrand,
              axt->qName, axt->symCount, axt->qStart, axt->qEnd,
              axt->tName, axt->symCount, axt->tStart, axt->tEnd
              );
              */
            totSkip++;
            continue;
            }
        }
    axtWrite(axt, f);

    axtFree(&axt);
    }
fclose(f);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 5)
    usage();
axtDropOverlap(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
