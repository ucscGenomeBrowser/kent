#include "common.h"
#include "bed.h"
#include "options.h"

boolean cdsOnly = FALSE;

void usage() 
{
errAbort("bedToExons - Split a bed up into individual beds.\n"
	 "One for each internal exon.\n"
	 "usage:\n   "
	 "bedToExons originalBeds.bed splitBeds.bed\n"
	 "options:\n"
	 "   -cdsOnly - Only output the coding portions of exons.\n");
}

void bedToExons(char *fileIn, char *fileOut)
/* Split up each bed into individual internal exons and write them
   out. */
{
struct bed *bed = NULL, *bedList = bedLoadAll(fileIn);
FILE *out = mustOpen(fileOut, "w");
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    int i = 0;
    if(bed->blockCount == 0)
	{
	if(cdsOnly)
	    fprintf(out, "%s\t%d\t%d\t%s\t%d\t%s\n",
		    bed->chrom, bed->thickStart,
		    bed->thickStart,
		    bed->name, bed->score, bed->strand);
	else
	    fprintf(out, "%s\t%d\t%d\t%s\t%d\t%s\n",
		    bed->chrom, bed->chromStart,
		    bed->chromStart,
		    bed->name, bed->score, bed->strand);
	}
    else
	{
	for(i = 0; i < bed->blockCount; i++)
	    {
	    int blockStart = bed->chromStart + bed->chromStarts[i];
	    int blockEnd = blockStart + bed->blockSizes[i];
	    if(cdsOnly)
		{
		if(blockEnd < bed->thickStart || blockStart > bed->thickEnd)
		    continue;
		if(blockStart < bed->thickStart) blockStart = bed->thickStart;
		if(blockEnd > bed->thickEnd) blockEnd = bed->thickEnd;
		}
	    fprintf(out, "%s\t%d\t%d\t%s\t%d\t%s\n",
		    bed->chrom, blockStart, blockEnd,
		    bed->name, bed->score, bed->strand);
	    }
	}
    }
carefulClose(&out);
}

int main(int argc, char *argv[])
{
optionInit(&argc, argv, NULL);
if(argc != 3)
    usage();
cdsOnly = optionExists("cdsOnly");
bedToExons(argv[1], argv[2]);
return 0;
}
    
