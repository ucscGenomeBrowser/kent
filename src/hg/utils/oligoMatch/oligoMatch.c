/* oligoMatch - based on the hgTracks track. */

#include "common.h"
#include "dnautil.h"
#include "dnaLoad.h"
#include "bed.h"

#define oligoMatchDefault "aaaaa"

void usage() 
{
errAbort("oligoMatch - find perfect matches in sequence.\n"
	 "usage:\n"
	 "   oligoMatch oligos sequence output.bed\n"
	 "where \"oligos\" and \"sequence\" can be .fa, .nib, or .2bit files.");
}

char *oligoMatchSeq(char *s)
/* Return sequence for oligo matching. */
{
if (s != NULL)
    {
    int len;
    tolowers(s);
    dnaFilter(s, s);
    len = strlen(s);
    if (len < 2)
       s = NULL;
    }
if (s == NULL)
    s = cloneString(oligoMatchDefault);
return s;
}

char *oligoMatchName(struct bed *bed)
/* Return name for oligo, which is just the base position. */
{
char buf[22];
buf[0] = bed->strand[0];
safef(buf+1, ArraySize(buf)-1, "%d", bed->chromStart+1);
return cloneString(buf);
}

struct bed *oligoMatch(struct dnaSeq *target, struct dnaSeq *query)
/* Create list of perfect matches to oligo on either strand. */
{
char *dna = target->dna;
char *fOligo = oligoMatchSeq(query->dna);
int oligoSize = strlen(fOligo);
char *rOligo = cloneString(fOligo);
char *rMatch = NULL, *fMatch = NULL;
struct bed *bedList = NULL, *bed;
char strand;
int count = 0, maxCount = 1000000;

tolowers(dna);
if (oligoSize >= 2)
    {
    fMatch = stringIn(fOligo, dna);
    reverseComplement(rOligo, oligoSize);
    if (sameString(rOligo, fOligo))
        rOligo = NULL;
    else
	rMatch = stringIn(rOligo, dna);
    for (;;)
        {
	char *oneMatch = NULL;
	if (rMatch == NULL)
	    {
	    if (fMatch == NULL)
	        break;
	    else
		{
	        oneMatch = fMatch;
		fMatch = stringIn(fOligo, fMatch+1);
		strand = '+';
		}
	    }
	else if (fMatch == NULL)
	    {

	    oneMatch = rMatch;
	    rMatch = stringIn(rOligo, rMatch+1);
	    strand = '-';
	    }
	else if (rMatch < fMatch)
	    {
	    oneMatch = rMatch;
	    rMatch = stringIn(rOligo, rMatch+1);
	    strand = '-';
	    }
	else
	    {
	    oneMatch = fMatch;
	    fMatch = stringIn(fOligo, fMatch+1);
	    strand = '+';
	    }
	++count;
	AllocVar(bed);
	bed->chrom = cloneString(target->name);
	bed->chromStart = oneMatch - dna;
	bed->chromEnd = bed->chromStart + oligoSize;
	bed->strand[0] = strand;
	bed->name = oligoMatchName(bed);
	slAddHead(&bedList, bed);
	}
    slReverse(&bedList);
    }
return bedList;
}

void outputBed6(struct bed *bedList, char *output)
/* self-explainatory */
{
FILE *outputFile = mustOpen(output, "w");
struct bed *bed = NULL;
for (bed = bedList; bed != NULL; bed = bed->next)
    bedTabOutN(bed, 6, outputFile);
carefulClose(&outputFile);
}

int main(int argc, char *argv[])
/* The program */
{
struct bed *bedList = NULL;
struct dnaSeq *targets = NULL, *target;
struct dnaSeq *queries = NULL, *query;
if (argc != 4)
    usage();
targets = dnaLoadAll(argv[2]);
queries = dnaLoadAll(argv[1]);
for (target = targets; target != NULL; target = target->next)
    for (query = queries; query != NULL; query = query->next)
	{
	struct bed *oneList = oligoMatch(target, query);
	bedList = slCat(bedList, oneList);
	}
outputBed6(bedList, argv[3]);
bedFreeList(&bedList);
dnaSeqFreeList(&targets);
dnaSeqFreeList(&queries);
return 0;
}

