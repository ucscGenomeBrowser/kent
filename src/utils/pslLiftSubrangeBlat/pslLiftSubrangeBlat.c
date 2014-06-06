/* pslLiftSubrangeBlat - lift PSLs from blat subrange alignments. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "psl.h"
#include "pslReader.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslLiftSubrangeBlat - lift PSLs from blat subrange alignments\n"
  "usage:\n"
  "   pslLiftSubrangeBlat isPsl outPsl\n"
  "\n"
  "Lift a PSL with target coordinates from a blat subrange query\n"
  "(e.g. blah/hg18.2bit:chr1:1000-20000) which has subrange\n"
  "coordinates as the target name (e.g. chr1:1000-200000) to\n"
  "actual target coordinates.\n"
  "\n"
  "options:\n"
  "  -tSizes=szfile - lift target side based on tName, using target sizes from\n"
  "                   this tab separated file.\n"
  "  -qSizes=szfile - lift query side based on qName, using query sizes from\n"
  "                   this tab separated file.\n"
  "Must specify at least on of -tSizes or -qSize or both.\n"
  );
}

static struct optionSpec options[] = {
    {"tSizes", OPTION_STRING},
    {"qSizes", OPTION_STRING},
    {NULL, 0},
};

static struct hash *loadSizes(char *szFile)
/* load sizes into a hash */
{
struct hash *sizes = hashNew(0);
struct lineFile *lf = lineFileOpen(szFile, TRUE);
char *row[2];
while (lineFileNextRowTab(lf, row, 2))
    hashAddInt(sizes, row[0], sqlSigned(row[1]));

return sizes;
}

static boolean parseName(char *desc, char *name, int *start, int *end)
/* parse and validate name into it's parts. Return FALSE if name doesn't
 * contain a subrange specification. */
{
char *nameEnd = strchr(name, ':');
if (nameEnd == NULL)
    return FALSE;
char *p = nameEnd+1;
char *pp = strchr(p, '-');
if (pp == NULL)
    errAbort("invalid %s subrange: %s", desc, name);
*pp = '\0';
*start = sqlUnsigned(p);
*pp = '-';
pp++;
*end = sqlUnsigned(pp);
if (*end <= *start)
    errAbort("range zero or negative length in %s subrange: %s", desc, name);
*nameEnd = '\0';
return TRUE;
}

static void liftSide(char *desc, struct hash *seqSizes, struct psl *psl, char *name, char strand, unsigned *seqSize, int *start, int *end, unsigned *starts)
/* life one side of the alignment */
{
int regStart, regEnd, i;
if (parseName(desc, name, &regStart, &regEnd))
    {
    *seqSize = hashIntVal(seqSizes, name);
    if (*end > *seqSize)
        errAbort("subrange %s:%d-%d extends past sequence end %ud", name, regStart, regEnd, *seqSize);
    *start += regStart;
    *end += regStart;
    if (strand == '-')
        reverseIntRange(&regStart, &regEnd, *seqSize);
    for (i = 0; i < psl->blockCount; i++)
        starts[i] += regStart;
    }
}

static void liftIt(struct psl *psl, struct hash *qSizes, struct hash *tSizes)
/* lift one subrange PSL based on tName */
{
if (qSizes != NULL)
    liftSide("qName", qSizes, psl, psl->qName, pslQStrand(psl), &psl->qSize, &psl->qStart, &psl->qEnd, psl->qStarts);
if (tSizes != NULL)
    liftSide("tName", tSizes, psl, psl->tName, pslTStrand(psl), &psl->tSize, &psl->tStart, &psl->tEnd, psl->tStarts);
}

static void pslLiftSubrangeBlat(char *inPsl, char *outPsl, char *qSizesFile, char *tSizesFile)
/* pslLiftSubrangeBlat - lift PSLs from blat subrange alignments. */
{
struct hash *qSizes = (qSizesFile != NULL) ? loadSizes(qSizesFile) : NULL;
struct hash *tSizes = (tSizesFile != NULL) ? loadSizes(tSizesFile) : NULL;
struct pslReader *inRd = pslReaderFile(inPsl, NULL);
FILE *outFh = mustOpen(outPsl, "w");
struct psl *psl;
while ((psl = pslReaderNext(inRd)) != NULL)
    {
    liftIt(psl, qSizes, tSizes);
    pslTabOut(psl, outFh);
    pslFree(&psl);
    }
carefulClose(&outFh);
pslReaderFree(&inRd);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
char *qSizesFile = optionVal("qSizes", NULL);
char *tSizesFile = optionVal("tSizes", NULL);
if ((qSizesFile == NULL) && (tSizesFile == NULL))
    errAbort("must specify at least one of -tSizes or -qSizes");
pslLiftSubrangeBlat(argv[1], argv[2], qSizesFile, tSizesFile);
return 0;
}
