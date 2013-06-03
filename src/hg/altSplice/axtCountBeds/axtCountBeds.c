/* axtCountBeds.c - Program to count matching bases in bed and output positional information as well. */
#include "common.h"
#include "axt.h"
#include "bed.h"
#include "linefile.h"
#include "hdb.h"
#include "dnaseq.h"
#include "dnautil.h"
#include "chromKeeper.h"
#include "options.h"
#include "obscure.h"

void usage()
{
errAbort("axtCountBeds.c - Program to count matching bases in bed and output\n"
	 "positional information as well.\n"
	 "usage:\n   "
	 "axtCountBeds bedFile axtFile db outFile chrom\n");
}

struct axt *axtList = NULL;
boolean ckInit = FALSE;

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
struct axt *axt = NULL;
axtList = axtReadAll(fileName);
if(!ckInit)
    {
    chromKeeperInit(db);
    ckInit = TRUE;
    }
for(axt = axtList; axt != NULL; axt = axt->next)
    {
    chromKeeperAdd(axt->tName, axt->tStart, axt->tEnd, axt);
    }
}

void cleanUpChromKeeper()
/* Clean stuff out of the chromkeeper. */
{
struct axt *axt = NULL, *axtNext = NULL;
for(axt = axtList; axt != NULL; axt = axtNext)
    {
    axtNext = axt->next;
    chromKeeperRemove(axt->tName, axt->tStart, axt->tEnd, axt);
    }
axtFreeList(&axtList);
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
return (strchr("atgcATGC", c)) != NULL;
}

