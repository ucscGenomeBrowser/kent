/* axtCountBeds.c - Program to count matching bases in bed and output positional information as well. */
#include "common.h"
#include "axt.h"
#include "rbTree.h"
#include "bed.h"
#include "linefile.h"
#include "hdb.h"
#include "dnaseq.h"
#include "dnautil.h"

void usage()
{
errAbort("axtCountBeds bedFile axtFile db outFile chrom\n");
}

struct axt* axtReadAll(char* fileName)
/* Read all of the axt records from a file. */
{
struct axt *axtList = NULL;
struct axt *rec;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
while ((rec = axtRead(lf)) != NULL)
    {
    slAddHead(&axtList, rec);
    }
lineFileClose(&lf);
if (axtList == NULL)
    errAbort("no alignments found in %s", fileName);
slReverse(&axtList);
return axtList;
}

int axtRangeCmp(void *va, void *vb)
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */
{
struct axt *a = va;
struct axt *b = vb;
if (a->tEnd <= b->tStart)
    return -1;
else if (b->tEnd <= a->tStart)
    return 1;
else
    return 0;
}

int voidAxtCmpTarget(void *a, void *b)
{
return axtCmpTarget(&a,&b);
}

struct rbTree *rbTreeFromAxt(struct axt *axtList)
/* Build an rbTree from a list of axts. */
{
struct rbTree *rbTree = rbTreeNew(axtRangeCmp);
struct axt *axt = NULL;
for(axt = axtList; axt != NULL; axt = axt->next)
    {
    rbTreeAdd(rbTree, axt);
    }
return rbTree;
}

struct rbTree *axtFileToRbTree(char *fileName)
/* Read in all the axts from a file and put them in a rbTree. */
{
struct rbTree *rbTree = NULL;
struct axt *axtList = NULL;
axtList = axtReadAll(fileName);
rbTree = rbTreeFromAxt(axtList);
return rbTree;
}


struct axt *axtInRbTree(struct rbTree *rbTree, char *chrom, int chromStart, int chromEnd)
/* Return a list of axts that span a given region on a given chromosome. */
{
struct axt *minItem = NULL, *maxItem = NULL;
struct slRef *refList = NULL, *ref = NULL;
struct axt *axtList = NULL;
/* Create items to search rbTree. */
AllocVar(minItem);
minItem->tName = chrom;
minItem->tStart = chromStart-1;
minItem->tEnd = chromStart-1; 
AllocVar(maxItem);
maxItem->tName = chrom;
maxItem->tStart = chromEnd+1;
maxItem->tEnd = chromEnd+1; 

/* Construct axtList. */
refList = rbTreeItemsInRange(rbTree, minItem, maxItem);
for(ref = refList; ref != NULL; ref = ref->next)
    {
    slSafeAddHead(&axtList, ((struct slList *)ref->val));
    }
slReverse(&axtList);
slFreeList(&refList);
return axtList;
}

void printIntArray(int *array, int count)
{
int i;
for(i = 0; i < count; i++)
    {
    fprintf(stderr, "%d,", array[i]);
    }
fprintf(stderr, "\n");
fflush(stderr);
}

void scoreMatches(struct axt *axt, int *matches, char *dna, int matchCount, int chromStart, int chromEnd)
/* Look through the axt record and score mathes in matches. */
{
int start = chromStart - axt->tStart;
int end = min(chromEnd - axt->tStart, axt->tEnd = axt->tStart);
int i =0;
int position = 0;
toLowerN(axt->qSym, axt->symCount);
toLowerN(axt->tSym, axt->symCount);
while(position < end && axt->qSym[i] != '\0')
    {
    if(axt->tSym[i] == '-')
	{
	i++;
	continue;
	}
    else if(position >= start && position <end)
	{
	dna[position + axt->tStart - chromStart] = axt->tSym[i];
	if(axt->qSym[i] == axt->tSym[i])
	    matches[position + axt->tStart - chromStart] = 1;
	else if(axt->qSym[i] != axt->tSym[i])
	    matches[position + axt->tStart - chromStart] = 0;
	else if(axt->qSym[i] == '-' || axt->tSym[i] == '-')
	    matches[position + axt->tStart - chromStart] = -1;
	else 
	    errAbort("axtCountBeds::scoreMatches() - Got %c and %c for qSym and tSym. "
		     "ChromStart %d, chromEnd %d, offSet %d",
		     axt->qSym[i], axt->tSym[i], chromStart, chromEnd, i);
	
	}
    i++;
    position++;
    }
}

