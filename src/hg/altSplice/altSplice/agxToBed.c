/* agxToBed program that merges all exons in an altGraphX
   structure to form a bed.
*/
#include "common.h"
#include "altGraphX.h"
#include "obscure.h"

void usage() 
{
errAbort("agxToBed - Utility program that condenses an altGraphX record\n"
	 "into a bed record.\n"
	 "usage:\n\t"
	 "agxToBed altGraphXFile bedOutFile\n");
}

void occassionalDot()
/* Write out a dot every 20 times this is called. */
{
static int dotMod = 1000;
static int dot = 10000;
if (--dot <= 0)
    {
    putc('.', stdout);
    fflush(stdout);
    dot = dotMod;
    }
}

void agxToBed(char *agxFileName, char *bedFileName)
{
struct altGraphX *ag = NULL, *agList = NULL;
struct bed *bed = NULL;
FILE *bedOut = NULL;
warn("Loading altGraphX records.");
agList = altGraphXLoadAll(agxFileName);
warn("Converting to beds.");
bedOut = mustOpen(bedFileName, "w");
dotForUserInit(max(slCount(agList)/15, 1));
for(ag = agList; ag != NULL; ag = ag->next)
    {
    struct altGraphX *agComp = NULL, *agCompList = NULL;
    dotForUser();
    agCompList = agxConnectedComponents(ag);
    for(agComp = agCompList; agComp != NULL; agComp = agComp->next)
	{
	if(agComp->edgeCount > 0)
	    {
	    bed = altGraphXToBed(agComp);
	    if(bed->blockCount > 0)
		bedTabOutN(bed, 12, bedOut);
	    bedFree(&bed);
	    }
	}
    altGraphXFreeList(&agCompList);
    }
carefulClose(&bedOut);
warn("Done.");
}

int main(int argc, char *argv[])
{
if(argc != 3)
    usage();
agxToBed(argv[1], argv[2]);
return 0;
}