void scoreMatches(struct axt *axt, int *matches, char *dna, int matchCount, int chromStart, int chromEnd)
/* Look through the axt record and score mathes in matches. */
{
int start = chromStart - axt->tStart;
int end = min(chromEnd - axt->tStart, axt->tEnd - axt->tStart);
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
    else if(position >= start && position < end)
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

void writeAxt(FILE *out, struct bed *bed, struct binElement *beList, boolean qrnaStyle)
{
struct dyString *outSeq = newDyString(bed->chromEnd - bed->chromStart+100);
struct dyString *outSeqOrtho = newDyString(bed->chromEnd - bed->chromStart+100);
int currentStart = bed->chromStart;
int currentEnd = bed->chromEnd;
struct binElement *be = NULL;
fprintf(out, ">%s.%d.%s.%s:%d-%d\n", 
	bed->name, bed->score, bed->strand, 
	bed->chrom, bed->chromStart,bed->chromEnd);
for(be = beList; be != NULL; be = be->next)
    {
    struct axt *axt = be->val;
    int start = currentStart - axt->tStart;
    int end = 0;//max(currentEnd - axt->tStart, axt->tEnd - currentStart);
    int pos = 0;
    int i = 0;
    struct dnaSeq *seq = NULL;
    end = min(axt->tEnd - axt->tStart, currentEnd - axt->tStart);
    if(bed->chromEnd >= axt->tStart && bed->chromStart <= axt->tEnd)
	{
	/* If axt doesn't cover, pad things with genomic dna. */
	if(start < 0)
	    {
	    seq = hChromSeq(bed->chrom, currentStart, axt->tStart);
	    for(i = 0; i < abs(start); i++)
		{
		dyStringPrintf(outSeq, "%c", seq->dna[i]);
		dyStringPrintf(outSeqOrtho, "-");
		}
	    dnaSeqFree(&seq);
	    currentStart = axt->tStart;
	    start = 0;
	    }
	i = 0;
	/* Loop through incrementing position when non-indel characters
	   are found in target sequence. */
	while(pos < end && axt->qSym[i] != '\0') 
	    {
	    if(pos >= start && pos < end)
		{
		dyStringPrintf(outSeq, "%c", axt->tSym[i]);
		dyStringPrintf(outSeqOrtho, "%c", axt->qSym[i]);
		if(axt->tSym[i] != '-')
		    currentStart++;
		}
	    if(axt->tSym[i] != '-')
		{
		pos++;
		}
	    i++;
	    }
	}
    }
/* Pad out the end with sequence. */
if(currentStart < bed->chromEnd)
    {
    struct dnaSeq *seq = NULL;
    int i = 0;
    seq = hChromSeq(bed->chrom, currentStart, bed->chromEnd);
    for(i = 0; i < bed->chromEnd - currentStart; i++)
	{
	dyStringPrintf(outSeq, "%c", seq->dna[i]);
	dyStringPrintf(outSeqOrtho, "-");
	}
    dnaSeqFree(&seq);
    }
if(bed->strand[0] == '-')
    {
    reverseComplement(outSeq->string, outSeq->stringSize);
    reverseComplement(outSeqOrtho->string, outSeqOrtho->stringSize);
    }

if(qrnaStyle)
    {
    writeSeqWithBreaks(out, outSeq->string, outSeq->stringSize, 50);
    fprintf(out, ">%s.%d.%s.%s:%d-%d-Ortho\n", 
	    bed->name, bed->score, bed->strand, 
	    bed->chrom, bed->chromStart,bed->chromEnd);
    writeSeqWithBreaks(out, outSeqOrtho->string, outSeqOrtho->stringSize, 50);
    }
else
    {
    fprintf(out, "%s\n", outSeq->string);
    fprintf(out, "%s\n", outSeqOrtho->string);
    }
dyStringFree(&outSeq);
dyStringFree(&outSeqOrtho);
}

void countMatches(struct bed *bed, FILE *out, FILE *axtOut, FILE *qrnaOut)
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

if(axtOut != NULL && slCount(beList) != 0 )
    writeAxt(axtOut, bed, beList, FALSE);

if(qrnaOut != NULL && slCount(beList) != 0)
    writeAxt(qrnaOut, bed, beList, TRUE);

else
    {
    warn("Nothing for %s %s:%d-%d", bed->name, bed->chrom, bed->chromStart, bed->chromEnd);
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
FILE *axtOut = NULL;
FILE *qrnaOut = NULL;
char *axtOutName = optionVal("axtOut", NULL);
char *qrnaOutName = optionVal("qrnaOut", NULL);
struct sqlConnection *conn = NULL;
struct sqlResult *sr = NULL;
char **row;
char query[256];
struct dyString *axtChromFile = NULL;

warn("Loading Beds.");
bedList = bedLoadAll(bedFile);
hSetDb(db);
conn = hAllocConn();
if(sameString(chrom, "all"))
    sqlSafef(query, sizeof(query), "select chrom from chromInfo where chrom like '%%' order by chrom;");
else
    sqlSafef(query, sizeof(query), "select chrom from chromInfo where chrom like '%s';", chrom);
sr = sqlGetResult(conn, query);
out = mustOpen(outFile, "w");
outputHeader(out);
axtChromFile = newDyString(strlen(axtFile)+100);
if(axtOutName != NULL)
    axtOut = mustOpen(axtOutName, "w");
if(qrnaOutName != NULL)
    qrnaOut = mustOpen(qrnaOutName, "w");
dotForUserInit(1);
warn("Reading axts.");
while((row = sqlNextRow(sr)) != NULL)
    {
    dyStringClear(axtChromFile);
    dyStringPrintf(axtChromFile, "%s/%s.axt", axtFile, row[0]);
    /* If the .axt isn't there try to find a gzipped version. */
    if(!fileExists(axtChromFile->string)) 
	{
	dyStringClear(axtChromFile);
	dyStringPrintf(axtChromFile, "%s/%s.axt.gz", axtFile, row[0]);
	}
    dotForUser();
    chromKeeperForAxt(axtChromFile->string, db);
    for(bed = bedList; bed != NULL; bed = bed->next)
	{
	if(sameString(bed->strand,""))
	    bed->strand[0] = '+';
	if(differentString(bed->chrom, row[0]))
	    continue;
	countMatches(bed, out, axtOut, qrnaOut);
	}
    cleanUpChromKeeper();
    }
carefulClose(&out);
carefulClose(&axtOut);
warn("\nDone.");
}

int main(int argc, char *argv[])
{
optionInit(&argc, argv, NULL);
if(argc <= 2)
    usage();
dnaUtilOpen();
axtCountBeds(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
