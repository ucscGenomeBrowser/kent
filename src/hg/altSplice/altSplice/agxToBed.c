/* agxToBed program that merges all exons in an altGraphX
   structure to form a bed.
*/
#include "common.h"
#include "altGraphX.h"

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
for(ag = agList; ag != NULL; ag = ag->next)
    {
    occassionalDot();
    bed = altGraphXToBed(ag);
    if(bed->blockCount > 0)
	bedTabOutN(bed, 12, bedOut);
    bedFree(&bed);
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
