/* snpException.c - Get exceptions to snp invariant rules */
#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "obscure.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "dnaseq.h"
#include "hdb.h"
#include "memalloc.h"
#include "hash.h"
#include "jksql.h"
#include "snp.h"
#include "snpExceptions.h"

static char const rcsid[] = "";

void usage()
/* Explain usage and exit. */
{
errAbort("snpException - Get exceptions to a snp invariant rule.\n"
	 "Usage:  \tsnpException database exceptionId fileBase\n"
	 "Example:\tsnpException hg17     1           hg17snpException\n"
	 "where the exceptionId is the primary key from the snpExceptions table,\n"
	 "\tand using exceptionId=0 will process all invariant rules. \n");
}

struct slName *getChromList()
/* Get list of all chromosomes. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row = NULL;
struct slName *list = NULL;
struct slName *el = NULL;
char  *queryString = "select chrom from chromInfo ";
/*                     "where    chrom not like '%random' " */
/*                     "where    chrom = 'chrY' "*/
/*                     "order by size desc";*/
sr = sqlGetResult(conn, queryString);
while ((row=sqlNextRow(sr)))
    {
    el = newSlName(row[0]);
    slAddHead(&list, el);
    }
slReverse(&list);  /* Restore order */
sqlFreeResult(&sr);
hFreeConn(&conn);
return list;
}

struct snpExceptions *getExceptionList(int inputExceptionId)
/* Get list of all exceptions to be tested. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row = NULL;
struct snpExceptions *list = NULL;
struct snpExceptions *el = NULL;
char   queryString[256] = "select * from snpExceptions";

if (inputExceptionId>0)
    safef(queryString, sizeof(queryString), 
	  "select * from snpExceptions where exceptionId=%d", inputExceptionId);

sr = sqlGetResult(conn, queryString);
while ((row=sqlNextRow(sr)) != NULL )
    {
    el = snpExceptionsLoad(row);
    slAddHead(&list, el);
    }
slReverse(&list);  /* Restore order */
sqlFreeResult(&sr);
hFreeConn(&conn);
return list;
}

boolean isLargeInDel(char *observed)
/* check to see if an allele was written as LARGEDELETION or LARGEINSERTION */
{
if (strstr(observed,"LARGE"))
    return FALSE;
else
    return TRUE;
}

void getWobbleBase(char *dna, char* wobble)
/* return the first wobble base not found in the input string */
{
char iupac[]="nxbdhryumwskv";
int  offset = strcspn(dna, iupac);

strncpy(wobble, dna+offset, 1);
}

void getInvariants(struct snpExceptions *exceptionList, struct slName *chromList, char *fileBase)
/* write list of invariants to output file */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult     *sr;
char                  query[1024];
char                **row = NULL;
struct snpExceptions *el    = NULL;
struct slName        *chrom = NULL;
unsigned long int     invariantCount;
char                  thisFile[64];
FILE                 *outFile;
int                   colCount, i;

for (el=exceptionList; el!=NULL; el=el->next)
    {
    safef(thisFile,sizeof(thisFile),"%s.%u.bed",fileBase,el->exceptionId);
    outFile = mustOpen(thisFile,"w");
    invariantCount = 0;
    for (chrom=chromList; chrom!=NULL; chrom=chrom->next)
	{
	safef(query,sizeof(query),"%s and chrom='%s'", el->query, chrom->name);
	sr = sqlGetResult(conn, query);
	colCount = sqlCountColumns(sr);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    invariantCount++; 
	    fprintf(outFile,"%s\t%d", row[0], el->exceptionId);
	    for (i=1; i<colCount; i++)
		fprintf(outFile,"\t%s", row[i]);
	    fprintf(outFile,"\n");
	    }
	}
    carefulClose(&outFile);
    printf("Invariant %d has %lu exceptions, written to this file: %s\n", 
	   el->exceptionId, invariantCount, thisFile);
    safef(query,sizeof(query),"update snpExceptions set num=%lu where exceptionId=%d",
	  invariantCount, el->exceptionId);    
    sr=sqlGetResult(conn,query);
    }
}

int main(int argc, char *argv[])
/* error check, process command line input, and call getSnpDetails */
{
struct snpExceptions *exceptionList = NULL;
struct slName        *chromList = NULL;

if (argc != 4)
    {
    usage();
    return 1;
    }
hSetDb(argv[1]);
exceptionList=getExceptionList(atoi(argv[2]));
chromList=getChromList();
getInvariants(exceptionList, chromList, argv[3]);
return 0;
}
