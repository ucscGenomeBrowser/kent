/* hapmapMonomorphic -- first step after reading genotype files. */
/* If there are 10 elements rather than 12, add 2. */
/* Use "?" for allele2, zero for allele2Count. */
/* Also check and discard total count. */

#include "common.h"

#include "hdb.h"
#include "linefile.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: hapmapMonomorphic.c,v 1.1 2007/01/30 06:10:56 heather Exp $";

FILE *errorFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hapmapMonomorphic - Change 10 elements into 12 as necessary.\n"
  "usage:\n"
  "  hapmapMonomorphic inputFileName outputFileName \n");
}

boolean validAllele(char *allele)
{
if (sameString(allele, "A")) return TRUE;
if (sameString(allele, "C")) return TRUE;
if (sameString(allele, "G")) return TRUE;
if (sameString(allele, "T")) return TRUE;
return FALSE;
}


boolean validInput(char **row, int count)
/* This checks any input row. */
/* Errors are written to errorFileHandle. */
{
int count1, count2, total;

/* allele should be 'A', 'C', 'G', or 'T'  */
if (!validAllele(row[7]))
    {
    fprintf(errorFileHandle, "invalid allele %s for %s \n", row[7], row[3]);
    return FALSE;
    }

if (count == 12)
    if (!validAllele(row[9]))
        {
        fprintf(errorFileHandle, "invalid allele %s for %s \n", row[7], row[3]);
        return FALSE;
        }

/* check counts */
count1 = sqlSigned(row[8]);
if (count == 12)
    {
    count2 = sqlSigned(row[10]);
    total = sqlSigned(row[11]);
    }
else
    {
    count2 = 0;
    total = sqlSigned(row[9]);
    }
    
if (total != count1 + count2)
    {
    fprintf(errorFileHandle, "wrong counts for %s \n", row[3]);
    fprintf(errorFileHandle, "count1 = %d, count2 = %d, total = %d\n", count1, count2, total);
    return FALSE;
    }

return TRUE;
}


void readFile(char *inputFileName, char *outputFileName)
/* Read input.  Write to output and log file as necessary. */
{
struct lineFile *lf = NULL;
char *line;
char *row[12];
int elementCount;
boolean isValid;
FILE *outputFileHandle = mustOpen(outputFileName, "w");
char *allele2 = NULL;
int allele2Count = 0;

FILE *logFileHandle = mustOpen("hapmapMonomorphic.log", "w");

lf = lineFileOpen(inputFileName, TRUE);
while (lineFileNext(lf, &line, NULL)) 
    {
    elementCount = chopString(line, " ", row, ArraySize(row));
    if (elementCount != 10 && elementCount != 12)
        {
	fprintf(errorFileHandle, "unexpected format: %s\n", line);
	continue;
	}

    isValid = validInput(row, elementCount);
    if (!isValid) continue;
    if (elementCount == 10)
        {
	allele2 = "?";
	allele2Count = 0;
	fprintf(logFileHandle, "%s\n", row[3]);
        }
    else
        {
	/* memory trick here */
	allele2 = row[9];
	allele2Count = sqlUnsigned(row[10]);
	}
    fprintf(outputFileHandle, "%s %s %s %s %s %s %s ", 
                              row[0], row[1], row[2], row[3], row[4], row[5], row[6]);
    /* allele1, allele1Count */
    fprintf(outputFileHandle, "%s %s ", row[7], row[8]);
    fprintf(outputFileHandle, "%s %d\n", allele2, allele2Count);
    }
lineFileClose(&lf);
carefulClose(&outputFileHandle);
carefulClose(&logFileHandle);
}

int main(int argc, char *argv[])
/* read hapmap allele frequency files which have lines with 10 or 12 elements */
/* output all lines with 12 elements */
{
if (argc != 3)
    usage();

errorFileHandle = mustOpen("hapmapMonomorphic.err", "w");
readFile(argv[1], argv[2]);
carefulClose(&errorFileHandle);

return 0;
}
