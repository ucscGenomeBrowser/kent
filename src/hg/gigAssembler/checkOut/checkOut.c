/* Check out - check directory to make sure it has
 * a .out file for each .fa file, and that .out file
 * is non-empty. */
#include "common.h"
#include "hash.h"
#include "portable.h"


void checkOut(char *dir)
/* Check out - check directory to make sure it has
 * a .out file for each .fa file, and that .out file
 * is non-empty. */
{
char outName[512];
struct slName *outList, *outEl, *faList, *faEl;
struct hash *outHash = newHash(16);
boolean missingAny = FALSE, zeroAny = FALSE;

printf("Listing *.fa.out\n");
outList = listDir(dir, "*.fa.out");
for (outEl = outList; outEl != NULL; outEl = outEl->next)
    hashAdd(outHash, outEl->name, NULL);
printf("Listing *.fa\n");
faList = listDir(dir, "*.fa");
printf("Missing .out\n");
for (faEl = faList; faEl != NULL; faEl = faEl->next)
    {
    sprintf(outName, "%s.out", faEl->name);
    if (!hashLookup(outHash, outName))
	{
	printf("%s\n", faEl->name);
	missingAny = TRUE;
	}
    }
if (!missingAny)
    printf("  none\n");
printf("Zero size .out\n");
for (outEl = outList; outEl != NULL; outEl = outEl->next)
    {
    long size;
    sprintf(outName, "%s/%s", dir, outEl->name);
    size = fileSize(outName);
    if (size <= 0)
	{
	printf("%s\n", outEl->name);
	zeroAny = TRUE;
	}
    }
if (!zeroAny)
    printf("  none\n");
}


void usage()
/* Explain usage and exit. */
{
errAbort("checkOut - check to make sure all .fa files in a dir\n"
         "have a .fa.out file\n"
	 "usage:\n"
	 "   checkOut dir");
}

int main(int argc, char *argv[])
{
if (argc != 2)
    usage();
checkOut(argv[1]);
return 0;
}
