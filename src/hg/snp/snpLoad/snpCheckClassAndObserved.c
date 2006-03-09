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
#include "hdb.h"

static char const rcsid[] = "$Id: snpCheckClassAndObserved.c,v 1.11 2006/03/09 05:25:17 heather Exp $";

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


boolean triAllelic(char *observed)
{
if (sameString(observed, "A/C/G")) return TRUE;
if (sameString(observed, "A/C/T")) return TRUE;
if (sameString(observed, "A/G/T")) return TRUE;
if (sameString(observed, "C/G/T")) return TRUE;
return FALSE;
}

boolean quadAllelic(char *observed)
{
if (sameString(observed, "A/C/G/T")) return TRUE;
return FALSE;
}

boolean validSingleObserved(char *observed)
{
if (sameString(observed, "A/C")) return TRUE;
if (sameString(observed, "A/G")) return TRUE;
if (sameString(observed, "A/T")) return TRUE;
if (sameString(observed, "C/G")) return TRUE;
if (sameString(observed, "C/T")) return TRUE;
if (sameString(observed, "G/T")) return TRUE;
return FALSE;
}

void checkSingleObserved(char *chromName, char *start, char *end, char *rsId, char *observed)
/* check for exceptions in single class */
{
if (quadAllelic(observed))
    {
    fprintf(exceptionFileHandle, 
            "%s\t%s\t%s\t%s\trs%s\t%s\n", chromName, start, end, rsId, "SingleClassQuadAllelic", observed);
    return;
    }

if (triAllelic(observed))
    {
    fprintf(exceptionFileHandle, 
            "%s\t%s\t%s\t%s\trs%s\t%s\n", chromName, start, end, rsId, "SingleClassTriAllelic", observed);
    return;
    }

if (validSingleObserved(observed)) return;

fprintf(exceptionFileHandle, 
        "%s\t%s\t%s\t%s\trs%s\t%s\n", chromName, start, end, rsId, "SingleClassWrongObserved", observed);

}

void checkIndelObserved(char *chromName, char *start, char *end, char *rsId, char *observed)
/* Check for exceptions in in-del class. */
/* First char should be dash, second char should be forward slash. */
/* To do: no IUPAC */
{
int slashCount = 0;

if (strlen(observed) < 2)
    {
    fprintf(exceptionFileHandle, 
            "%s\t%s\t%s\trs%s\t%s\n", chromName, start, end, rsId, "IndelClassTruncatedObserved");
    return;
    }

slashCount = chopString(observed, "/", NULL, 0);

if (slashCount > 2)
    fprintf(exceptionFileHandle, 
            "%s\t%s\t%s\t%s\trs%s\t%s\n", chromName, start, end, rsId, "IndelClassObservedWrongFormat", observed);

if (observed[0] != '-' || observed[1] != '/')
    fprintf(exceptionFileHandle, 
            "%s\t%s\t%s\t%s\trs%s\t%s\n", chromName, start, end, rsId, "IndelClassObservedWrongFormat", observed);
}

void checkMixedObserved(char *chromName, char *start, char *end, char *rsId, char *observed)
/* Check for exceptions in mixed class. */
/* should be multi-allelic */
/* To do: no IUPAC */
{
int slashCount = 0;

if (strlen(observed) < 2)
    {
    fprintf(exceptionFileHandle, 
            "%s\t%s\t%s\t%s\t%s\n", chromName, start, end, rsId, "MixedClassTruncatedObserved");
    return;
    }

if (observed[0] != '-' || observed[1] != '/')
    {
    fprintf(exceptionFileHandle, 
            "%s\t%s\t%s\t%s\trs%s\t%s\n", chromName, start, end, rsId, "MixedClassObservedWrongFormat", observed);
    return;
    }

slashCount = chopString(observed, "/", NULL, 0);
if (slashCount < 3)
    fprintf(exceptionFileHandle, 
            "%s\t%s\t%s\t%s\trs%s\t%s\n", chromName, start, end, rsId, "MixedClassObservedWrongFormat", observed);

}

void checkNamedObserved(char *chromName, char *start, char *end, char *rsId, char *observed)
/* Check for exceptions in named class. */
/* Should be (name). */
{

if (observed[0] != '(')
    fprintf(exceptionFileHandle, 
            "%s\t%s\t%s\t%s\trs%s\t%s\n", chromName, start, end, rsId, "NamedClassObservedWrongFormat", observed);
}


void doCheck(char *chromName)
/* simple checks: 
   "SingleClassQuadAllelic" and "SingleClassTriAllelic" 
   "SingleClassWrongObserved" 
   "IndelClassTruncatedObserved" and "IndelClassObservedWrongFormat" 
   "MixedClassTruncatedObserved" and "MixedClassObservedWrongFormat" 
   "NamedClassObservedWrongFormat" 
*/

{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
if (!hTableExists(tableName)) return;

verbose(1, "chrom = %s\n", chromName);
safef(query, sizeof(query), "select snp_id, chromStart, chromEnd, class, observed from %s", tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (sameString(row[4], "unknown")) continue;
    if (sameString(row[4], "lengthTooLong")) continue;
    if (sameString(row[3], "single"))
	checkSingleObserved(chromName, row[0], row[1], row[2], row[4]);
    if (sameString(row[3], "insertion") || sameString(row[3], "deletion"))
	checkIndelObserved(chromName, row[0], row[1], row[2], row[4]);
    if (sameString(row[3], "mixed"))
        checkMixedObserved(chromName, row[0], row[1], row[2], row[4]);
    if (sameString(row[3], "named"))
        checkNamedObserved(chromName, row[0], row[1], row[2], row[4]);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doCheckWithLocType(char *chromName)
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
 
for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    doCheckWithLocType(chromPtr->name);

carefulClose(&exceptionFileHandle);
return 0;
}
