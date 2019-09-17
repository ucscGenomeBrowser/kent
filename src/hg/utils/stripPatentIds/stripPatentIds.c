/* stripPatentIds - strip patent ids from a list of ids. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "genbank.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "stripPatentIds - strip patent ids from a list of ids\n"
  "usage:\n"
  "   stripPatentIds listOfIds listOfIdsWithoutPatents\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void stripPatentIds(char *accessionList, char *outFile)
/* stripPatentIds - strip patent ids from a list of ids. */
{
struct lineFile *lf = lineFileOpen(accessionList, TRUE);
FILE *outF = mustOpen(outFile, "w");
char *row[1];
while (lineFileRow(lf, row))
    {
    if (!isGenbankPatentAccession(row[0]))
        fprintf(outF, "%s\n", row[0]);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
stripPatentIds(argv[1], argv[2]);
return 0;
}
