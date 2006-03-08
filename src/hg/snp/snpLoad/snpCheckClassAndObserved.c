/* snpCheckClassAndObserved
 *
 * Log 6 exceptions:
 *
 * 1) SingleClassBetweenLocType
 *    class = 'single' and loc_type = 3 (between)
 *
 *    Daryl tells me dbSNP wants to leave these as is.
 *    Probably due to submitters leaving off a base on a flank sequence.
 *
 * 2) SingleClassRangeLocType
 *    class = 'single' and loc_type = 1 (range)
 *
 * 3) SingleClassWrongObservedPositiveStrand
 *    SingleClassWrongObservedNegativeStrand
 *
 *    For positive strand, if the allele is not in the observed string.
 *    For negative strand, if reverseComp(allele) is not in the observed string.
 *    
 * 4) DeletionClassWrongObservedSize
 *    for class = 'deletion', compare the length 
 *    of the observed string to chromRange
 *
 * 5) DeletionClassWrongObserved
 *    observed doesn't match refUCSC or refUCSCReverseComp
 *
 * 6) NamedClassWrongLocType
 *    for class = 'named'
 *    if observed like "LARGEDELETION" then expect loc_type = 1
 *    if observed like "LARGEINSERTION" then expect loc_type = 3
 *
 * Use UCSC chromInfo.  */


#include "common.h"

#include "dystring.h"
#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpCheckClassAndObserved.c,v 1.9 2006/03/08 21:17:28 heather Exp $";

static char *snpDb = NULL;
FILE *exceptionFileHandle = NULL;


void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpCheckClassAndObserved - log exceptions with class and observed\n"
    "usage:\n"
    "    snpCheckClassAndObserved snpDb \n");
}



void writeToExceptionFile(char *chrom, char *start, char *end, char *name, char *exception)
{
fprintf(exceptionFileHandle, "%s\t%s\t%s\trs%s\t%s\n", chrom, start, end, name, exception);
}



void doCheck(char *chromName)
/* check each row for exceptions */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];
int loc_type = 0;
int alleleLen = 0;
int span = 0;
char *subString = NULL;
int slashCount = 0;
char *allele = NULL;

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
if (!hTableExists(tableName)) return;

verbose(1, "chrom = %s\n", chromName);
safef(query, sizeof(query), 
    "select snp_id, chromStart, chromEnd, loc_type, class, observed, refUCSC, refUCSCReverseComp, allele, orientation from %s", tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    loc_type = sqlUnsigned(row[3]);

    /* SingleClass */
    if (sameString(row[4], "single"))
        {
        if (loc_type == 1)
            writeToExceptionFile(chromName, row[1], row[2], row[0], "SingleClassRangeLocType");
        if (loc_type == 3)
            writeToExceptionFile(chromName, row[1], row[2], row[0], "SingleClassBetweenLocType");
	if (loc_type == 2)
	    {
	    allele = cloneString(row[8]);
	    /* allele from NCBI should be reverse complemented already */
	    subString = strstr(row[5], allele);
            if (subString == NULL)
	        {
	        if (sameString(row[9], "0"))
                    writeToExceptionFile(chromName, row[1], row[2], row[0], 
		                         "SingleClassWrongObservedPositiveStrand");
		else
                    writeToExceptionFile(chromName, row[1], row[2], row[0], 
		                         "SingleClassWrongObservedNegativeStrand");
		}
	    }
	}

    /* NamedClassWrongLocType */
    /* need to reconsider this */
    if (sameString(row[4], "named"))
        {
	subString = strstr(row[5], "LARGEINSERTION");
	if (subString != NULL && loc_type != 3)
            writeToExceptionFile(chromName, row[1], row[2], row[0], "NamedClassWrongLocType");
        subString = strstr(row[5], "LARGEDELETION");
	if (subString != NULL && loc_type != 1)
            writeToExceptionFile(chromName, row[1], row[2], row[0], "NamedClassWrongLocType");
        }

    /* DeletionClass */
    if (sameString(row[4], "deletion") && loc_type < 3)
        {
        /* DeletionClassWrongObservedSize */
	alleleLen = strlen(row[5]);
	alleleLen = alleleLen - 2;
	span = sqlUnsigned(row[2]) - sqlUnsigned(row[1]);
	if (alleleLen != span)
	    {
            writeToExceptionFile(chromName, row[1], row[2], row[0], "DeletionClassWrongObservedSize");
	    continue;
	    }

	/* DeletionClassWrongObserved */
        subString = cloneString(row[5]);
	if (strlen(subString) < 2) continue;
	if (subString[0] != '-' || subString[1] != '/') continue;
	slashCount = chopString(subString, "/", NULL, 0);
	if (slashCount > 1) continue;
	subString = subString + 2;
	if (!sameString(subString, row[6]) && !sameString(subString, row[7]))
            writeToExceptionFile(chromName, row[1], row[2], row[0], "DeletionClassWrongObserved");
	}


    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* read chrN_snpTmp, log exceptions */
{
struct slName *chromList, *chromPtr;

if (argc != 2)
    usage();

snpDb = argv[1];
hSetDb(snpDb);
chromList = hAllChromNamesDb(snpDb);

exceptionFileHandle = mustOpen("snpCheckClassAndObserved.exceptions", "w");

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    doCheck(chromPtr->name);

carefulClose(&exceptionFileHandle);
return 0;
}
