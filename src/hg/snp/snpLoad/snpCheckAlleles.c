/* snpCheckAlleles
 * Run after snpRefUCSC.
 *
 * Log 2 annotations:
 * RefAlleleNotRevComp
 * RefAlleleMismatch
 *
 * Use UCSC chromInfo.  */


#include "common.h"

#include "dystring.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpCheckAlleles.c,v 1.9 2006/04/06 18:19:37 heather Exp $";

static char *snpDb = NULL;
FILE *exceptionFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpCheckAlleles - log allele exceptions\n"
    "usage:\n"
    "    snpCheckAlleles snpDb \n");
}

void writeToExceptionFile(char *chrom, char *start, char *end, char *name, char *exception)
{
fprintf(exceptionFileHandle, "%s\t%s\t%s\trs%s\t%s\n", chrom, start, end, name, exception);
}

void doCheckAlleles(char *chromName)
/* check each row for exceptions */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];
int loc_type = 0;
int chromStart = 0;
int chromEnd = 0;
int span = 0;

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
if (!hTableExists(tableName)) return;

verbose(1, "chrom = %s\n", chromName);
safef(query, sizeof(query), 
    "select snp_id, chromStart, chromEnd, loc_type, orientation, allele, refUCSC, refUCSCReverseComp from %s where loc_type != 3", 
     tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    /* we already check size of allele in snpLocType and snpExpandAllele. */
    /* skip past alleles with wrong size here. */
    chromStart = sqlUnsigned(row[1]);
    chromEnd = sqlUnsigned(row[2]);
    span = chromEnd - chromStart;
    if (span != strlen(row[5])) continue;

    loc_type = sqlUnsigned(row[3]);

    /* check positive strand first */
    if (sameString(row[4], "0"))
        {
        if (sameString(row[5], row[6])) continue;
        writeToExceptionFile(chromName, row[1], row[2], row[0], "RefAlleleMismatchPositiveStrand");
	continue;
	}

    /* negative strand */
    /* ideally matches reverse comp */
    if (sameString(row[5], row[7])) continue;

    /* matching non-reverse comp is useful info */
    if (sameString(row[5], row[6]))
        {
	if (loc_type == 2)
            writeToExceptionFile(chromName, row[1], row[2], row[0], "RefAlleleNotRevCompExactLocType");
	else
            writeToExceptionFile(chromName, row[1], row[2], row[0], "RefAlleleNotRevCompRangeLocType");
        continue;
	}

    /* nothing matching */
    writeToExceptionFile(chromName, row[1], row[2], row[0], "RefAlleleMismatchNegativeStrand");
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* read chrN_snpTmp, log exceptions */
{
struct slName *chromList, *chromPtr;
char tableName[64];

if (argc != 2)
    usage();

snpDb = argv[1];
hSetDb(snpDb);
chromList = hAllChromNamesDb(snpDb);

exceptionFileHandle = mustOpen("snpCheckAlleles.exceptions", "w");

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    doCheckAlleles(chromPtr->name);

carefulClose(&exceptionFileHandle);
return 0;
}
