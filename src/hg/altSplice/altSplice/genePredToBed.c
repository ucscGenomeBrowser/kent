#include "common.h"
#include "genePred.h"
#include "bed.h"

void usage() 
{
errAbort("genePredToBed - Convert genePred files to beds.\n"
	 "usage:\n"
	 "   genePredToBed <input.genePred> <output.bed>\n");
}


int main(int argc, char *argv[])
{
FILE *out = NULL;
struct genePred *gpList=NULL, *gp =NULL;
struct bed *bed = NULL, *bedList = NULL;
if(argc != 3)
    usage();
warn("Reading genePred records.");
gpList = genePredLoadAll(argv[1]);
out = mustOpen(argv[2], "w");
warn("Converting genePred records.");
for(gp = gpList; gp != NULL; gp = gp->next)
    {
    bed = bedFromGenePred(gp);
    bedTabOutN(bed, 12, out);
    bedFree(&bed);
    }
carefulClose(&out);
genePredFreeList(&gpList);
warn("Done.");
return 0;
}
