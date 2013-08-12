/* faUniqify - Remove redundant sequences from fasta.  Warn if different sequences have same name.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faUniqify - Remove redundant sequences from fasta.  Warn if different sequences have same name.\n"
  "usage:\n"
  "   faUniqify in.fa out.fa\n"
  "options:\n"
  "   -verbose=0 - suppress warning messages\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

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
    else
         {
	 hashAdd(uniqHash, name, newDna);
	 faWriteNext(f, name, newDna, size);
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
faUniqify(argv[1], argv[2]);
return 0;
}
