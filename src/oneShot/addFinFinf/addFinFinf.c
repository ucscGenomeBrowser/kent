/* addFinFinf - Add list of finished clones to end of finf file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "obscure.h"
#include "fa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "addFinFinf - Add list of finished clones to end of finf file.\n"
  "usage:\n"
  "   addFinFinf cloneList finished.finf\n"
  "options:\n"
  "   -inf - do same thing to sequence.inf file instead\n"
  );
}

void addFinFinf(char *newClones, char *finfName)
/* addFinFinf - Add list of finished clones to end of finf file.. */
{
struct slName *cloneList = readAllLines(newClones), *clone;
char *cloneName;
char acc[126];
FILE *f = mustOpen(finfName, "a");
char faFile[512];
struct dnaSeq *seq = NULL;
boolean doInf = cgiBoolean("inf");

for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    cloneName = clone->name;
    strcpy(acc, cloneName);
    chopSuffix(acc);
    sprintf(faFile, "/projects/hg3/gs.8/missing/fa/%s.fa", acc);
    seq = faReadDna(faFile);
    if (doInf)
        fprintf(f, "%s\tunknown\t%d\t3\t-\t-\t-\tunknown\tunknown\n", cloneName, seq->size);
    else
	fprintf(f, "%s~1\t%s\t1\t%d\t1\t1,1/1\tw\n", cloneName, cloneName, seq->size);
    freeDnaSeq(&seq);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
addFinFinf(argv[1], argv[2]);
return 0;
}
