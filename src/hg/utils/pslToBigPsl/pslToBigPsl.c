/* pslToBigPsl - converts psl to bigPsl. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "genePred.h"
#include "psl.h"
#include "bigPsl.h"
#include "fa.h"
#include "genbank.h"
#include "net.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslToBigPsl - converts psl to bigPsl input (bed format with extra fields)\n"
  "usage:\n"
  "  pslToBigPsl file.psl stdout | sort -k1,1 -k2,2n > file.bigPslInput\n"
  "options:\n"
  "  -cds=file.cds\n"
  "  -fa=file.fasta\n"
  "NOTE: to build bigBed:\n"
  "   bedToBigBed -type=bed12+13 -tab -as=bigPsl.as file.bigPslInput chrom.sizes output.bb\n"
  );
}

boolean snakeMode = FALSE;

/* Command line validation table. */
static struct optionSpec options[] = {
   {"cds", OPTION_STRING},
   {"fa", OPTION_STRING},
   {NULL, 0},
};

static unsigned initNumBlocks = 1000;
static unsigned currentNumBlocks = 0;
static unsigned *blockSizes = NULL;
static unsigned *blockStarts = NULL;
static unsigned *oBlockStarts = NULL;

static void growBlockSpace(unsigned minBlockCount)
/* Ensure that there is sufficient space in the block arrays  */
{
/* always ask for more then min */
int size = max(2 * minBlockCount, initNumBlocks);
if (currentNumBlocks == 0)
    {
    AllocArray(blockSizes, size);
    AllocArray(blockStarts, size);
    AllocArray(oBlockStarts, size);
    }
else
    {
    ExpandArray(blockSizes, currentNumBlocks, size);
    ExpandArray(blockStarts, currentNumBlocks, size);
    ExpandArray(oBlockStarts, currentNumBlocks, size);
    }
currentNumBlocks = size;
}

void outBigPsl(FILE *fp, struct psl *psl, struct hash *fastaHash, struct hash *cdsHash)
{
struct bigPsl bigPsl;

if (psl->blockCount > currentNumBlocks)
    growBlockSpace(psl->blockCount);

// make sure blocks are represented on reference's positive strand as required by BED format
boolean didRc = FALSE;
if (psl->strand[1] == '-')
    {
    didRc = TRUE;
    pslRc(psl); 
    }

bigPsl.chrom = psl->tName;
bigPsl.chromSize = psl->tSize;
bigPsl.match = psl->match;
bigPsl.misMatch = psl->misMatch;
bigPsl.repMatch = psl->repMatch;
bigPsl.nCount = psl->nCount;
bigPsl.oChromStart = psl->qStart;
bigPsl.oChromEnd = psl->qEnd;
bigPsl.oChromSize = psl->qSize;
bigPsl.chromStart = psl->tStart;
bigPsl.chromEnd = psl->tEnd;
bigPsl.thickStart = psl->tStart;
bigPsl.thickEnd = psl->tEnd;
bigPsl.name = psl->qName;
bigPsl.score = 1000;
bigPsl.strand[0] = psl->strand[0];
bigPsl.strand[1] = 0;
if (didRc)
    bigPsl.oStrand[0] = '-';
else
    bigPsl.oStrand[0] = '+';
bigPsl.oStrand[1] = 0;
bigPsl.reserved = 0; // BLACK
bigPsl.blockCount = psl->blockCount;
bigPsl.blockSizes = (int *)blockSizes;
bigPsl.chromStarts = (int *)blockStarts;
bigPsl.oChromStarts = (int *)oBlockStarts;
bigPsl.oSequence = "";
bigPsl.oCDS = "";
int ii;
boolean isProt = pslIsProtein(psl);
int mult = pslIsProtein(psl) ? 3 : 1;
for(ii=0; ii < bigPsl.blockCount; ii++)
    {
    blockStarts[ii] = psl->tStarts[ii] - bigPsl.chromStart;
    oBlockStarts[ii] = psl->qStarts[ii] ;
    blockSizes[ii] = psl->blockSizes[ii] * mult;
    }
    
bigPsl.seqType = PSL_SEQTYPE_EMPTY;
if (fastaHash)
    {
    struct dnaSeq *seq = hashFindVal(fastaHash, psl->qName);

    if (seq != NULL)
        {
	bigPsl.oSequence = seq->dna;
        bigPsl.seqType = isProt ? PSL_SEQTYPE_PROTEIN : PSL_SEQTYPE_NUCLEOTIDE;
        }
    else
	{
	warn("Cannot find sequence for %s. Dropping", psl->qName);
	return;
	}
    }

if (cdsHash)
    {
    char *cds = hashFindVal(cdsHash, psl->qName);

    if (cds != NULL)
	{
	bigPsl.oCDS = cds;

	struct genbankCds mrnaCds;
	genbankCdsParse(cds, &mrnaCds); 
	struct genbankCds genomeCds = genbankCdsToGenome(&mrnaCds, psl);
	bigPsl.thickStart = genomeCds.start;
	bigPsl.thickEnd = genomeCds.end;
	}
    }

bigPslOutput(&bigPsl, fp, '\t', '\n');
}


void pslToBigPsl(char *pslFile, char *bigPslOutput, char *fastaFile, char *cdsFile)
/* pslToBigPsl - converts psl to bigPsl. */
{
struct psl *psl = pslLoadAll(pslFile);
struct hash *fastHash = NULL;
struct hash *cdsHash = NULL;

if (fastaFile != NULL)
    {
    struct dnaSeq  *seqs = pslIsProtein(psl) ?faReadAllPep(fastaFile) : faReadAllDna(fastaFile); 
    fastHash = newHash(10);
    for(; seqs; seqs = seqs->next)
	hashAdd(fastHash, seqs->name, seqs);
    }

if (cdsFile != NULL)
    {
    cdsHash = newHash(10);
    struct lineFile *lf = netLineFileOpen(cdsFile);
    char *row[2];
    while (lineFileRow(lf, row))
	{
        hashAdd(cdsHash, row[0], cloneString(row[1]));
	}
	  
    lineFileClose(&lf);
    }

FILE *fp = mustOpen(bigPslOutput, "w");

for(; psl ; psl = psl->next)
    {
    outBigPsl(fp, psl, fastHash, cdsHash);
    }

fclose(fp);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
char *fastaFile = optionVal("fa", NULL);
char *cdsFile = optionVal("cds", NULL);

pslToBigPsl(argv[1], argv[2], fastaFile, cdsFile);
return 0;
}
