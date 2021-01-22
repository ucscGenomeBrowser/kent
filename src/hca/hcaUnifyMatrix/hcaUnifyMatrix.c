/* hcaUnifyMatrix - Given a list of matrices on maybe different gene sets produce 
 * a unified matrix on one gene set. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "rangeTree.h"
#include "vMatrix.h"
#include "obscure.h"
#include "../../hg/inc/txGraph.h"
#include "../../hg/inc/bed12Source.h"
#include "../../hg/inc/bed.h"

#define BED12_NAME2_SIZE 13
#define BED_NAME_FIELD 3

double emptyVal = -1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hcaUnifyMatrix - Given a list of matrices on maybe different gene sets produce a unified matrix on one gene set\n"
  "usage:\n"
  "   hcaUnifyMatrix graph.txg inList.tsv outMatrix.tsv\n"
  "where\n"
  "   graph.txg is made with txBedToGraph on the mapping.beds in the input\n"
  "   inList.tsv has one line per input we are unifying.  It is tab separated with columns:\n"
  "       1 - name of input to add to column labels\n"
  "       2 - relative priority, lower numbers get attended to first in gene mapping\n"
  "       3 - input mapping bed file, often result of gencodeVersionForGenes -bed output\n"
  "       4 - input expression matrix file in tsv format\n"
  "options:\n"
  "   -bed=mapping.bed\n"
  "   -empty=val - store val (rather than default -1) to show missing data\n"
  "   -trackDb=stanza.ra - save trackDb stanza here\n"
  "   -stats=stats.tsv - save merged stats file here\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"bed", OPTION_STRING},
   {"empty", OPTION_DOUBLE},
   {"trackDb", OPTION_STRING},
   {"stats", OPTION_STRING},
   {NULL, 0},
};

struct uniInput
/* A single input to our unification process. */
    {
    struct uniInput *next;  /* Next in singly linked list. */
    char *name;	/* Short, will be reused as label prefix */
    int priority;	/* 1 is attended to first, then 2, etc.  */
    double scaleOut;	/* Everything gets multiplied by this in the output */
    char *mappingBed;	/* Mapping bed file name */
    char *matrixFile;	/* Matrix file name */
    char *colorFile;	/* File it first column sample, last color */
    char *statsFile;	/* File stats are in */
    struct hash *rowHash;	/* keyed by 4th field, values char**  */
    struct hash *bedHash;	/* Also keyed by 4th field */
    struct slRef *rowList;	/* List of all parsed out rows as char** */
    struct memMatrix *matrix;   /* Loaded matrixFile */
    struct hash *matrixHash;	/* Matrix Y value keyed by gene name */
    };


struct uniInput *uniInputLoad(char **row)
/* Load a uniInput from row fetched with select * from uniInput
 * from database.  Dispose of this with uniInputFree(). */
{
struct uniInput *ret;

AllocVar(ret);
ret->name = cloneString(row[0]);
ret->priority = sqlSigned(row[1]);
ret->scaleOut = sqlDouble(row[2]);
ret->mappingBed = cloneString(row[3]);
ret->matrixFile = cloneString(row[4]);
ret->colorFile = cloneString(row[5]);
ret->statsFile = cloneString(row[6]);
return ret;
}

