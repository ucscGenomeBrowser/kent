/* twoBitInfo - get information about sequences in a .2bit file. */
#include "common.h"
#include "options.h"
#include "twoBit.h"

static char const rcsid[] = "$Id: twoBitInfo.c,v 1.1 2004/02/27 08:35:28 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "twoBitInfo - get information about sequences in a .2bit file\n"
  "usage:\n"
  "   twoBitInfo input.2bit output.tab\n"
  "options:\n"
  ":\n"
  "Output file has the columns::\n"
  "   seqName size\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void twoBitInfo(char *inName, char *outName)
/* twoBitInfo - get information about sequences in a .2bit file. */
{
struct twoBitFile *tbf = twoBitOpen(inName);
FILE *outFile = mustOpen(outName, "w");
struct twoBitIndex *index;

 for (index = tbf->indexList; index != NULL; index = index->next)
     {
     fprintf(outFile, "%s\t%d\n", index->name, twoBitSeqSize(tbf, index->name));
     }
twoBitClose(&tbf);
carefulClose(&outFile); 
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
twoBitInfo(argv[1], argv[2]);
return 0;
}
