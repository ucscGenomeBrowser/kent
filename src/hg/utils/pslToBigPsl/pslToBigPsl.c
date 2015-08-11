/* pslToBigPsl - converts genePred or genePredExt to bigGenePred. */
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
  "  -snakes\n"
  "NOTE: to build bigBed:\n"
  "   bedToBigBed -type=bed12+12 -tab -as=bigPsl.as file.bigPslInput chrom.sizes output.bb\n"
  );
}

boolean snakeMode = FALSE;

/* Command line validation table. */
static struct optionSpec options[] = {
   {"cds", OPTION_STRING},
   {"fa", OPTION_STRING},
   {"snakes", OPTION_BOOLEAN},
   {NULL, 0},
};

#define MAX_BLOCKS 10000
unsigned blockSizes[MAX_BLOCKS];
unsigned blockStarts[MAX_BLOCKS];
unsigned oBlockStarts[MAX_BLOCKS];

void outBigPsl(FILE *fp, struct psl *psl, struct hash *fastaHash, struct hash *cdsHash)
{
struct bigPsl bigPsl;

if (psl->blockCount > MAX_BLOCKS)
    errAbort("psl has more than %d blocks, make MAX_BLOCKS bigger in source", MAX_BLOCKS);


bigPsl.chrom = psl->tName;
// what about tSize?
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
if (psl->strand[1])
    bigPsl.oStrand[0] = psl->strand[1];
else
    bigPsl.oStrand[0] = '+';
bigPsl.oStrand[1] = 0;
bigPsl.reserved = 0; // BLACK
bigPsl.blockCount = psl->blockCount;
bigPsl.blockSizes = (int *)blockSizes;
bigPsl.chromStarts = (int *)blockStarts;
bigPsl.oChromStarts = (int *)oBlockStarts;
bigPsl.oSequence = "";
int ii;
for(ii=0; ii < bigPsl.blockCount; ii++)
    {
    blockStarts[ii] = psl->tStarts[ii] - bigPsl.chromStart;
    //oBlockStarts[ii] = psl->qStarts[ii] - bigPsl.oChromStart;
    oBlockStarts[ii] = psl->qStarts[ii] ;
    blockSizes[ii] = psl->blockSizes[ii];
    }
    
if (fastaHash)
    {
    struct dnaSeq *seq = hashFindVal(fastaHash, psl->qName);

    if (seq != NULL)
	bigPsl.oSequence = seq->dna;
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
    else
    	warn("CDS missing for %s\n", psl->qName);
    }

bigPslOutput(&bigPsl, fp, '\t', '\n');
}


struct snakeBlock
{
struct snakeBlock *next;
unsigned tStart, qStart, size;
char strand;
unsigned qNumber;
};

struct listHead 
{
    char *combo;
    struct snakeBlock *sbList;
};

struct hash *parsePsls(struct psl *psl)
{
struct hash *chromHash = newHash(10);
struct listHead *listHead = NULL;
char *lastCombo = "noCombo";

for(; psl; psl = psl->next)
    {
    char buffer[1024];
    safef(buffer, sizeof buffer, "%s-%s", psl->tName, psl->qName);
    if (differentString(lastCombo, buffer))
	{
	lastCombo = cloneString(buffer);
	listHead = hashFindVal(chromHash, lastCombo);
	}
    int ii;
    for(ii=0; ii < psl->blockCount; ii++)
	{
	struct snakeBlock *sb;
	AllocVar(sb);
	sb->tStart = psl->tStarts[ii];
	sb->qStart = psl->qStarts[ii];
	sb->size = blockSizes[ii];
	sb->strand = psl->strand[0];

	if (sb->strand == '-')
	    sb->qStart = psl->qSize - sb->qStart;

	if (listHead == NULL)
	    {
	    AllocVar(listHead);
	    listHead->sbList = sb;
	    listHead->combo = lastCombo;
	    hashAdd(chromHash, lastCombo, listHead);
	    }
	else
	    slAddHead(&listHead->sbList, sb);
	}
    }
return chromHash;
}

static int snakeBlockCmpTStart(const void *va, const void *vb)
/* sort by start position on the query sequence */
{
const struct snakeBlock *a = *((struct snakeBlock **)va);
const struct snakeBlock *b = *((struct snakeBlock **)vb);
int diff = a->tStart - b->tStart;

if (diff == 0)
    {
    diff = a->qStart - b->qStart;
    }

return diff;
}

static int snakeBlockCmpQStart(const void *va, const void *vb)
/* sort by start position on the query sequence */
{
const struct snakeBlock *a = *((struct snakeBlock **)va);
const struct snakeBlock *b = *((struct snakeBlock **)vb);
int diff = a->qStart - b->qStart;

if (diff == 0)
    {
    diff = a->tStart - b->tStart;
    }

return diff;
}

void pslToBigPsl(char *pslFile, char *bigPslOutput, char *fastaFile, char *cdsFile)
/* pslToBigPsl - converts psl to bigPsl. */
{
struct psl *psl = pslLoadAll(pslFile);
struct hash *fastHash = NULL;
struct hash *cdsHash = NULL;

if (fastaFile != NULL)
    {
    struct dnaSeq  *seqs = faReadAllDna(fastaFile);
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
	/*
	struct genbankCds *cds;
	AllocVar(cds);
	genbankCdsParse(row[1], cds); 
	*/
        hashAdd(cdsHash, row[0], cloneString(row[1]));
	}
	  
    lineFileClose(&lf);
    }

FILE *fp = mustOpen(bigPslOutput, "w");

if (snakeMode)
    {
    struct hash *chromHash = parsePsls(psl);
    struct hashCookie cook = hashFirst(chromHash);
    struct hashEl *hel;

    while((hel = hashNext(&cook)) != NULL)
	{
	struct listHead *head = hel->val;
	slSort(&head->sbList,  snakeBlockCmpQStart);
	struct snakeBlock *sb  = head->sbList;
	unsigned ii = 0;

	for(; sb; sb = sb->next)
	    sb->qNumber = ii++;
	slSort(&head->sbList,  snakeBlockCmpTStart);
	}
    }
else
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

snakeMode = optionExists("snakes");

if (snakeMode)
    {
    if (fastaFile != NULL)
	errAbort("fasta specification doesn't work for snakes");
    if (cdsFile != NULL)
	errAbort("cds specification doesn't work for snakes");
    }

pslToBigPsl(argv[1], argv[2], fastaFile, cdsFile);
return 0;
}
