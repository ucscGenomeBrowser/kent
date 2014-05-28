/* oligoMatch - based on the hgTracks track. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "dnautil.h"
#include "dnaLoad.h"
#include "bed.h"
#include "iupac.h"

#define oligoMatchDefault "aaaaa"

void usage() 
{
errAbort(
"oligoMatch - find perfect matches in sequence.\n"
"usage:\n"
"   oligoMatch oligos sequence output.bed\n"
"where \"oligos\" and \"sequence\" can be .fa, .nib, or .2bit files.\n"
"The oligos may contain IUPAC codes.\n"
);
}

char *oligoMatchSeq(char *s)
/* Return sequence for oligo matching. */
{
if (s != NULL)
    {
    int len;
    tolowers(s);
    iupacFilter(s, s);
    len = strlen(s);
    if (len < 2)
       s = NULL;
    }
if (s == NULL)
    s = cloneString(oligoMatchDefault);
return s;
}

char *oligoMatchName(struct bed *bed, char *queryName)
/* Return name for oligo, which is just the base position. */
{
char buf[22];
buf[0] = bed->strand[0];
safef(buf+1, ArraySize(buf)-1, "%d", bed->chromStart+1);
// because oligoMatch utility supports multiple query oligos, 
// include the oligo name in the output.
char result[1024];
safef(result, sizeof result, "%s%s", queryName, buf);
return cloneString(result);
}

char *stringInWrapper(char *needle, char *haystack)
/* Wrapper around string in to make it so it's a function rather than a macro. */
{
return stringIn(needle, haystack);
}


struct bed *oligoMatch(struct dnaSeq *target, struct dnaSeq *query)
/* Create list of perfect matches to oligo on either strand. */
{
char *dna = target->dna;
char *fOligo = oligoMatchSeq(query->dna);
char *(*finder)(char *needle, char *haystack) = (anyIupac(fOligo) ? iupacIn : stringInWrapper);
int oligoSize = strlen(fOligo);
char *rOligo = cloneString(fOligo);
char *rMatch = NULL, *fMatch = NULL;
struct bed *bedList = NULL, *bed;
char strand;
int count = 0;

tolowers(dna);
if (oligoSize >= 2)
    {
    fMatch = finder(fOligo, dna);
    iupacReverseComplement(rOligo, oligoSize);
    if (sameString(rOligo, fOligo))
        rOligo = NULL;
    else
	rMatch = finder(rOligo, dna);
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
		fMatch = finder(fOligo, fMatch+1);
		strand = '+';
		}
	    }
	else if (fMatch == NULL)
	    {

	    oneMatch = rMatch;
	    rMatch = finder(rOligo, rMatch+1);
	    strand = '-';
	    }
	else if (rMatch < fMatch)
	    {
	    oneMatch = rMatch;
	    rMatch = finder(rOligo, rMatch+1);
	    strand = '-';
	    }
	else
	    {
	    oneMatch = fMatch;
	    fMatch = finder(fOligo, fMatch+1);
	    strand = '+';
	    }
	++count;
	AllocVar(bed);
	bed->chrom = cloneString(target->name);
	bed->chromStart = oneMatch - dna;
	bed->chromEnd = bed->chromStart + oligoSize;
	bed->strand[0] = strand;
	bed->name = oligoMatchName(bed, query->name);
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

