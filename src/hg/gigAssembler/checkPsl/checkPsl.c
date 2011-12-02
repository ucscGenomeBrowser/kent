/* checkPsl - make sure all the psls that should be there are there
 * and are not empty. */
#include "common.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkPsl - make sure all psls that should be there are there\n"
  "and are not empty.\n"
  "usage:\n"
  "    checkPsl inLstDir pslDir");
}

void checkPsl(char *inDir, char *pslDir)
/* checkPsl - make sure all the psls that should be there are there
 * and are not empty. */
{
char pslName[512];
struct slName *inList, *inEl;
struct slName *smallList = NULL, *small;
struct slName *missingList = NULL, *missing;

inList = listDir(inDir, "*");
for (inEl = inList; inEl != NULL; inEl = inEl->next)
    {
    char *in = inEl->name;
    chopSuffix(in);
    sprintf(pslName, "%s/%s.psl", pslDir, in);
    if (!fileExists(pslName))
	{
	missing = newSlName(in);
	slAddHead(&missingList, missing);
	}
    else
	{
	if (fileSize(pslName) <= 420) 
	    {
	    small = newSlName(in);
	    slAddHead(&smallList, small);
	    }
	}
    }
if (smallList == NULL && missingList == NULL)
    printf("No problems in %s\n", pslDir);
if (missingList != NULL)
    {
    slReverse(&missingList);
    printf("%d missing files in %s\n", slCount(missingList), pslDir);
    for (missing = missingList; missing != NULL; missing = missing->next)
	printf("%s\n", missing->name);
    }
if (smallList != NULL)
    {
    slReverse(&smallList);
    printf("%d empty files in %s\n", slCount(smallList), pslDir);
    for (small = smallList; small != NULL; small = small->next)
   	printf("%s\n", small->name);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
checkPsl(argv[1], argv[2]);
return 0;
}
