/* axtCountBeds.c - Program to count matching bases in bed and output positional information as well. */
#include "common.h"
#include "axt.h"
#include "bed.h"
#include "linefile.h"
#include "hdb.h"
#include "dnaseq.h"
#include "dnautil.h"
#include "chromKeeper.h"
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

void chromKeeperForAxt(char *fileName, char *db)
{
struct axt *axtList = NULL, *axt = NULL;
axtList = axtReadAll(fileName);
chromKeeperInit(db);
for(axt = axtList; axt != NULL; axt = axt->next)
    {
    chromKeeperAdd(axt->tName, axt->tStart, axt->tEnd, axt);
    }
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

boolean isDna(char c)
{
return (strchr("atgc", c)) != NULL;
}

void scoreMatches(struct axt *axt, int *matches, char *dna, int matchCount, int chromStart, int chromEnd)
/* Look through the axt record and score mathes in matches. */
{
int start = chromStart - axt->tStart;
int end = min(chromEnd - axt->tStart, axt->tEnd = axt->tStart);
int i =0;
int position = 0;
tolowers(axt->qSym);
tolowers(axt->tSym);
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
	else if((axt->qSym[i] != axt->tSym[i]) && isDna(axt->tSym[i])
		&& isDna(axt->qSym[i]))
	    matches[position + axt->tStart - chromStart] = 0;
	else if(axt->qSym[i] == '-' || axt->tSym[i] == '-' ||
		axt->qSym[i] == 'n' || axt->tSym[i] == 'n')
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

void countMatches(struct bed *bed, FILE *out)
/* Count up the matches for a particular bed. */
{
struct axt *axtList = NULL, *axt;
struct binElement *beList = NULL, *be = NULL;
int *matches = NULL;
int matchSize = 0, i = 0;
int match = 0, misMatch=0, notAlign=0;
struct slRef *refList = NULL, *ref = NULL;
char *dna = NULL;
struct dnaSeq *seq = NULL;
matchSize = bed->chromEnd - bed->chromStart;
AllocArray(matches, matchSize);
AllocArray(dna, matchSize+1);
initMatches(matches, matchSize, -1);
beList = chromKeeperFind(bed->chrom, bed->chromStart, bed->chromEnd);
for(be = beList; be != NULL; be = be->next)
    {
    axt = be->val;
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
if(strlen(dna) == seq->size && differentString(seq->dna, dna) && refList != NULL) 
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
slFreeList(&beList);
}


void axtCountBeds(char *bedFile, char *axtFile, char *db, char *outFile, char *chrom)
{
struct bed *bedList = NULL, *bed = NULL;
struct rbTree *axtTree = NULL;
FILE *out = NULL;
warn("Loading Beds.");
bedList = bedLoadNAll(bedFile, 6);
warn("Loading Axts.");
chromKeeperForAxt(axtFile, db);
warn("Counting Matches.");
out = mustOpen(outFile, "w");
outputHeader(out);
hSetDb(db);
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    if(differentString(bed->chrom, chrom))
	continue;
    countMatches(bed, out);
    }
carefulClose(&out);
warn("Done.");
}

int main(int argc, char *argv[])
{
if(argc <= 2)
    usage();
dnaUtilOpen();
axtCountBeds(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