struct uniInput *uniInputLoadAll(char *fileName) 
/* Load all uniInput from a whitespace-separated file.
 * Dispose of this with uniInputFreeList(). */
{
struct uniInput *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[7];

while (lineFileRow(lf, row))
    {
    el = uniInputLoad(row);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

void loadMappingBed(struct uniInput *uni)
/* Load up a mapping bed file and make up row and bed hashes from it */
{
struct hash *rowHash = uni->rowHash = hashNew(0);
struct hash *bedHash = uni->bedHash = hashNew(0);
struct lm *lm = rowHash->lm;

struct lineFile *lf = lineFileOpen(uni->mappingBed, TRUE);
char *row[BED12_NAME2_SIZE];
while (lineFileRow(lf, row))
    {
    char *name = row[BED_NAME_FIELD];
    struct bed12Source *bed = bed12SourceLoad(row);
    hashAdd(bedHash, name, bed);
    char **rowCopy = lmCloneRow(lm, row, ArraySize(row));
    hashAdd(rowHash, name, rowCopy);
    refAdd(&uni->rowList, rowCopy);
    }
slReverse(&uni->rowList);
lineFileClose(&lf);
}

void loadMatrix(struct uniInput *uni)
/* Load up a mapping bed file and make up row and bed hashes from it */
{
struct memMatrix *matrix = uni->matrix = memMatrixFromTsv(uni->matrixFile);
struct hash *hash = uni->matrixHash = hashNew(0);
int y;
for (y=0; y<matrix->ySize; ++y)
    hashAddInt(hash, matrix->yLabels[y], y);
}

int countDistinctTypes(struct txGraph *gList)
/* Just count up how many distinct source types we got to track in all graphs*/
{
/* Figure up how many distinct sources we have */
struct hash *uniq = hashNew(0);
struct txGraph *g;
for (g = gList; g != NULL; g = g->next)
    {
    int i;
    for (i=0; i<g->sourceCount; ++i)
        {
	char *typeName = g->sources[i].type;
	hashStore(uniq, typeName);
	}
    }
int typeCount = uniq->elCount;
hashFree(&uniq);
return typeCount;
}

int countDistinctSourceTypes(struct txGraph *g)
/* Count up number of sources in one graph */
{
struct hash *typeHash = hashNew(0);
struct txSource *sources = g->sources;
int count = g->sourceCount;
int i;
for (i=0; i<count; ++i)
    {
    char *typeName = sources[i].type;
    hashStore(typeHash, typeName);
    }
int retVal = typeHash->elCount;
hashFree(&typeHash);
return retVal;
}


struct rbTree *rangeTreeOnBed(struct bed *bed)
/* Return an rbTree holding all the blocks of the bed */
{
struct rbTree *t = rangeTreeNew();
int i;
for (i=0; i<bed->blockCount; ++i)
    {
    int start = bed->chromStart + bed->chromStarts[i];
    int end = start + bed->blockSizes[i];
    rangeTreeAdd(t, start, end);
    }
return t;
}

int bedBedOverlap(struct bed *a, struct bed *b)
/* Return amount of overlap at exon level between a and b */
{
if (!sameString(a->chrom, b->chrom))
    return 0;
if (a->strand[0] != b->strand[0])
    return 0;
int overlap = rangeIntersection(a->chromStart, a->chromEnd, b->chromStart, b->chromEnd);
if (overlap <= 0)
    return 0;

/* Ok, go do the slow things of building up a range tree.  We could write some code
 * that is a little complex like a merge sort to avoid this, maybe some day if this 
 * is too slow. */
struct rbTree *t = rangeTreeOnBed(a);
int totalOverlap = 0;
int i;
for (i=0; i<b->blockCount; ++i)
    {
    int start = b->chromStart + b->chromStarts[i];
    int end = start + b->blockSizes[i];
    totalOverlap += rangeTreeOverlapSize(t, start, end);
    }
rangeTreeFree(&t);
return totalOverlap;
}

char *findClosestOfType(struct txGraph *g, struct uniInput *type, struct bed *bed)
/* Find the accession of the source of given type that overlaps most with bed */
{
struct txSource *txSource = g->sources;
int i;
int bestOverlap = 0;
char *bestAcc = NULL;
for (i=0; i<g->sourceCount; ++i)
    {
    if (sameString(type->mappingBed, txSource->type))
        {
	struct bed *other = hashMustFindVal(type->bedHash, txSource->accession);
	int overlap = bedBedOverlap(bed, other);
	if (overlap > bestOverlap)
	    {
	    bestOverlap = overlap;
	    bestAcc = txSource->accession;
	    }
	}
    txSource += 1;
    }
return bestAcc;
}

struct txSource *findBestSource(struct txGraph *g, struct bed *bestBed, struct uniInput *uni)
/* Go through all transcripts belonging to txSource in g, and try to find for them
 * the best mate among the chosen type in the very same g.  This will output no
 * more than one mate for each element of bestType and of ourType */
{
char *ourType = uni->mappingBed;
int i;
struct txSource *bestSource = NULL;
int bestOverlap = 0;
struct txSource *txSource = g->sources;
for (i=0; i<g->sourceCount; ++i)
    {
    if (sameString(txSource->type, ourType))
        {
	char *ourAcc = txSource->accession;
	struct bed *ourBed = hashMustFindVal(uni->bedHash, ourAcc);
	int overlap = bedBedOverlap(bestBed, ourBed);
	if (overlap > bestOverlap)
	    {
	    bestOverlap = overlap;
	    bestSource = txSource;
	    }
	}
    txSource += 1;
    }
return bestSource;
}


void outputTrackDb(char *fileName, struct uniInput *uniList)
/* Write out a trackDb stanza for us */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "track uniBarChart\n");
fprintf(f, "type bigBarChart\n");
struct dyString *bars = dyStringNew(0), *colors = dyStringNew(0);
dyStringAppend(bars, "barChartBars");
dyStringAppend(colors, "barChartColors");
struct uniInput *uni;
for (uni = uniList; uni != NULL; uni = uni->next)
    {
    struct lineFile *lf = lineFileOpen(uni->colorFile, TRUE);
    char *line;
    while (lineFileNextReal(lf, &line))
        {
	char *firstWord = nextTabWord(&line);
	char *lastWord = NULL;
	char *word;
	while ((word = nextTabWord(&line)) != NULL)
	     lastWord = word;
	subChar(firstWord, ' ', '_');	// Oh no, underbars!
	dyStringPrintf(bars, " %s_%s", uni->name, firstWord);
	dyStringPrintf(colors, " %s", lastWord);
	}
    lineFileClose(&lf);
    }
fprintf(f, "%s\n", bars->string);
fprintf(f, "%s\n", colors->string);
carefulClose(&f);
}

