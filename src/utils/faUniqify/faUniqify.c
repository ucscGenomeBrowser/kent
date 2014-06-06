/* faUniqify - Remove redundant sequences from fasta.  Warn if different sequences have same name.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fa.h"

/* Globals that hold command line options. */
boolean addId = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faUniqify - Remove redundant sequences from fasta.  Warn if different sequences have same name.\n"
  "usage:\n"
  "   faUniqify in.fa out.fa\n"
  "options:\n"
  "   -verbose=0 - suppress warning messages\n"
  "   -addId - add an ID suffix to make things unique\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"addId", OPTION_BOOLEAN},
   {NULL, 0},
};

int hashCountMatches(struct hash *hash, char *key)
/* Count number of things matching key in hash */
{
int count = 0;
struct hashEl *hel;
for (hel = hashLookup(hash, key); hel != NULL; hel = hashLookupNext(hel))
     ++count;
return count;
}
void faUniqify(char *inFile, char *outFile)
/* faUniqify - Remove redundant sequences from fasta.  Warn if different sequences have same name.. */
{
struct hash *uniqHash = hashNew(0);
struct lineFile *lf = lineFileOpen(inFile, FALSE);
FILE *f = mustOpen(outFile, "w");
DNA *dna;
int size;
char *name;
while (faMixedSpeedReadNext(lf, &dna, &size, &name))
    {
    char *newDna = cloneMem(dna, size+1);
    char *oldDna = hashFindVal(uniqHash, name);
    if (oldDna != NULL)
         {
	 if (!sameString(oldDna, newDna))
	     warn("Name %s reused for different sequences", name);
	 }
    if (addId || oldDna == NULL)
         {
	 hashAdd(uniqHash, name, newDna);
	 char *fullName = name;
	 char buf[PATH_LEN];
	 if (addId)
	     {
	     int count = hashCountMatches(uniqHash, name);
	     safef(buf, sizeof(buf), "%sv%d", name, count);
	     fullName = buf;
	     }
	 faWriteNext(f, fullName, newDna, size);
	 }
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
addId = optionExists("addId");
faUniqify(argv[1], argv[2]);
return 0;
}
