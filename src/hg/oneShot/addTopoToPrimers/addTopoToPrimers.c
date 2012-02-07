/* addTopoToPrimers - Add topo sequence to primers in two column name/primer file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "addTopoToPrimers - Add topo sequence to primers in two column name/primer file\n"
  "usage:\n"
  "   addTopoToPrimers inFile outFile\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void addTopoToPrimers(char *inFile, char *outFile)
/* addTopoToPrimers - Add topo sequence to primers in two column name/primer file. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
char *row[2];
while (lineFileRow(lf, row))
    {
    char *primer = row[1];
    tolowers(primer);
    fprintf(f, "%s\t", row[0]);
    if (startsWith("cacc", primer))
        ;
    else if (startsWith("acc", primer))
        fprintf(f, "C");
    else if (startsWith("cc", primer))
        fprintf(f, "CA");
    else if (startsWith("c", primer))
        fprintf(f, "CAC");
    else
        fprintf(f, "CACC");
    fprintf(f, "%s\n", primer);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
addTopoToPrimers(argv[1], argv[2]);
return 0;
}