void initMatches(int *matches, int count, int val)
{
int i;
for(i=0; i<count; i++)
    matches[i] = val;
}

void outputHeader(FILE *out)
{
fprintf(out,"%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
	"chrom", "chromStart", "chromEnd", "name", "score", "strand", 
	"match", "mismatch", "notAlign", "percentId", "dna", "matchesSize", "matches");
}

void countMatches(struct rbTree *rbTree, struct bed *bed, FILE *out)
/* Count up the matches for a particular bed. */
{
struct axt *axtList = NULL, *axt;
int *matches = NULL;
int matchSize = 0, i = 0;
int match = 0, misMatch=0, notAlign=0;
char *dna = NULL;
struct dnaSeq *seq = NULL;
matchSize = bed->chromEnd - bed->chromStart;
AllocArray(matches, matchSize);
AllocArray(dna, matchSize+1);
initMatches(matches, matchSize, -1);
/* for(i=0; i<matchSize; i++) */
/*     dna[i] = 'n'; */
axtList = axtInRbTree(rbTree, bed->chrom, bed->chromStart, bed->chromEnd);
for(axt = axtList; axt != NULL; axt = axt->next)
    {
    scoreMatches(axt, matches, dna, matchSize, bed->chromStart, bed->chromEnd);
    } 
/* Output bed. */
bedOutputN(bed, 6, out, '\t', '\t');
/* Gather info for percent id */
for(i = 0; i < matchSize; i++)
    {
    if(matches[i] == 1)
	match++;
    else if(matches[i] == 0)
	misMatch++;
    else if(matches[i] == -1)
	notAlign++;
    else
	errAbort("agxCountBeds::countMatches() - matches array at position %d is: %d, not 1, 0, or -1",
		 i, matches[i]);
    }
/* Get sequence. */
seq = hChromSeq(bed->chrom, bed->chromStart, bed->chromEnd);
if(strlen(dna) == seq->size && differentString(seq->dna, dna) && axtList != NULL) 
    warn("axtCountBeds::countMatches() - Different sequences from coordinates and axt file.:\ncoord: %s\naxt:    %s\n",
	 seq->dna, dna);
if(sameString(bed->strand, "-"))
    reverseComplement(seq->dna, seq->size);
fprintf(out, "%d\t%d\t%d\t%f\t",  match, misMatch, notAlign, (float)match/(match+misMatch));
fprintf(out, "%s\t", seq->dna);
fprintf(out, "%d\t", matchSize);
if(sameString(bed->strand, "-"))
    {
    for(i=matchSize-1; i >= 0; i--)
	fprintf(out, "%d,", matches[i]);
    }
else
    {
    for(i=0; i<matchSize; i++)
	fprintf(out, "%d,", matches[i]);
    }
/* for(i=0; i<matchSize; i++) */
/*     fprintf(out, "%d,", matches[i]); */
fprintf(out, "\n");
freez(&matches);
freez(&dna);
dnaSeqFree(&seq);
}


void axtCountBeds(char *bedFile, char *axtFile, char *db, char *outFile, char *chrom)
{
struct bed *bedList = NULL, *bed = NULL;
struct rbTree *axtTree = NULL;
FILE *out = NULL;
warn("Loading Beds.");
bedList = bedLoadNAll(bedFile, 6);
warn("Loading Axts.");
axtTree = axtFileToRbTree(axtFile);
warn("Counting Matches.");
out = mustOpen(outFile, "w");
outputHeader(out);
hSetDb(db);
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    if(differentString(bed->chrom, chrom))
	continue;
    countMatches(axtTree, bed, out);
    }
carefulClose(&out);
warn("Done.");
}

int main(int argc, char *argv[])
{
if(argc <= 2)
    usage();
axtCountBeds(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
