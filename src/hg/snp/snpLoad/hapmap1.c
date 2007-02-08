/* hapmap1 -- first step after reading genotype files. */
/* Determine 2 alleles */
/* Input is:
   chrom
   chromStart
   chromEnd
   name (rsId)
   score (always zero)
   strand
   observed
   A, homoCount, heteroCount
   C, homoCount, heteroCount
   G, homoCount, heteroCount
   T, homoCount, heteroCount */

/* Check for triallelic or quadallelic -- log and discard if found. */
/* Also check for no alleles. */
/* Also calculate score. */

/* Output is:
   chrom
   chromStart
   chromEnd
   name
   score
   strand
   observed
   allele1
   allele1Count (homozygotes)
   allele2 (if not found, use '?')
   allele2Count (homozygotes)
   heteroCount */

#include "common.h"

#include "hdb.h"
#include "linefile.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: hapmap1.c,v 1.2 2007/02/08 21:09:04 heather Exp $";

FILE *logFileHandle = NULL;
FILE *outputFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hapmap1 \n"
  "usage:\n"
  "  hapmap1 inputFileName outputFileName \n");
}

int alleleCount(int aCount, int cCount, int gCount, int tCount)
/* returns 0 if no alleles */
/* returns 1 if monomorphic */
/* returns 2 if biallelic */
/* returns 3 if triallelic */
/* returns 4 if quadallelic */
{
int ret = 0;
if (aCount > 0) ret++;
if (cCount > 0) ret++;
if (gCount > 0) ret++;
if (tCount > 0) ret++;
return ret;
}

int getScore(int aCount, int cCount, int gCount, int tCount)
/* return major allele frequency */
{
int maxCount = 0;
int total = aCount + cCount + gCount + tCount;

maxCount = aCount;
if (cCount > maxCount) maxCount = cCount;
if (gCount > maxCount) maxCount = gCount;
if (tCount > maxCount) maxCount = tCount;

maxCount = maxCount * 1000;
return maxCount / total;
}

void handleInput(char **row)
{
int aCountHomo = sqlUnsigned(row[8]);
int aCountHetero = sqlUnsigned(row[9]);
int cCountHomo = sqlUnsigned(row[11]);
int cCountHetero = sqlUnsigned(row[12]);
int gCountHomo = sqlUnsigned(row[14]);
int gCountHetero = sqlUnsigned(row[15]);
int tCountHomo = sqlUnsigned(row[17]);
int tCountHetero = sqlUnsigned(row[18]);

int status = alleleCount(aCountHomo + aCountHetero, cCountHomo + cCountHetero, gCountHomo + gCountHetero, tCountHomo + tCountHetero);

int score = 0;

if (status > 2 || status == 0)
    {
    fprintf(logFileHandle, "Wrong allele count for %s\n", row[3]);
    return;
    }

score = getScore(aCountHomo + aCountHetero, cCountHomo + cCountHetero, gCountHomo + gCountHetero, tCountHomo + tCountHetero);

fprintf(outputFileHandle, "%s %s %s %s %d %s %s ", row[0], row[1], row[2], row[3], score, row[5], row[6]);

if (aCountHomo > 0 || aCountHetero > 0)
    fprintf(outputFileHandle, "A %d ", aCountHomo);
if (cCountHomo > 0 || cCountHetero > 0)
    fprintf(outputFileHandle, "C %d ", cCountHomo);
if (gCountHomo > 0 || gCountHetero > 0)
    fprintf(outputFileHandle, "G %d ", gCountHomo);
if (tCountHomo > 0 || tCountHetero > 0)
    fprintf(outputFileHandle, "T %d ", tCountHomo);

/* handle monomorphic */
if (status == 1)
    fprintf(outputFileHandle, "? 0 ");

/* maximum 2 of these can be greater than zero */
fprintf(outputFileHandle, "%d\n", aCountHetero + cCountHetero + gCountHetero + tCountHetero);
}


void readFile(char *inputFileName)
{
struct lineFile *lf = NULL;
char *line;
char *row[19];
int elementCount;

lf = lineFileOpen(inputFileName, TRUE);
while (lineFileNext(lf, &line, NULL)) 
    {
    elementCount = chopString(line, " ", row, ArraySize(row));
    if (elementCount != 19)
        {
	fprintf(logFileHandle, "unexpected format: %s\n", line);
	continue;
	}
    handleInput(row);
    }
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* read hapmap allele files which have lines with 19 elements */
/* output lines with 12 elements */
{
if (argc != 3)
    usage();

outputFileHandle = mustOpen(argv[2], "w");
logFileHandle = mustOpen("hapmap1.log", "w");
readFile(argv[1]);
carefulClose(&outputFileHandle);
carefulClose(&logFileHandle);

return 0;
}
