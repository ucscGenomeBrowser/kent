/* subsetRef - Make BED track that is a subset of reference sequence. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "hdb.h"
#include "jksql.h"
#include "oldRefAlign.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "subsetRef - Make BED track that is a subset of reference sequence\n"
  "usage:\n"
  "   subsetRef database chromosome refTrack match mismatch gap gapExt newTrack\n"
  "example:\n"
  "   subsetRef hg7 chr22 mouseRef 10 20 80 20 mouseTight\n"
  );
}

int num(char *s)
/* Convert s to a number.  Squawk and die if it isn't. */
{
if (!isdigit(s[0]))
    errAbort("Expecting number, got %s", s);
return atoi(s);
}

struct refAlign *readRefAli(char *chrom, char *refTrack)
/* Read in reference alignment for one chromosome. */
{
struct sqlConnection *conn = hAllocConn();
struct refAlign *aliList = NULL, *ali;
int rowOffset;
struct sqlResult *sr;
char **row;

sr = hChromQuery(conn, refTrack, chrom, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ali = refAlignLoad(row+rowOffset);
    if (ali->chromEnd - ali->chromStart < 255)	// uglyf patch
    slAddHead(&aliList, ali);
    }
hFreeConn(&conn);
slReverse(&aliList);
return aliList;
}

int countNonDash(char *a, int size)
/* Count number of non-dash characters. */
{
int count = 0;
int i;
for (i=0; i<size; ++i)
    if (a[i] != '-') 
        ++count;
return count;
}

struct refAlign *clumpAdjacent(struct refAlign *startAli)
/* Return first ali in list that isn't adjacent with start of list.
 * (This may include NULL at end.) */
{
struct refAlign *lastAli = startAli, *ali;
for (ali = startAli->next; ali != NULL; ali = ali->next)
    {
    assert(ali->chromEnd - ali->chromStart == countNonDash(ali->humanSeq, strlen(ali->humanSeq)));
    if (ali->chromStart != lastAli->chromEnd)
        return ali;
    lastAli = ali;
    }
return NULL;
}

void mergeSeq(struct refAlign *startAli, struct refAlign *endAli,
	struct dyString *aSeq, struct dyString *hSeq)
/* Merge all ali sequences into  dyStrings. */
{
struct refAlign *ali;
dyStringClear(aSeq);
dyStringClear(hSeq);
for (ali = startAli; ali != endAli; ali = ali->next)
    {
    dyStringAppend(aSeq, ali->alignSeq);
    dyStringAppend(hSeq, ali->humanSeq);
    }
}

boolean findHsp(char *as, char *bs, int size, 
	int match, int mismatch, int gapStart, int gapExt, int minScore,
	int *retStart, int *retEnd, int *retScore)
/* Find first HSP between a and b. */
{
int start = 0, best = 0, bestScore = 0, end;
int score = 0;
char a,b;
boolean lastGap = FALSE;

for (end = 0; end < size; ++end)
    {
    a = as[end];
    b = bs[end];
    if (a == '-' || b == '-')
        {
	if (lastGap)
	    score -= gapExt;
	else
	    {
	    score -= gapStart;
	    lastGap = TRUE;
	    }
	}
    else
        {
	if (a == b)
	    score += match;
	else
	    score -= mismatch;
	lastGap = FALSE;
	}
    if (score > bestScore)
        {
	bestScore = score;
	best = end;
	}
    if (score < 0 || end == size-1)
        {
	if (bestScore >= minScore)
	    {
	    *retStart = start;
	    *retEnd = best+1;
	    *retScore = bestScore;
	    return TRUE;
	    }
	start = end+1;
	}
    }
return FALSE;
}

void outputHsp(FILE *f, char *chrom, int start, int end, int score)
/* Output one HSP to bed file */
{
static int id = 0;
fprintf(f, "%s\t%d\t%d\t%s.%04d\t%d\n",
	chrom, start, end, chrom, ++id, score);
}

void outputBed(FILE *f, char *a, char *h, int size, int hOffset,
    char *chrom, int match, int mismatch, int gap, int gapExt)
{
int start, end, score;
int offset = 0;

while (findHsp(a + offset, h + offset, size - offset, 
	match, mismatch, gap, gapExt, 10*match, &start, &end, &score))
    {
    assert(start < end);
    assert(end <= size - offset);
    if (end - start > 100)
	{
	int hStart = hOffset + countNonDash(h, start+offset);
	int hEnd = hStart + countNonDash(h+start+offset, end-start);
	if (hEnd - hStart > 100)
	    outputHsp(f, chrom, hStart, hEnd, score);
	}
    offset += end;
    if (offset == size)
        break;
    }
}

void writeSubset(FILE *f, struct refAlign *aliList, char *chrom,
	int match, int mismatch, int gap, int gapExt)
/* Write a subset of matches to file. */
{
struct refAlign *startAli, *endAli, *ali;
struct dyString *aSeq = newDyString(16*1024);
struct dyString *hSeq = newDyString(16*1024);

for (startAli = aliList; startAli != NULL; startAli = endAli)
    {
    endAli = clumpAdjacent(startAli);
    mergeSeq(startAli, endAli, aSeq, hSeq);
    assert(aSeq->stringSize == hSeq->stringSize);
    outputBed(f, aSeq->string, hSeq->string, aSeq->stringSize, 
    	startAli->chromStart, chrom, match, mismatch, gap, gapExt);
    }
}


void dumpAliList(struct refAlign *aliList, FILE *f)
{
struct refAlign *ali;
int i, size;

for (ali = aliList; ali != NULL; ali = ali->next)
    {
    size = strlen(ali->alignSeq);
    fprintf(f, "%s:%d-%d\n", ali->chrom, ali->chromStart, ali->chromEnd);
    fprintf(f, "%s\n", ali->alignSeq);
    for (i=0; i<size; ++i)
        {
	if (ali->alignSeq[i] == ali->humanSeq[i])
	    fputc('|', f);
	else
	    fputc(' ', f);
	}
    fputc('\n', f);
    fprintf(f, "%s\n", ali->humanSeq);
    fputc('\n', f);
    }
}

void subsetRef(char *database, char *chrom, char *refTrack, 
	char *sMatch, char *sMismatch, char *sGap, char *sGapExt, char *newTrack)
/* subsetRef - Make BED track that is a subset of reference sequence. */
{
int match = num(sMatch);
int mismatch = num(sMismatch);
int gap = num(sGap);
int gapExt = num(sGapExt);
struct refAlign *aliList = NULL, *ali;
char *tabFile = "temp.tab";
FILE *f = mustOpen(tabFile, "w");

hSetDb(database);
aliList = readRefAli(chrom, refTrack);
uglyf("Read %d refAlignments\n", slCount(aliList));
writeSubset(f, aliList, chrom, match, mismatch, gap, gapExt);
carefulClose(&f);
uglyAbort("Check out %s, all for now", tabFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 9)
    usage();
subsetRef(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8]);
return 0;
}

