/* plateOfPrimers - Convert two column file of primers to fodder for plate/primer table. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "plateOfPrimers - Convert two column file of primers to fodder for plate table\n"
  "usage:\n"
  "   plateOfPrimers twoCol.in plateName primerSuffix project purpose outFile\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void plateOfPrimers(char *inFile, char *plate, char *suffix, 
	char *project, char *purpose, char *outFile)
/* plateOfPrimers - Convert two column file of primers to fodder for plate/primer table. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
char *words[2]; 
int rowIx = 0, maxRow = 8, colIx = 0, maxCol=12;
int total = 0;
while (lineFileRow(lf, words))
    {
    char *name = words[0];
    char *seq = words[1];
    fprintf(f, "%s%s\t", name, suffix);
    fprintf(f, "%s\t", name);
    fprintf(f, "%s\t", seq);
    fprintf(f, "%s\t", plate);
    fprintf(f, "%c\t", 'A' + rowIx);
    fprintf(f, "%d\t", 1 + colIx);
    fprintf(f, "%s\t", project);
    fprintf(f, "%s\t", purpose);
    fprintf(f, "0\n");
    ++total;
    ++colIx;
    if (colIx >= maxCol)
        {
	colIx = 0;
	++rowIx;
	}
    }
if (total < 96)
    warn("Expected 96, got only %d", total);
if (total > 96)
    errAbort("Got %d, but only can handl 96", total);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
plateOfPrimers(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
