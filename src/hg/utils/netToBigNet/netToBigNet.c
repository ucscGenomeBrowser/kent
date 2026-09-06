/* netToBigNet - converts a net to bigNet. */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "chainNet.h"
#include "bigNet.h"

boolean warnFlag = FALSE;	/* Convert even with missing fields. */
boolean warned = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netToBigNet - converts a net to bigNet input (bed format with extra fields)\n"
  "usage:\n"
  "  netToBigNet netIn bigNetOut\n"
  "options:\n"
  "  -warn   convert even with missing fields\n"
  "\n"
  "Output will be sorted\n"
  "\n"
  "To build the bigBed file:\n"
  "  bedToBigBed -type=bed6+20 -as=bigNet.as -tab data.bigNet hg38.chrom.sizes data.net.bb\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"warn", OPTION_BOOLEAN},
   {NULL, 0},
};

static void checkFill(struct cnFill *fill)
/* Complain if the fields netSyntenic and netClass add are missing. */
{
if (fill->chainId == 0)
    return;
if (fill->type == NULL)
    errAbort("No type field, please run netSyntenic on input");
if (fill->tN < 0)
    {
    if (!warnFlag)
        errAbort("Missing fields.  Please run netClass on input");
    if (!warned)
        {
        fprintf(stderr, "Warning: missing fields\n");
        warned = TRUE;
        }
    }
}

static struct bigNet *cnFillToBigNet(char *chrom, struct cnFill *fill, int level)
/* Make a bigNet record out of one fill or gap of a net. */
{
struct bigNet *bn;
AllocVar(bn);
bn->chrom = cloneString(chrom);
bn->chromStart = fill->tStart;
bn->chromEnd = fill->tStart + fill->tSize;
bn->name = cloneString(fill->qName);
bn->score = 1000;
bn->strand[0] = fill->qStrand;
bn->level = level;
bn->qStart = fill->qStart;
bn->qEnd = fill->qStart + fill->qSize;
bn->chainId = fill->chainId;
bn->ali = fill->ali;
bn->chainScore = (fill->score < 0 ? 0 : fill->score);
bn->type = cloneString(fill->type == NULL ? "gap" : fill->type);
bn->qOver = fill->qOver;
bn->qFar = fill->qFar;
bn->qDup = fill->qDup;
bn->tN = fill->tN;
bn->qN = fill->qN;
bn->tR = fill->tR;
bn->qR = fill->qR;
bn->tNewR = fill->tNewR;
bn->qNewR = fill->qNewR;
bn->tOldR = fill->tOldR;
bn->qOldR = fill->qOldR;
bn->tTrf = fill->tTrf;
bn->qTrf = fill->qTrf;
return bn;
}

static void convertFills(char *chrom, struct cnFill *fillList, int level,
                         struct bigNet **bigNets)
/* Recursively convert a list of fills and the gaps under them. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    checkFill(fill);
    slAddHead(bigNets, cnFillToBigNet(chrom, fill, level));
    if (fill->children)
        convertFills(chrom, fill->children, level+1, bigNets);
    }
}

static struct bigNet *convertNets(char *netIn)
/* Convert every net in a file to a sorted list of bigNet records. */
{
struct bigNet *bigNets = NULL;
struct chainNet *net;
struct lineFile *lf = lineFileOpen(netIn, TRUE);
while ((net = chainNetRead(lf)) != NULL)
    {
    convertFills(net->name, net->fillList, 1, &bigNets);
    chainNetFree(&net);
    }
lineFileClose(&lf);
slSort(&bigNets, bigNetCmpTarget);
return bigNets;
}

static void bigNetWriteOne(struct bigNet *bn, FILE *f)
/* Write one bigNet record as a tab separated line.  This does what
 * bigNetTabOut does, except that the score is written the way hgLoadNet
 * writes it.  bigNetTabOut uses %g, which drops digits off a chain score. */
{
fprintf(f, "%s\t%u\t%u\t%s\t%u\t%s\t%u\t%u\t%u\t%u\t%u\t%1.1f\t%s",
    bn->chrom, bn->chromStart, bn->chromEnd, bn->name, bn->score, bn->strand,
    bn->level, bn->qStart, bn->qEnd, bn->chainId, bn->ali, bn->chainScore, bn->type);
fprintf(f, "\t%d\t%d\t%d", bn->qOver, bn->qFar, bn->qDup);
fprintf(f, "\t%d\t%d\t%d\t%d", bn->tN, bn->qN, bn->tR, bn->qR);
fprintf(f, "\t%d\t%d\t%d\t%d", bn->tNewR, bn->qNewR, bn->tOldR, bn->qOldR);
fprintf(f, "\t%d\t%d\n", bn->tTrf, bn->qTrf);
}

static void netToBigNet(char *netIn, char *bigNetOut)
/* netToBigNet - converts a net to a bigNet file. */
{
struct bigNet *bigNets = convertNets(netIn);
FILE *f = mustOpen(bigNetOut, "w");
struct bigNet *bn;
for (bn = bigNets; bn != NULL; bn = bn->next)
    bigNetWriteOne(bn, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
warnFlag = optionExists("warn");
netToBigNet(argv[1], argv[2]);
return 0;
}
