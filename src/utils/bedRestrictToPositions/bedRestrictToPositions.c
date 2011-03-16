/* bedRestrictToPositions - Filter bed file, restricting to only ones that match chrom/start/ends 
 * specified in restrict.bed file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedRestrictToPositions - Filter bed file, restricting to only ones that match chrom/start/ends specified in restrict.bed file.\n"
  "usage:\n"
  "   bedRestrictToPositions in.bed restrict.bed out.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

#define BED_STRING_SIZE 256

char *bedString(char *chrom, char *start, char *end, char result[BED_STRING_SIZE])
/* Return space delimited concatenation: chrom start end */
{
safef(result, BED_STRING_SIZE, "%s\t%s\t%s", chrom, start, end);
return result;
}

struct hash *bedIntoHash(char *fileName)
/* Read in a bed file, return hash keyed by bedStrings (with empty vals)
 * and return this hash. */
{
/* Add each bed item to hash, and list, checking uniqueness */
struct hash *hash = hashNew(21);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[3];
while (lineFileRow(lf, row))
    {
    char key[BED_STRING_SIZE];
    bedString(row[0], row[1], row[2], key);
    hashAdd(hash, key, NULL);
    }

/* Clean up and go home. */
lineFileClose(&lf);
return hash;
}
void bedRestrictToPositions(char *inFile, char *restrictFile, char *outFile)
/* bedRestrictToPositions - Filter bed file, restricting to only ones that match chrom/start/ends 
 * specified in restrict.bed file. */
{
struct hash *restrictHash = bedIntoHash(restrictFile);
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
char *line;
while (lineFileNextReal(lf, &line))
    {
    char *chrom = nextWord(&line);
    char *start = nextWord(&line);
    char *end = nextWord(&line);
    if (end == NULL)
        errAbort("Expecting at least three words line %d of %s", lf->lineIx, lf->fileName);
    char key[BED_STRING_SIZE];
    bedString(chrom, start, end, key);
    if (hashLookup(restrictHash, key))
        {
	fprintf(f, "%s\t%s\t%s", chrom, start, end);
	line = skipLeadingSpaces(line);
	if (isEmpty(line))
	    fputc('\n', f);
	else
	    fprintf(f, "\t%s\n", line);
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
bedRestrictToPositions(argv[1], argv[2], argv[3]);
return 0;
}
