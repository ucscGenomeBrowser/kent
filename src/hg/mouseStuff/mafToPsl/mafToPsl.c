/* mafToPsl - Convert maf to psl format. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "maf.h"
#include "psl.h"



/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafToPsl - Convert maf to psl format\n"
  "usage:\n"
  "   mafToPsl querySrc targetSrc in.maf out.psl\n"
  "\n"
  "The query and target src can be either an organism prefix (hg17),\n"
  "or a full src sequence name (hg17.chr11), or just the sequence name\n"
  "if the MAF does not contain organism prefixes.\n"
  "\n"
  );
}

char *skipDot(char *src)
/* skip past dot in src name */
{
char *dot = strchr(src, '.');
if (dot == NULL)
    return src;
else
    return dot+1;
}

static void convertToPsl(struct mafComp *qc, struct mafComp *tc, FILE *pslFh)
/* convert two components to a psl */
{
struct psl* psl;
int qStart = qc->start;
int qEnd = qc->start+qc->size;
int tStart = tc->start;
int tEnd = tc->start+tc->size;
char strand[3];
strand[0] = qc->strand;
strand[1] = tc->strand;
strand[2] = '\0';

    
if (qc->strand == '-')
    reverseIntRange(&qStart, &qEnd, qc->srcSize);
if (tc->strand == '-')
    reverseIntRange(&tStart, &tEnd, tc->srcSize);

psl = pslFromAlign(skipDot(qc->src), qc->srcSize, qStart, qEnd, qc->text,
                   skipDot(tc->src), tc->srcSize, tStart, tEnd, tc->text,
                   strand, 0);
if (psl != NULL)
    {
    /* drop target strand */
    if (psl->strand[1] == '-')
        pslRc(psl);
    psl->strand[1] = '\0';
    pslTabOut(psl, pslFh);
    }
}


void mafAliToPsl(char *querySrc, char *targetSrc,
                 struct mafAli *maf, FILE *pslFh)
/* convert a MAF alignment to a psl */
{
struct mafComp *qc = mafMayFindComponentDb(maf, querySrc);
struct mafComp *tc = mafMayFindComponentDb(maf, targetSrc);

if ((qc != NULL) && (tc != NULL))
    convertToPsl(qc, tc, pslFh);
}

void mafToPsl(char *querySrc, char *targetSrc, char *inName, char *outName)
/* mafToPsl - Convert maf to psl format. */
{
struct mafFile *mf = mafOpen(inName);
FILE *pslFh = mustOpen(outName, "w");
struct mafAli *maf;

while ((maf = mafNext(mf)) != NULL)
    {
    mafAliToPsl(querySrc, targetSrc, maf, pslFh);
    mafAliFree(&maf);
    }
carefulClose(&pslFh);
mafFileFree(&mf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 5)
    usage();
mafToPsl(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
