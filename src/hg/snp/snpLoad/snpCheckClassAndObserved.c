/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* snpCheckClassAndObserved
 * Use UCSC chromInfo.  
 *
 * Check class for annotations:
 * SingleClassBetweenLocType
 * SingleClassRangeLocType
 * NamedClassWrongLocType
 *
 * Check observed for annotations:
 * ObservedNotAvailable (obsolete)
 * ObservedWrongFormat
 * ObservedWrongSize
 * ObservedMismatch
 *
 * Also note RangeSubstitutionLocTypeExactMatch
 *
 * Also note SingleClassTriAllelic and SingleClassQuadAllelic
 */


#include "common.h"
#include "hdb.h"


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

void writeToExceptionFile(char *chrom, int start, int end, int name, char *exception)
{
fprintf(exceptionFileHandle, "%s\t%d\t%d\trs%d\t%s\n", chrom, start, end, name, exception);
}

void checkClass(char *chromName, int start, int end, int snp_id, int loc_type, char *class, char *observed)
{
char *subString = NULL;
/* SingleClass should be loc_type 2 */
if (sameString(class, "single"))
    {
    if (loc_type == 1 || loc_type == 4 || loc_type == 5 || loc_type == 6)
        writeToExceptionFile(chromName, start, end, snp_id, "SingleClassRangeLocType");
    if (loc_type == 3)
        writeToExceptionFile(chromName, start, end, snp_id, "SingleClassBetweenLocType");
    }

if (sameString(class, "named"))
    {
    subString = strstr(observed, "LARGEINSERTION");
    if (subString != NULL && loc_type != 3)
            writeToExceptionFile(chromName, start, end, snp_id, "NamedClassWrongLocType");
    subString = strstr(observed, "LARGEDELETION");
    if (subString != NULL && loc_type != 1)
        writeToExceptionFile(chromName, start, end, snp_id, "NamedClassWrongLocType");
    }
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

boolean iupac(char *observed)
{
int pos = strspn(observed, "ACGT-/");
if (pos != strlen(observed)) 
    {
    verbose(1, "IUPAC detected in %s\n", observed);
    return TRUE;
    }
return FALSE;
}

boolean checkObservedFormat(char *chromName, int start, int end, int snp_id, int loc_type, char *class, char *observed)
/* return TRUE if format is valid so downstream checks can continue */
{
/* Single class */
if (sameString(class, "single"))
    {
    if (quadAllelic(observed))
        {
        writeToExceptionFile(chromName, start, end, snp_id, "SingleClassQuadAllelic");
        return TRUE;
	}
    if (triAllelic(observed))
        {
        writeToExceptionFile(chromName, start, end, snp_id, "SingleClassTriAllelic");
        return TRUE;
	}
    if (validSingleObserved(observed)) return TRUE;
    writeToExceptionFile(chromName, start, end, snp_id, "ObservedWrongFormat");
    return FALSE;
    }

/* In-dels */
/* First char should be dash, second char should be forward slash. */
/* Only one forward slash. */
/* No IUPAC. */
if (sameString(class, "deletion") || sameString(class, "insertion") || sameString(class, "in-del"))
    {
    int slashCount = chopString(observed, "/", NULL, 0);

    if (strlen(observed) < 2 || slashCount > 2 || observed[0] != '-' || observed[1] != '/' || iupac(observed))
        {
        writeToExceptionFile(chromName, start, end, snp_id, "ObservedWrongFormat");
        return FALSE;
        }
    return TRUE;
    }

/* Named. */
if (sameString(class, "named"))
    {
    if (observed[0] != '(')
        {
        writeToExceptionFile(chromName, start, end, snp_id, "ObservedWrongFormat");
	return FALSE;
	}
    return TRUE;
    }

/* Mixed. */
if (sameString(class, "mixed"))
    {
    int slashCount = chopString(observed, "/", NULL, 0);
    if (strlen(observed) < 2 || slashCount < 3 || observed[0] != '-' || observed[1] != '/' || iupac(observed))
        {
        writeToExceptionFile(chromName, start, end, snp_id, "ObservedWrongFormat");
        return FALSE;
        }
    return TRUE;
    }

/* Microsat. */
return TRUE;
}

boolean checkObservedSize(char *chromName, int start, int end, int snp_id, int loc_type, char *class, char *observed)
/* This only applies to loc_type range. */
{
int observedLen = strlen(observed) - 2;
int span = end - start;

if (loc_type == 3) return TRUE;

/* start with simple deletions */
/* deletions can be range (loc_type 1) or exact (loc_type 2) */
if (sameString(class, "deletion") && (loc_type == 1 || loc_type == 2))
    {
    if (observedLen != span)
        {
        writeToExceptionFile(chromName, start, end, snp_id, "ObservedWrongSize");
	return FALSE;
	}
    return TRUE;
    }

/* loc_type 4,5,6 */
/* only check for class = 'in-del' */
if (!sameString(class, "in-del")) return TRUE;
if (loc_type == 4 && observedLen >= span)
    {
    writeToExceptionFile(chromName, start, end, snp_id, "ObservedWrongSize");
    return FALSE;
    }
if (loc_type == 5 && observedLen != span)
    {
    writeToExceptionFile(chromName, start, end, snp_id, "ObservedWrongSize");
    return FALSE;
    }

if (loc_type == 6 && observedLen <= span)
    {
    writeToExceptionFile(chromName, start, end, snp_id, "ObservedWrongSize");
    return FALSE;
    }
return TRUE;
}


void checkObservedAgainstReference(char *chromName, int start, int end, int snp_id, int loc_type, char *class, 
                                   char *observed, char *refUCSC, char *refUCSCReverseComp, int orientation)
{
char *refAllele = NULL;
char *subString = NULL;

if (orientation == 0)
   refAllele = cloneString(refUCSC);
else
   refAllele = cloneString(refUCSCReverseComp);

if (sameString(class, "single") && loc_type == 2 && strlen(refUCSC) == 1)
    {
    subString = strstr(observed, refAllele);
    if (subString == NULL)
        writeToExceptionFile(chromName, start, end, snp_id, "ObservedMismatch");
    return;
    }

if (sameString(class, "deletion"))
    {
    subString = cloneString(observed);
    subString = subString + 2;
    if (!sameString(subString, refAllele))
        writeToExceptionFile(chromName, start, end, snp_id, "ObservedMismatch");
    return;
    }

if (sameString(class, "in-del") && loc_type == 5)
    {
    subString = cloneString(observed);
    subString = subString + 2;
    if (sameString(subString, refAllele))
        writeToExceptionFile(chromName, start, end, snp_id, "RangeSubstitutionLocTypeExactMatch");
    }

/* todo here: check rangeInsertion and rangeDeletion for substring matches at start and end of observed */
}

void doCheckWithLocType(char *chromName)
/* check each row for exceptions */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];

