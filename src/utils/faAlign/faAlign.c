/* faAlign - Align two fasta files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "fa.h"
#include "axt.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "faAlign - Align two fasta files\n"
  "usage:\n"
  "   faAlign target.fa query.fa output.axt\n"
  "options:\n"
  "   -dna - use DNA scoring scheme\n"
  );
}

static struct optionSpec options[] = {
   {"dna", OPTION_BOOLEAN},
   {NULL, 0},
};

void faAlign(char *targetFile, char *queryFile, char *outFile, boolean isDna)
/* faAlign - Align two fasta files. */
{
bioSeq *targetList = faReadAllMixed(targetFile);
bioSeq *queryList = faReadAllMixed(queryFile);
bioSeq *target, *query;
struct axtScoreScheme *ss;
FILE *f = mustOpen(outFile, "w");

if (isDna)
    ss = axtScoreSchemeDefault();
else
    ss = axtScoreSchemeProteinDefault();
for (query = queryList; query != NULL; query = query->next)
    {
    for (target = targetList; target != NULL; target = target->next)
       {
       struct axt *axt = axtAffine(query, target, ss);
       if (axt != NULL)
           {
	   axtWrite(axt, f);
	   axtFree(&axt);
	   }
       }
    }
carefulClose(&f);
freeDnaSeq(&targetList);
freeDnaSeq(&queryList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
faAlign(argv[1], argv[2], argv[3], optionExists("dna"));
return 0;
}
