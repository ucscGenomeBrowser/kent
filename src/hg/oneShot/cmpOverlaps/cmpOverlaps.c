/* cmpOverlaps - Compare two overlap files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"


struct ocp
/* Overlapping clone pair. */
    {
    struct ocp *next;
    char *name;
    char *a;
    char *b;
    int overlap;
    };

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cmpOverlaps - Compare two overlap files\n"
  "usage:\n"
  "   cmpOverlaps a b\n");
}

char *ocpHashName(char *a, char *b)
/* Return name of ocp associated with a and b */
{
static char buf[256];
if (strcmp(a,b) < 0)
    sprintf(buf, "%s^%s", a, b);
else
    sprintf(buf, "%s^%s", b, a);
return buf;
}

void readOverlap(char *fileName, struct ocp **retList, struct hash **retHash)
/* Read overlap file into hash/list. */
{
struct ocp *ocpList = NULL, *ocp;
struct hash *hash = newHash(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[3];

while (lineFileRow(lf, row))
    {
    AllocVar(ocp);
    slAddHead(&ocpList, ocp);
    ocp->a = cloneString(row[0]);
    ocp->b = cloneString(row[1]);
    hashAddSaveName(hash, ocpHashName(ocp->a, ocp->b), ocp, &ocp->name);
    ocp->overlap = lineFileNeedNum(lf, row, 2);
    }
lineFileClose(&lf);
slReverse(&ocpList);
*retList = ocpList;
*retHash = hash;
}

boolean anyDiff = FALSE;

void crossCompare(char *listName, char *hashName, 
	struct ocp *ocpList, struct hash *ocpHash)
/* Make sure everything on list is also in hash.  Print where this
 * is not true. */
{
struct ocp *ocp, *hashOcp;
for (ocp = ocpList; ocp != NULL; ocp = ocp->next)
    {
    if (!hashLookup(ocpHash, ocp->name))
	{
	printf("%s is in %s but not %s\n", ocp->name, listName, hashName);
	anyDiff = TRUE;
	}
    }
}

void cmpOverlaps(char *aFile, char *bFile)
/* cmpOverlaps - Compare two overlap files. */
{
struct ocp *aList, *bList, *ocp;
struct hash *aHash, *bHash;

readOverlap(aFile, &aList, &aHash);
readOverlap(bFile, &bList, &bHash);
crossCompare(aFile, bFile, aList, bHash);
crossCompare(bFile, aFile, bList, aHash);
if (!anyDiff)
    printf("%s and %s are the same\n", aFile, bFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
cmpOverlaps(argv[1], argv[2]);
return 0;
}
