/* Program to match upa bed and a borf and output genePred file. */
#include "common.h"
#include "bed.h"
#include "borf.h"
#include "genePred.h"
#include "obscure.h"
#include "options.h"

void usage()
/* Print usage and quit. */
{
errAbort("borfMatcher - Program that takes a bed and a borf (from borfBig program)\n"
	 "and sets the thickStart and thickEnd for the bed as well as creating\n"
	 "a genePred record as well. Assumes records are in order in files.\n"
	 "usage:\n  "
	 "borfMatcher bedFileIn borfFileIn bedFileOut genePredFileOut\n"
	 "options:\n"
	 "   -keepNmd - Keep proteins that appear to be nmd targets.\n"
	 "   -keepSmall - Keep orfs that are small.\n"
	 "   -minScore - Minimum score to keep, default 50\n");

}

void setThickStartStop(struct bed *bed, struct borf *borf)
/* Use the borf strucutre to insert coding start/stop. */
{
int thickStart = -1, thickEnd =-1;
int baseCount = 0;
int bedSize = 0;
int i=0;
int endDist = 0;
assert(bed);
assert(borf);
/* If we are on the reverse strand, reverse the coordinates. */
if(sameString(bed->strand, "-")) 
    {
    int tmp = 0;
    for(i=0; i < bed->blockCount; i++)
	bedSize += bed->blockSizes[i];
    tmp = bedSize - borf->cdsStart;
    borf->cdsStart = bedSize - borf->cdsEnd;
    borf->cdsEnd = tmp;
    }

/* Place the start/end. */
for(i=0; i<bed->blockCount; i++)
    {
    baseCount += bed->blockSizes[i];
    if(baseCount > borf->cdsStart && thickStart == -1)
	thickStart = bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i] - (baseCount - borf->cdsStart);
    if(baseCount >= borf->cdsEnd && thickEnd == -1)
	thickEnd = bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i] - (baseCount - borf->cdsEnd) ;
    }
bed->thickStart = max(0,thickStart);
bed->thickEnd = max(0,thickEnd);
}

boolean nmdTarget(struct bed *bed) 
/* Return TRUE if cds end is more than 50bp upstream of
   last intron. */
{
int bedSize = 0;
int i = 0;
int startDist = 0, endDist = 0;
if(bed->blockCount < 2)
    return TRUE;
for(i = 0; i < bed->blockCount; i++)
    {
    int blockStart = bed->chromStart + bed->chromStarts[i];
    int blockEnd = blockStart + bed->blockSizes[i];
    int blockSize = bed->blockSizes[i];
    if( blockStart <= bed->thickStart && bed->thickStart <= blockEnd)
	startDist += blockSize - (blockEnd - bed->thickStart);
    else if(blockEnd <= bed->thickStart)
	startDist += blockSize;
    if( blockStart <= bed->thickEnd && bed->thickEnd <= blockEnd)
	endDist += blockSize - (blockEnd - bed->thickEnd);
    else if(blockEnd <= bed->thickEnd)
	endDist += blockSize;

    bedSize += blockSize;
    }

if(sameString(bed->strand, "+"))
    return endDist < (bedSize - (bed->blockSizes[bed->blockCount - 1] + 50));
else if(sameString(bed->strand, "-"))
    return startDist > (bed->blockSizes[0] + 50);
else 
    errAbort("What strand is %s\n", bed->strand);
return FALSE;
}
	    
void testNmd()
{
struct bed *bed = NULL;
AllocVar(bed);
bed->name = "test";
bed->chrom = "test";
bed->chromStart = 1000;
bed->chromEnd = 2000;
bed->blockCount = 3;
AllocArray(bed->chromStarts, bed->blockCount);
AllocArray(bed->blockSizes, bed->blockCount);
bed->strand[0] = '+';
bed->chromStarts[1] = 500;
bed->chromStarts[2] = 800;
bed->blockSizes[0] = 100; 
bed->blockSizes[1] = 100;
bed->blockSizes[2] = 200;
bed->thickStart = 1050;
bed->thickEnd = 1549;
assert(nmdTarget(bed) == TRUE);
bed->strand[0] = '-';
assert(nmdTarget(bed) == FALSE);
bed->thickEnd = 1550;
assert(nmdTarget(bed) == FALSE);
bed->strand[0] = '+';
assert(nmdTarget(bed) == FALSE);
bed->strand[0] = '-';
bed->thickStart = 1551;
bed->thickEnd = 1800;
assert(nmdTarget(bed) == TRUE);
warn("NmdTests passed");
}

void borfMatcher(char *bedIn, char *borfIn, char *bedOutFile, char *genePredOutFile)
/* Top level function to open files and call other functions. */
{
struct borf *borf = NULL, *borfList = NULL;
struct bed *bed = NULL, *bedList = NULL;
struct genePred *gp = NULL;
float threshold = optionFloat("minScore", 50);
FILE *bedOut = mustOpen(bedOutFile, "w");
FILE *genePredOut = mustOpen(genePredOutFile, "w");
boolean keepSmall = optionExists("keepSmall");
boolean keepNmd = optionExists("keepNmd");

borfList = borfLoadAll(borfIn);
bedList = bedLoadAll(bedIn);
dotForUserInit(slCount(bedList)/10);
for(bed = bedList, borf = borfList; bed != NULL && borf != NULL; bed = bed->next, borf = borf->next)
    {
    dotForUser();
    if(!stringIn(bed->name, borf->name))
	errAbort("Trying to match up %s bed with %s borf - bad idea!", bed->name, borf->name);
    /* Have to adjust cds end. Borf puts stop codon outside of cds, 
       we put it inside. */
    borf->cdsEnd = min(borf->cdsEnd+3, borf->size);
    if((borf->score > threshold || (keepSmall && borf->cdsSize > 0)) && sameString(borf->strand, "+"))
	{
	setThickStartStop(bed, borf);
	if(keepNmd || !nmdTarget(bed))
	    {
	    gp = bedToGenePred(bed);
	    bedTabOutN(bed, 12, bedOut);
	    genePredTabOut(gp, genePredOut);
	    genePredFree(&gp);
	    }
	}
    }
warn("Done.");
carefulClose(&bedOut);
carefulClose(&genePredOut);
}

int main(int argc, char *argv[])
/* Everybody's favorite function. */
{
optionInit(&argc, argv, NULL);
if(argc != 5)
    usage();
testNmd();
borfMatcher(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