int snp_id = 0;
int chromStart = 0;
int chromEnd = 0;
int loc_type = 0;
char *class = NULL;
char *observed = NULL;
char *refUCSC = NULL;
char *refUCSCReverseComp = NULL;
int orientation = 0;

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
if (!hTableExists(tableName)) return;

verbose(1, "chrom = %s\n", chromName);
sqlSafef(query, sizeof(query), 
    "select snp_id, chromStart, chromEnd, loc_type, class, observed, refUCSC, refUCSCReverseComp, orientation from %s", tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    snp_id = sqlUnsigned(row[0]);
    chromStart = sqlUnsigned(row[1]);
    chromEnd = sqlUnsigned(row[2]);
    loc_type = sqlUnsigned(row[3]);
    class = cloneString(row[4]);
    observed = cloneString(row[5]);
    refUCSC = cloneString(row[6]);;
    refUCSCReverseComp = cloneString(row[7]);
    orientation = sqlUnsigned(row[8]);

    checkClass(chromName, chromStart, chromEnd, snp_id, loc_type, class, observed);

    if (sameString(observed, "unknown") || sameString(observed, "lengthTooLong"))
        {
        writeToExceptionFile(chromName, chromStart, chromEnd, snp_id, "ObservedNotAvailable");
	continue;
	}

    if(!checkObservedFormat(chromName, chromStart, chromEnd, snp_id, loc_type, class, observed)) continue;
    if(!checkObservedSize(chromName, chromStart, chromEnd, snp_id, loc_type, class, observed)) continue;
    checkObservedAgainstReference(chromName, chromStart, chromEnd, snp_id, loc_type, class, observed, refUCSC, refUCSCReverseComp, orientation);
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

verbose(1, "checking class and observed ...\n");
for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    doCheckWithLocType(chromPtr->name);

carefulClose(&exceptionFileHandle);
return 0;
}
