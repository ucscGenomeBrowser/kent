/* bedSpliceCheck.c - See if beds have valid splice sites on genome.
   That is consensus GT-AG, or GC-AG.
*/
#include "common.h"
#include "hdb.h"
#include "dnaseq.h"
#include "bed.h"
#include "dnautil.h"
#include "options.h"
#include "obscure.h"

int intronCount = 0;    /* Number with valid introns. */
int noIntroncount = 0;  /* Number without valid introns. */

void usage()
/* Print Usage and quit. */
{
errAbort("bedSpliceCheck - Check to see if some beds contain invalid splice\n"
	 "sites, that is other than the consensus GT-AG or rarer GC-AG\n"
	 "usage:\n"
	 "   bedSpliceCheck db bedFile goodFile rejectsFile\n");
}

boolean isGoodIntron(struct dnaSeq *targetSeq, 
    int start, int end,  boolean isRev)
/* Return true if there's a good intron between a and b. If true will update strand.*/
{
DNA *iStart = targetSeq->dna + start;
DNA *iEnd = targetSeq->dna + end;
if (isRev)
    {
    if (iStart[0] == 'c' && iStart[1] == 't' && iEnd[-2] == 'a' && iEnd[-1] == 'c')
	return TRUE;
    if (iStart[0] == 'c' && iStart[1] == 't' && iEnd[-2] == 'g' && iEnd[-1] == 'c')
	return TRUE;
    }
else
    {
    if (iStart[0] == 'g' && iStart[1] == 't' && iEnd[-2] == 'a' && iEnd[-1] == 'g')
	return TRUE;
    if (iStart[0] == 'g' && iStart[1] == 'c' && iEnd[-2] == 'a' && iEnd[-1] == 'g')
	return TRUE;
    }
return FALSE;
}

void bedSpliceCheck(char *db, char *bedFile, char *goodFile, char *rejectFile)
/* Loop through beds checking for invalid splicings. */
{
struct bed *bed = NULL, *bedList = NULL;
struct dnaSeq *seq = NULL;
FILE *rejectOut = NULL;
FILE *goodOut = NULL;
int offSet =0;
int i = 0;
dotForUser(1000);
hSetDb(db);
warn("Reading beds from %s", bedFile);
bedList = bedLoadAll(bedFile);
rejectOut = mustOpen(rejectFile, "w");
goodOut = mustOpen(goodFile, "w");
warn("Writing out beds with invalid introns to %s", rejectFile);
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    boolean good = TRUE;
    dotForUser();
    seq = hChromSeq(bed->chrom, bed->chromStart, bed->chromEnd);
    offSet = bed->chromStart;
    for(i = 0; i < bed->blockCount - 1; i++)
	{
	if(!isGoodIntron(seq, bed->chromStarts[i]+bed->blockSizes[i], bed->chromStarts[i+1], bed->strand[0] == '-'))
	    {
	    good = FALSE;
	    bedTabOutN(bed, 12, rejectOut);
	    dnaSeqFree(&seq);
	    break;
	    }
	}
    if(good)
	bedTabOutN(bed, 12, goodOut);
    }
carefulClose(&rejectOut);
carefulClose(&goodOut);
warn("\nDone.");
}

    
int main(int argc, char *argv[])
{
if(argc != 5)
    usage();
dnaUtilOpen();
bedSpliceCheck(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
