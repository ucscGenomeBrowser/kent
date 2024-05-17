/* pslSpliceJunctions - Extract splice junctions for a PSL file. */
#include "common.h"
#include "options.h"
#include "twoBit.h"
#include "psl.h"
#include "dnautil.h"

static void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslSpliceJunctions - Extract splice junctions from a PSL file\n"
  "usage:\n"
  "   pslSpliceJunctions pslFile genome2bit junctionsTsv\n"
  "options:\n"
  "\n"
  "Output query and target coordinates of target gaps, often introns,\n"
  "in alignments. Output is always in query-positive and target-positive coordinates,\n"
  "with only gaps in the target reported. Canonical junctions will be in upper cases,\n"
  "unknown ones lower case. \n");
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {NULL, 0},
};

/* canonical splice junctions */
static char *canonicalJuncs[][2] = {
    {"GT", "AG"},
    {"AT", "AC"},
    {NULL, NULL}
};

static boolean isCanonicalJuncs(char *junc5p, char *junc3p)
/* is this canonical junction pair? */
{
for (int i = 0; canonicalJuncs[i][0] != NULL; i++)
    {
    if (sameString(junc5p, canonicalJuncs[i][0]) && sameString(junc3p, canonicalJuncs[i][1]))
        return TRUE;
    }
return FALSE;
}
    
static void getJuncs(struct psl *transPsl, unsigned tStart, unsigned tEnd, unsigned tSize,
                     struct twoBitFile *genomeFh, char junc5p[3], char junc3p[3])
/* get sequences at junctions */
{
if (pslTStrand(transPsl) == '-')
    reverseUnsignedRange(&tStart, &tEnd, transPsl->tSize);
struct dnaSeq *juncA = twoBitReadSeqFrag(genomeFh, transPsl->tName, tStart, tStart + 2);
struct dnaSeq *juncB = twoBitReadSeqFrag(genomeFh, transPsl->tName, tEnd - 2, tEnd);

if (pslTStrand(transPsl) == '-')
    {
    struct dnaSeq *hold = juncA;
    juncA = juncB;
    juncB = hold;
    reverseComplement(juncA->dna, 2);
    reverseComplement(juncB->dna, 2);
    }

safecpy(junc5p, sizeof(junc5p), juncA->dna);
safecpy(junc3p, sizeof(junc3p), juncB->dna);
freeDnaSeq(&juncA);
freeDnaSeq(&juncB);

touppers(junc5p);
touppers(junc3p);
if (!isCanonicalJuncs(junc5p, junc3p))
    {
    tolowers(junc5p);
    tolowers(junc3p);
    }
}

static void writeRow(FILE *juncsTsvFh,  struct psl *transPsl,
                     unsigned qStart, unsigned qEnd,  unsigned tStart, unsigned tEnd,
                     int iBlk, int tGapIdx, char *junc5p, char *junc3p)
/* output information about one junction or gap. */
{
if (pslTStrand(transPsl) == '-')
    reverseUnsignedRange(&tStart, &tEnd, transPsl->tSize);

fprintf(juncsTsvFh, "%s\t%u\t%u\t%d\t%s\t%u\t%u\t%d\t%c\t%d\t%d\t%u\t%s\t%s\t%d\n",
        transPsl->qName, qStart, qEnd, transPsl->qSize,
        transPsl->tName, tStart, tEnd, transPsl->tSize,
        pslTStrand(transPsl), iBlk, tGapIdx, tEnd - tStart, junc5p, junc3p,
        isCanonicalJuncs(junc5p, junc3p));
}

static void processPslGap(struct psl* transPsl, int iBlk, int tGapIdx, struct twoBitFile *genomeFh, FILE *juncsTsvFh)
/* process one target gap; psl is query-positive */
{
char junc5p[3], junc3p[3];
getJuncs(transPsl, pslTEnd(transPsl, iBlk), pslTStart(transPsl, iBlk + 1), pslTStrand(transPsl),
         genomeFh, junc5p, junc3p);
writeRow(juncsTsvFh, transPsl, pslQEnd(transPsl, iBlk), pslQStart(transPsl, iBlk + 1),
         pslTStart(transPsl, iBlk), pslTEnd(transPsl, iBlk + 1), iBlk, tGapIdx,
         junc5p, junc3p);
}

static void processPsl(struct psl* transPsl, struct twoBitFile *genomeFh, FILE *juncsTsvFh)
/* get splice junctions of one alignment */
{
if (pslQStrand(transPsl) == '-')
    pslRc(transPsl);
int tGapIdx = 0;
for (int iBlk = 0; iBlk < transPsl->blockCount - 1; iBlk++)
    {
    if (pslTEnd(transPsl, iBlk) < pslTStart(transPsl, iBlk + 1))
        {
        processPslGap(transPsl, iBlk, tGapIdx, genomeFh, juncsTsvFh);
        tGapIdx++;
        }
    }
}

static void pslSpliceJunctions(char *pslFile, char *genome2bitFile, char *junctionsTsv)
/* pslSpliceJunctions - Extract splice junctions for a PSL file. */
{
struct psl *transPsls = pslLoadAll(pslFile);
slSort(&transPsls, pslCmpTarget);

struct twoBitFile *genomeFh = twoBitOpen(genome2bitFile);
FILE *juncsTsvFh = mustOpen(junctionsTsv, "w");
fprintf(juncsTsvFh, "qName\tqStart\tqEnd\tqSize\ttName\ttStart\ttEnd\ttSize\tstrand\tiBlk\ttGapIdx\ttGapSize\tjunc5p\tjunc3p\tisCannon\n");

for (struct psl *transPsl = transPsls; transPsl != NULL; transPsl = transPsl->next)
    processPsl(transPsl, genomeFh, juncsTsvFh);
carefulClose(&juncsTsvFh);
twoBitClose(&genomeFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
pslSpliceJunctions(argv[1], argv[2], argv[3]);
return 0;
}
