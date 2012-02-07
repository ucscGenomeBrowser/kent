/* clonesInAgp - List clones in AGP file. */
#include "common.h"
#include "hash.h"
#include "linefile.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "clonesInAgp - List clones in AGP file\n"
  "usage:\n"
  "   clonesInAgp agpFile(s)\n");
}

struct clone
/* Info on one clone. */
   {
   struct clone *next;
   char *name;	/* Allocated in hash. */
   };

void clonesInAgp(int agpCount, char *agpFiles[])
/* clonesInAgp - List clones in AGP file. */
{
struct hash *cloneHash = newHash(0);
int i;
char *fileName;
struct lineFile *lf;
int wordCount;
char *words[16];
struct clone *cloneList = NULL, *clone;

for (i=0; i<agpCount; ++i)
    {
    fileName = agpFiles[i];
    lf = lineFileOpen(fileName, TRUE);
    while ((wordCount = lineFileChop(lf, words)) > 0)
        {
	if (wordCount == 9 && !sameString(words[4], "N"))
	    {
	    char *cloneName = words[5];
	    if (!hashLookup(cloneHash, cloneName))
	        {
		struct hashEl *hel;
		AllocVar(clone);
		hel = hashAdd(cloneHash, cloneName, clone);
		clone->name = hel->name;
		slAddHead(&cloneList, clone);
		}
	    }
	}
    lineFileClose(&lf);
    }
slReverse(&cloneList);
for (clone = cloneList; clone != NULL; clone = clone->next)
    printf("%s\n", clone->name);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 2)
    usage();
clonesInAgp(argc-1, argv+1);
return 0;
}
