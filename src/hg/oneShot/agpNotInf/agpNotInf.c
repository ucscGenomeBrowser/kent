/* agpNotInf - List clones in .agp file not in .inf file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "hCommon.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "agpNotInf - List clones in .agp file not in .inf file\n"
  "usage:\n"
  "   agpNotInf ctg_nt.agp sequence.inf\n"
  );
}

struct clone
/* Info on one clone. */
   {
   struct clone *next;
   char *name;		/* Allocated in hash. */
   char *version;	/* Genbank version number of clone. */
   };

void addCloneToHash(struct hash *hash, char *accVer)
/* Add accession.ver name to hash. */
{
char cloneName[256];
struct clone *clone;

strcpy(cloneName, accVer);
chopSuffix(cloneName);
if (!hashLookup(hash, cloneName))
    {
    AllocVar(clone);
    hashAddSaveName(hash, cloneName, clone, &clone->name);
    clone->version = cloneString(accVer);
    }
}

struct hash *readAgp(char *fileName)
/* Read AGP file into hash */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);
int wordCount;
char *words[16];

while ((wordCount = lineFileChop(lf, words)) > 0)
    {
    if (wordCount == 9 && !sameString(words[4], "N"))
	{
	addCloneToHash(hash, words[5]);
	}
    }
uglyf("Read %d lines in %s\n", lf->lineIx, fileName);
lineFileClose(&lf);
return hash;
}

struct hash *readInf(char *fileName)
/* Read info file into hash */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);
struct clone *clone;
int wordCount;
char *words[9];

while (lineFileRow(lf, words))
    {
    addCloneToHash(hash, words[0]);
    }
uglyf("Read %d lines in %s\n", lf->lineIx, fileName);
lineFileClose(&lf);
return hash;
}


void agpNotInf(char *agpFile, char *infFile)
/* agpNotInf - List clones in .agp file not in .inf file. */
{
struct hash *agpHash = readAgp(agpFile);
struct hash *infHash = readInf(infFile);
struct hashEl *list, *el;
struct clone *agpClone, *infClone;

list = hashElListHash(agpHash);
uglyf("%d clones in agpHash\n", slCount(list));
for (el = list; el != NULL; el = el->next)
    {
    agpClone = el->val;
    infClone = hashFindVal(infHash, agpClone->name);
    if (infClone == NULL)
        printf("%s missing\n", agpClone->version);
    else if (!sameString(agpClone->version, infClone->version))
        printf("%s updated from %s\n", agpClone->version, infClone->version);
    }
hashElFreeList(&list);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
agpNotInf(argv[1], argv[2]);
return 0;
}
