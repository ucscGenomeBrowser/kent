/* snpExceptions - read chrN_snp125 tables, write to snp125Exceptions table */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"

#include "hdb.h"
#include "snp125.h"
#include "snp125Exceptions.h"


char *database = NULL;
static struct slName *chromList = NULL;
struct snp125 *listExact=NULL, *listBetween=NULL, *listRange=NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpExceptions - read chrN_snp125 tables, write to snp125Exceptions table\n"
    "usage:\n"
    "    snpExceptions database\n");
}

void createTable()
/* create the table one time, load each time */
{
struct sqlConnection *conn = hAllocConn();
snp125ExceptionsTableCreate(conn);
hFreeConn(&conn);
}

void readSnps(char *chromName)
/* slurp in chrN_snp125 */
{
struct snp125 *list=NULL, *el = NULL;
char tableName[64];
char query[512];
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
int count = 0;

verbose(1, "reading snps...\n");
strcpy(tableName, "chr");
strcat(tableName, chromName);
strcat(tableName, "_snp125");

if (!sqlTableExists(conn, tableName))
    return;

/* not checking chrom */
sqlSafef(query, sizeof(query), "select chrom, chromStart, chromEnd, name, strand, refNCBI, refUCSC, "
                            "observed, class, locType from %s", tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(el);
    // el->chrom = chromName;
    el->chrom = cloneString(row[0]);
    // el->score = 0;
    el->chromStart = atoi(row[1]);
    el->chromEnd = atoi(row[2]);
    el->name = cloneString(row[3]);
    strcpy(el->strand, row[4]);
    el->refNCBI = cloneString(row[5]);
    el->refUCSC = cloneString(row[6]);
    el->observed = cloneString(row[7]);
    el->class = cloneString(row[8]);
    el->locType = cloneString(row[9]);
    if (sameString(el->locType, "exact")) slAddHead(&listExact,el);
    else if (sameString(el->locType, "between")) slAddHead(&listBetween,el);
    else if (sameString(el->locType, "range")) slAddHead(&listRange,el);
    count++;
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
verbose(1, "%d snps found\n", count);
slReverse(&listExact);
slReverse(&listBetween);
slReverse(&listRange);
}

void writeOneException(FILE *f, struct snp125 *el, char exception[])
{
verbose(5, "%s:%d-%d (%s) %s\n", el->chrom, el->chromStart, el->chromEnd, el->name, exception);
fprintf(f, "%s\t", el->chrom);
fprintf(f, "%d\t", el->chromStart);
fprintf(f, "%d\t", el->chromEnd);
fprintf(f, "%s\t", el->name);
fprintf(f, "%s\t", exception);
fprintf(f, "\n");
}

int writeSizeExceptions(FILE *f)
/* size should match locType */
{
struct snp125 *el = NULL;
int count = 0;

for (el = listExact; el != NULL; el = el->next)
    if (el->chromEnd > el->chromStart + 1) 
        {
        writeOneException(f, el, "ExactLocTypeWrongSize");
        count++;
        }

for (el = listBetween; el != NULL; el = el->next)
    if (el->chromEnd != el->chromStart) 
        {
        writeOneException(f, el, "BetweenLocTypeWrongSize");
        count++;
        }

for (el = listRange; el != NULL; el = el->next)
    if (el->chromEnd <= el->chromStart + 1) 
        {
        writeOneException(f, el, "RangeLocTypeWrongSize");
        count++;
	}
return count;
}

int writeClassExceptions(FILE *f)
/* check listBetween and listRange for class = single */
{
struct snp125 *el = NULL;
int count = 0;

for (el = listBetween; el != NULL; el = el->next)
    if (sameString(el->class, "single"))
        {
        writeOneException(f, el, "SingleClassWrongLocType");
	count++;
	}

for (el = listRange; el != NULL; el = el->next)
    if (sameString(el->class, "single"))
        {
        writeOneException(f, el, "SingleClassWrongLocType");
	count++;
        }
return count;
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
    if (sameString(observed, "n/a")) return TRUE;
    if (sameString(observed, "A/C")) return TRUE;
    if (sameString(observed, "A/G")) return TRUE;
    if (sameString(observed, "A/T")) return TRUE;
    if (sameString(observed, "C/G")) return TRUE;
    if (sameString(observed, "C/T")) return TRUE;
    if (sameString(observed, "G/T")) return TRUE;
    return FALSE;
}


int writeSingleObservedExceptions(FILE *f)
/* There are 3 exceptions here:
   SingleClassWrongObserved, SimpleClassTriAllelic, SimpleClassQuadAllelic */
{
struct snp125 *el = NULL;
int count = 0;

for (el = listExact; el != NULL; el = el->next)
    {
    if (!sameString(el->class, "single")) continue;
    if (quadAllelic(el->observed))
        {
        writeOneException(f, el, "SingleClassQuadAllelic");
	count++;
	continue;
        }
    if (triAllelic(el->observed))
        {
        writeOneException(f, el, "SingleClassTriAllelic");
	count++;
	continue;
        }
    if (!validSingleObserved(el->observed))
        {
        writeOneException(f, el, "SingleClassWrongObserved");
	count++;
	}
    }
return count;
}

int writeDeletionObservedExceptions(FILE *f)
/* Read listRange for class = 'deletion'. */ 
/* Might there be other exceptions in listRange? */
{
struct snp125 *el = NULL;
int rangeSize = 0;
int observedSize = 0;
int count = 0;

for (el = listRange; el != NULL; el = el->next)
    {
    if (!sameString(el->observed, "deletion")) continue;
    if (sameString(el->observed, "n/a")) continue;
    if (strlen(el->observed) < 2)
        {
	writeOneException(f, el, "DeletionClassWrongObserved");
	count++;
	continue;
	}
    if (el->observed[0] == '-' && el->observed[1] == '/') 
        {
	/* check sizes */
	rangeSize = el->chromEnd - el->chromStart;
        observedSize = strlen(el->observed) - 2;
	if (rangeSize != observedSize)
	    {
	    writeOneException(f, el, "DeletionClassWrongObserved");
	    count++;
	    }
	continue;
	}
    writeOneException(f, el, "DeletionClassWrongObserved");
    count++;
    }
return count;
}

int writeReferenceObservedExceptions(FILE *f)
/* Exceptions RefNCBINotInObserved and RefUCSCNotInObserved for listExact, class = 'single'. */
/* Also check that refNCBI matches observed for listRange, class = 'deletion'. */
/* This looks pretty good for positive strand, not so for negative strand. */
/* Correspondence between refNCBI and refUCSC is very odd for listRange, class = 'deletion'. */
/* Could also check that observed is '-' for listBetween. */
{
struct snp125 *el = NULL;
char *chopString = NULL;
int count = 0;

for (el = listExact; el != NULL; el = el->next)
    {
    if (!sameString(el->observed, "single")) continue;
    if (sameString(el->observed, "n/a")) continue;
    if (!sameString(el->refNCBI, "n/a") && !strstr(el->observed, el->refNCBI))
        {
        writeOneException(f, el, "RefNCBINotInObserved");
	count++;
	}
    if (!strstr(el->observed, el->refUCSC))
        {
        writeOneException(f, el, "RefUCSCNotInObserved");
	count++;
	}
    }

for (el = listRange; el != NULL; el = el->next)
    {
    if (!sameString(el->class, "deletion")) continue;
    if (sameString(el->observed, "n/a")) continue;
    chopString = chopPrefixAt(el->observed, '/');
    if (!sameString(chopString, el->refNCBI))
        {
        writeOneException(f, el, "RefNCBINotInObserved");
	count++;
	}
    }
return count;
}

void loadDatabase(FILE *f, char *fileName)
{
struct sqlConnection *conn = hAllocConn();
hgLoadNamedTabFile(conn, ".", "snp125Exceptions", fileName, &f);
hFreeConn(&conn);
}


int main(int argc, char *argv[])
/* Write chrN_snp125Exceptions.tab file for each chrom. */
{
struct slName *chromPtr;
FILE *f;
char fileName[64];
int total = 0;

if (argc != 2)
    usage();

database = argv[1];
hSetDb(database);

/* create output table */
createTable();

chromList = hAllChromNamesDb(database);

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    stripString(chromPtr->name, "chr");
    }

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    verbose(1, "chrom = %s\n", chromPtr->name);
    readSnps(chromPtr->name);
    if (listExact == NULL && listBetween == NULL && listRange == NULL) 
        {
	printf("no SNPs for chr%s\n", chromPtr->name);
        verbose(1, "---------------------------------------\n");
	continue;
	}

    strcpy(fileName, "chr");
    strcat(fileName, chromPtr->name);
    strcat(fileName, "_snp125Exceptions");
    f = hgCreateTabFile(".", fileName);

    total = 0;
    total = total + writeSizeExceptions(f);
    total = total + writeClassExceptions(f);
    total = total + writeSingleObservedExceptions(f);
    total = total + writeDeletionObservedExceptions(f);
    total = total + writeReferenceObservedExceptions(f);
    verbose(1, "%d exceptions found\n", total);

    loadDatabase(f, fileName);
    slFreeList(&listExact);
    slFreeList(&listBetween);
    slFreeList(&listRange);
    verbose(1, "---------------------------------------\n");
    }

return 0;
}