void outputStats(char *fileName, struct uniInput *uniList)
/* Write out a trackDb stanza for us */
{
FILE *f = mustOpen(fileName, "w");
struct uniInput *uni;
for (uni = uniList; uni != NULL; uni = uni->next)
    {
    struct lineFile *lf = lineFileOpen(uni->statsFile, TRUE);
    char *line;
    while (lineFileNext(lf, &line, NULL))
        {
	char *start = skipLeadingSpaces(line);
	if (start[0] == '#' || start[0] == 0)
	    fprintf(f, "%s\n", line);
	else
	    {
	    fprintf(f, "%s %s\n", uni->name, line);
	    }
	}
    lineFileClose(&lf);
    }
carefulClose(&f);
}



void writeOneGraph(struct txGraph *g,  struct uniInput *uniList, struct hash *uniHash, 
    FILE *f, FILE *bedF)
/* Here we write out one graph.  Normally just write in one piece, but
 * we will at least snoop for those that need breaking up */
{
verbose(3, "writeOneGraph %s with %d sources\n", g->name, g->sourceCount);
/* Find best txSource and it's associated type. */
int i;
struct txSource *bestTxSource = NULL;
int bestPriority = BIGNUM;  // priority is better at 1 than 2
for (i=0; i<g->sourceCount; ++i)
    {
    struct txSource *txSource = &g->sources[i];
    struct uniInput *uni = hashMustFindVal(uniHash, txSource->type);
    if (bestPriority > uni->priority)
        {
	bestPriority = uni->priority;
	bestTxSource = txSource;
	}
    txSource += 1;
    }
char *bestType = bestTxSource->type;
struct uniInput *bestUni = hashMustFindVal(uniHash, bestType);
struct hash *rowHash = bestUni->rowHash;
fprintf(f, "%s", bestTxSource->accession);

/* Now go through and output all from that type */
for (i=0; i<g->sourceCount; ++i)
    {
    struct txSource *txSource = &g->sources[i];
    if (sameString(txSource->type, bestType))
        {
	if (bedF != NULL)
	    {
	    char **row = hashMustFindVal(rowHash, txSource->accession);
	    writeTsvRow(bedF, BED12_NAME2_SIZE, row);
	    }

	/* Output matrix */
	struct uniInput *uni;
	struct bed *bestBed = hashMustFindVal(bestUni->bedHash, txSource->accession);
	for (uni = uniList; uni != NULL; uni = uni->next)
	    {
	    struct txSource *ourSource = findBestSource(g, bestBed, uni);
	    if (ourSource != NULL)
	        {
		char *geneName = ourSource->accession;
		struct hashEl *hel = hashLookup(uni->matrixHash, geneName);
		if (hel == NULL)
		    errAbort("Can't find %s in %s but it is in %s\n", geneName, uni->matrixFile, ourSource->type);
		int y = ptToInt(hel->val);
		int x;
		for (x=0; x<uni->matrix->xSize; ++x)
		    {
		    fprintf(f, "\t%g", uni->scaleOut*uni->matrix->rows[y][x]);
		    }
		}
	    else
	        {
		int x;
		for (x=0; x<uni->matrix->xSize; ++x)
		    {
		    fprintf(f, "\t%g", emptyVal);
		    }
		}
	    }
	fprintf(f, "\n");
	}
    }
}

