/* snpExceptions - read chrN_snp125 tables, write to snp125Exceptions table */
#include "common.h"

#include "hdb.h"
#include "snp125.h"
#include "snp125Exceptions.h"

static char const rcsid[] = "$Id: snpExceptions.c,v 1.2 2006/01/13 05:34:54 heather Exp $";

char *database = NULL;
static struct slName *chromList = NULL;
struct snp125 *listSimple=NULL, *listDeletion=NULL, *listInsertion=NULL, *listRange=NULL;

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
safef(query, sizeof(query), "select chrom, chromStart, chromEnd, name, strand, refNCBI, refUCSC, "
                            "observed, class from %s", tableName);

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
    if (sameString(el->class, "simple")) slAddHead(&listSimple,el);
    else if (sameString(el->class, "deletion")) slAddHead(&listDeletion,el);
    else if (sameString(el->class, "insertion")) slAddHead(&listInsertion,el);
    else if (sameString(el->class, "range")) slAddHead(&listRange,el);
    count++;
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
verbose(1, "%d snps found\n", count);
slReverse(&listSimple);
slReverse(&listDeletion);
slReverse(&listInsertion);
slReverse(&listRange);
}

void writeOneException(FILE *f, struct snp125 *el, char exception[])
{
verbose(1, "%s:%d-%d (%s) %s\n", el->chrom, el->chromStart, el->chromEnd, el->name, exception);
fprintf(f, "%s\t", el->chrom);
fprintf(f, "%d\t", el->chromStart);
fprintf(f, "%d\t", el->chromEnd);
fprintf(f, "%s\t", el->name);
fprintf(f, "%s\t", exception);
fprintf(f, "\n");
}

void writeSizeExceptions(FILE *f)
{
struct snp125 *el = NULL;
int size = 0;

for (el = listSimple; el != NULL; el = el->next)
    {
    size = el->chromEnd - el->chromStart;
    if (size != 1) 
       writeOneException(f, el, "SimpleClassWrongSize");
    }

for (el = listInsertion; el != NULL; el = el->next)
    {
    size = el->chromEnd - el->chromStart;
    if (size != 0) 
        writeOneException(f, el, "InsertionClassWrongSize");
    }

for (el = listDeletion; el != NULL; el = el->next)
    {
    size = el->chromEnd - el->chromStart;
    if (size == 0) 
        writeOneException(f, el, "DeletionClassWrongSize");
    }

for (el = listRange; el != NULL; el = el->next)
    {
    size = el->chromEnd - el->chromStart;
    if (size <= 1) 
        writeOneException(f, el, "RangeClassWrongSize");
    }
}

boolean triAllelic(char *observed)
{
    if (sameString(observed, "A/C/G")) return TRUE;
    if (sameString(observed, "A/C/T")) return TRUE;
    if (sameString(observed, "C/G/T")) return TRUE;
    return FALSE;
}

boolean quadAllelic(char *observed)
{
    if (sameString(observed, "A/C/G/T")) return TRUE;
    return FALSE;
}

boolean validSimpleObserved(char *observed)
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


void writeSimpleObservedExceptions(FILE *f)
/* There are 3 exceptions here:
   SimpleClassWrongObserved, SimpleClassTriAllelic, SimpleClassQuadAllelic */
{
struct snp125 *el = NULL;

for (el = listSimple; el != NULL; el = el->next)
    {
    if (quadAllelic(el->observed))
        {
        writeOneException(f, el, "SimpleClassQuadAllelic");
	continue;
        }
    if (triAllelic(el->observed))
        {
        writeOneException(f, el, "SimpleClassTriAllelic");
	continue;
        }
    if (!validSimpleObserved(el->observed))
        writeOneException(f, el, "SimpleClassWrongObserved");
    }
}

void writeDeletionObservedExceptions(FILE *f)
/* Look for DeletionClassWrongObserved */
{
struct snp125 *el = NULL;

for (el = listDeletion; el != NULL; el = el->next)
    {
    if (sameString(el->observed, "n/a")) continue;
    if (strlen(el->observed) < 2)
        {
	writeOneException(f, el, "DeletionClassWrongObserved");
	continue;
	}
    if (el->observed[0] == '-' && el->observed[1] == '/') continue;
    writeOneException(f, el, "DeletionClassWrongObserved");
    }
}

void writeReferenceExceptions(FILE *f)
/* RefNCBINotInObserved, RefUCSCNotInObserved. */
{
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
    if (listSimple == NULL && listDeletion == NULL && listInsertion == NULL && listRange == NULL) 
        {
	printf("no SNPs for chr%s\n", chromPtr->name);
        verbose(1, "---------------------------------------\n");
	continue;
	}

    strcpy(fileName, "chr");
    strcat(fileName, chromPtr->name);
    strcat(fileName, "_snp125Exceptions");
    f = hgCreateTabFile(".", fileName);

    writeSizeExceptions(f);
    writeSimpleObservedExceptions(f);
    writeDeletionObservedExceptions(f);

    loadDatabase(f, fileName);
    slFreeList(&listSimple);
    slFreeList(&listDeletion);
    slFreeList(&listInsertion);
    slFreeList(&listRange);
    verbose(1, "---------------------------------------\n");
    break;
    }

return 0;
}
