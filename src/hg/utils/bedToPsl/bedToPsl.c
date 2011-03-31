/* bedToGenePred - convert bed format files to genePred format */
#include "common.h"
#include "options.h"
#include "bed.h"
#include "psl.h"
#include "sqlNum.h"
#include "hash.h"
#include "linefile.h"

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

/* command line options */

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s:\n"
    "bedToPsl - convert bed format files to psl format\n"
    "usage:\n"
    "   bedToPsl chromSizes bedFile pslFile\n"
    "\n"
    "Convert a BED file to a PSL file. This the result is an alignment.\n"
    " It is intended to allow processing by tools that operate on PSL.\n"
    "If the BED has at least 12 columns, then a PSL with blocks is created.\n"
    "Otherwise single-exon PSLs are created.\n", msg);
}

static struct hash *loadChromSizes(char *chromSizesFile)
/* read the chromosome sizes file. */
{
struct lineFile *lf = lineFileOpen(chromSizesFile, TRUE);
struct hash *sizes = newHash(12);
char *words[2];
while (lineFileRow(lf, words))
    hashAddInt(sizes, words[0], sqlSigned(words[1]));
lineFileClose(&lf);
return sizes;
}

static void bedToPsl4(struct bed *bed, struct psl *psl)
/* convert a 4-column BED to a psl with one block */
{
psl->blockCount++;
psl->qStarts[0] = 0;
psl->tStarts[0] = bed->chromStart;
psl->blockSizes[0] = bed->chromEnd - bed->chromStart;
psl->blockCount++;
}

static void bedToPsl12(struct bed *bed, struct psl *psl)
/* convert a 4-column BED to a psl */
{
int i, qNext = 0;
for (i = 0; i < bed->blockCount; i++)
    {
    psl->qStarts[i] = qNext;
    psl->tStarts[i] = bed->chromStarts[i] + bed->chromStart;
    psl->blockSizes[i] = bed->blockSizes[i];
    if (i > 0)
        {
        psl->tNumInsert += 1;
        psl->tBaseInsert += psl->tStarts[i] - pslTEnd(psl, i-1);
        }
    psl->blockCount++;
    qNext += bed->blockSizes[i];
    }
}

static struct psl *bedToPsl(struct bed *bed, struct hash *chromSizes)
/* Convert a single bed to a PSL. */
{
int qSize = bedTotalBlockSize(bed);
struct psl *psl = pslNew(bed->name, qSize, 0, qSize,
                         bed->chrom, hashIntVal(chromSizes, bed->chrom), bed->chromStart, bed->chromEnd,
                         ((bed->strand[0] == '\0') ? "+" : bed->strand), (bed->blockCount == 0) ? 1 : bed->blockCount, 0);
psl->match = psl->qSize;
if (bed->blockCount == 0)
    bedToPsl4(bed, psl);
else
    bedToPsl12(bed, psl);
return psl;
}

/* convert one line read from a bed file to a PSL */
void cnvBedRec(char *line, struct hash *chromSizes, FILE *pslFh)
{
char *row[12];
int numCols = chopByWhite(line, row, ArraySize(row));
if (numCols < 4)
    errAbort("bed must have at least 4 columns");
struct bed *bed = bedLoadN(row, numCols);
struct psl* psl = bedToPsl(bed, chromSizes);
pslTabOut(psl, pslFh);
pslFree(&psl);
bedFree(&bed);
}

void cnvBedToPsl(char *chromSizesFile, char *bedFile, char *pslFile)
/* convert bed format files to PSL format */
{
struct hash *chromSizes = loadChromSizes(chromSizesFile);
struct lineFile *bedLf = lineFileOpen(bedFile, TRUE);
FILE *pslFh = mustOpen(pslFile, "w");
char *line;

while (lineFileNextReal(bedLf, &line))
    cnvBedRec(line, chromSizes, pslFh);

carefulClose(&pslFh);
lineFileClose(&bedLf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage("Too few arguments");
cnvBedToPsl(argv[1], argv[2], argv[3]);
return 0;
}

