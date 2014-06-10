/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
/* Also calculate score (minor allele frequency). */

/* Output is:
   chrom
   chromStart
   chromEnd
   name
   score (minor allele frequency)
   strand
   observed
   allele1
   homoCount1 (count of individuals who are homozygous for allele1)
   allele2 (if not found, use "none")
   homoCount2 (count of individuals who are homozygous for allele2)
   heteroCount */

#include "common.h"

#include "hdb.h"
#include "linefile.h"
#include "sqlNum.h"


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
int score = 0;

maxCount = aCount;
if (cCount > maxCount) maxCount = cCount;
if (gCount > maxCount) maxCount = gCount;
if (tCount > maxCount) maxCount = tCount;

maxCount = maxCount * 1000;
score = 1000 - (maxCount / total);
return score;
}

boolean validObserved(char *observed, int aCount, int cCount, int gCount, int tCount)
{
if (sameString(observed, "A/C"))
    {
    if (gCount > 0 || tCount > 0) return FALSE;
    return TRUE;
    }
if (sameString(observed, "A/G"))
    {
    if (cCount > 0 || tCount > 0) return FALSE;
    return TRUE;
    }
if (sameString(observed, "A/T"))
    {
    if (cCount > 0 || gCount > 0) return FALSE;
    return TRUE;
    }
if (sameString(observed, "C/G"))
    {
    if (aCount > 0 || tCount > 0) return FALSE;
    return TRUE;
    }
if (sameString(observed, "C/T"))
    {
    if (aCount > 0 || gCount > 0) return FALSE;
    return TRUE;
    }
if (sameString(observed, "G/T"))
    {
    if (aCount > 0 || cCount > 0) return FALSE;
    return TRUE;
    }

/* don't check if not simple */
return TRUE;
}

void handleInput(char **row)
{
int aCountHomo = sqlUnsigned(row[8]);
int aCountHetero = sqlUnsigned(row[9]);
int aCount = aCountHomo + aCountHetero;
int cCountHomo = sqlUnsigned(row[11]);
int cCountHetero = sqlUnsigned(row[12]);
int cCount = cCountHomo + cCountHetero;
int gCountHomo = sqlUnsigned(row[14]);
int gCountHetero = sqlUnsigned(row[15]);
int gCount = gCountHomo + gCountHetero;
int tCountHomo = sqlUnsigned(row[17]);
int tCountHetero = sqlUnsigned(row[18]);
int tCount = tCountHomo + tCountHetero;

int status = alleleCount(aCount, cCount, gCount, tCount);

int score = 0;

if (status > 2 || status == 0)
    {
    fprintf(logFileHandle, "Wrong allele count for %s\n", row[3]);
    return;
    }

score = getScore(aCount, cCount, gCount, tCount);

if (!validObserved(row[6], aCount, cCount, gCount, tCount))
    {
    fprintf(logFileHandle, "incorrect alleles for %s\n", row[3]);
    return;
    }

fprintf(outputFileHandle, "%s %s %s %s %d %s %s ", row[0], row[1], row[2], row[3], score, row[5], row[6]);

if (aCountHomo > 0 || aCountHetero > 0)
    fprintf(outputFileHandle, "A %d ", aCountHomo/2);
if (cCountHomo > 0 || cCountHetero > 0)
    fprintf(outputFileHandle, "C %d ", cCountHomo/2);
if (gCountHomo > 0 || gCountHetero > 0)
    fprintf(outputFileHandle, "G %d ", gCountHomo/2);
if (tCountHomo > 0 || tCountHetero > 0)
    fprintf(outputFileHandle, "T %d ", tCountHomo/2);

/* handle monomorphic */
if (status == 1)
    fprintf(outputFileHandle, "none 0 ");

/* maximum 2 of these can be greater than zero */
fprintf(outputFileHandle, "%d\n", (aCountHetero + cCountHetero + gCountHetero + tCountHetero)/2);
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
