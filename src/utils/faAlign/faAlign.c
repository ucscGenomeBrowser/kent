/* faAlign - Align two fasta files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "fa.h"
#include "axt.h"

static char const rcsid[] = "$Id: faAlign.c,v 1.1 2004/07/02 06:23:19 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faAlign - Align two fasta files\n"
  "usage:\n"
  "   faAlign target.fa query.fa output.axt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void faAlign(char *targetFile, char *queryFile, char *outFile)
/* faAlign - Align two fasta files. */
{
bioSeq *targetList = faReadAllPep(targetFile);
bioSeq *queryList = faReadAllPep(queryFile);
bioSeq *target, *query;
struct axtScoreScheme *ss = axtScoreSchemeProteinDefault();
FILE *f = mustOpen(outFile, "w");

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
faAlign(argv[1], argv[2], argv[3]);
return 0;
}