void hcaUnifyMatrix(char *graphInput, char *inList, char *outMatrix, char *outBed)
/* hcaUnifyMatrix - Given a list of matrices on maybe different gene sets produce a 
 * unified matrix on one gene set. */
{
struct txGraph *g, *gList = txGraphLoadAll(graphInput);
verbose(1, "Read %d graphs from %s\n", slCount(gList), graphInput);

int typeCount = countDistinctTypes(gList);
int minSources = typeCount/2;
verbose(1, "%d distinct sources of evidence in %s\n", typeCount, graphInput);


struct uniInput *uni, *uniList = uniInputLoadAll(inList);
struct hash *uniHash = hashNew(0);
verbose(1, "Read %d inputs from %s\n", slCount(uniList), inList);
int totalColumns = 0;
for (uni = uniList; uni != NULL; uni = uni->next)
    {
    hashAdd(uniHash, uni->mappingBed, uni);
    loadMappingBed(uni);
    verbose(2, "%d genes in %s\n", uni->bedHash->elCount, uni->mappingBed);
    loadMatrix(uni);
    verbose(2, "%d genes and %d samples in %s\n", 
	uni->matrix->ySize, uni->matrix->xSize, uni->matrixFile);
    totalColumns += uni->matrix->xSize;
    }
verbose(1, "Output will be %d columns\n", totalColumns);

/* Open output and write out header row */
FILE *f = mustOpen(outMatrix, "w");
fprintf(f, "#hcaUnifyMatrix");
for (uni = uniList; uni != NULL; uni = uni->next)
    {
    struct memMatrix *mm = uni->matrix;
    int x;
    for (x=0; x<mm->xSize; ++x)
        {
	fprintf(f, "\t%s %s", uni->name, mm->xLabels[x]);
	}
    }
fprintf(f, "\n");

FILE *bedF = NULL;
if (outBed != NULL)
    bedF = mustOpen(outBed, "w");

/* Go through outputting matrix and bed rows */
for (g = gList; g != NULL; g = g->next)
    {
    /* We only want ones with at least a certain minimum of evidence */
    if (g->sourceCount >= minSources)
	{
	int typeCount = countDistinctSourceTypes(g);
	if (typeCount >= minSources)
	   {
	   writeOneGraph(g, uniList, uniHash, f, bedF);
	   }
	}
    }
carefulClose(&f);
carefulClose(&bedF);

char *trackDb = optionVal("trackDb", NULL);
if (trackDb != NULL)
    {
    outputTrackDb(trackDb, uniList);
    }

char *stats = optionVal("stats", NULL);
if (stats != NULL)
    {
    outputStats(stats, uniList);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
emptyVal = optionDouble("empty", -1);
hcaUnifyMatrix(argv[1], argv[2], argv[3], optionVal("bed", NULL));
return 0;
}
