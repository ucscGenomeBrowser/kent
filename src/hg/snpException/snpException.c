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

static char const rcsid[] = "$Id: snpException.c,v 1.2 2005/01/03 21:50:49 daryl Exp $";

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
struct sqlConnection *conn  = hAllocConn();
struct sqlResult     *sr    = NULL;
char                **row   = NULL;
struct slName        *list  = NULL;
struct slName        *el    = NULL;
char                 *query = "select chrom from chromInfo";

sr = sqlGetResult(conn, query);
while (row=sqlNextRow(sr))
    {
    el = newSlName(row[0]);
    slAddHead(&list, el);
    }
slReverse(&list);
sqlFreeResult(&sr);
hFreeConn(&conn);
return list;
}

struct snpExceptions *getExceptionList(int inputExceptionId)
/* Get list of all exceptions to be tested. */
{
struct sqlConnection *conn       = hAllocConn();
struct sqlResult     *sr         = NULL;
char                **row        = NULL;
struct snpExceptions *list       = NULL;
struct snpExceptions *el         = NULL;
char                  query[256] = "select * from snpExceptions";

if (inputExceptionId>0)
    safef(query, sizeof(query), 
	  "select * from snpExceptions where exceptionId=%d", inputExceptionId);
sr = sqlGetResult(conn, query);
while (row=sqlNextRow(sr))
    {
    el = snpExceptionsLoad(row);
    slAddHead(&list, el);
    }
slReverse(&list);
sqlFreeResult(&sr);
hFreeConn(&conn);
return list;
}

void getInvariants(struct snpExceptions *exceptionList, 
		   struct slName *chromList, char *fileBase)
/* write list of invariants to output file */
{
struct sqlConnection *conn  = hAllocConn();
struct sqlResult     *sr;
char                  query[1024];
char                **row   = NULL;
struct snpExceptions *el    = NULL;
struct slName        *chrom = NULL;
unsigned long int     invariantCount;
char                  thisFile[64];
FILE                 *outFile;
int                   colCount, i;

for (el=exceptionList; el!=NULL; el=el->next)
    {
    safef(thisFile, sizeof(thisFile), "%s.%d.bed", fileBase, el->exceptionId);
    outFile = mustOpen(thisFile, "w");
    fprintf(outFile, "# exceptionId:\t%d\n# query:\t%s;\n", el->exceptionId, el->query);
    fflush(outFile); /* to keep an eye on output progress */
    invariantCount = 0;
    for (chrom=chromList; chrom!=NULL; chrom=chrom->next)
	{
	safef(query, sizeof(query),
	      "%s and chrom='%s'", el->query, chrom->name);
	sr = sqlGetResult(conn, query);
	colCount = sqlCountColumns(sr);
	while (row = sqlNextRow(sr))
	    {
	    invariantCount++; 
	    fprintf(outFile, "%s\t%s\t%s\t%s\t%d", 
		    row[0], row[1], row[2], row[3], el->exceptionId);
	    for (i=4; i<colCount; i++)
		fprintf(outFile, "\t%s", row[i]);
	    fprintf(outFile, "\n");
	    }
	}
    carefulClose(&outFile);
    printf("Invariant %d has %lu exceptions, written to this file: %s\n", 
	   el->exceptionId, invariantCount, thisFile);
    safef(query, sizeof(query),
	  "update snpExceptions set num=%lu where exceptionId=%d",
	  invariantCount, el->exceptionId);    
    sr=sqlGetResult(conn, query); /* there's probably a better way to do this */
    }
}

int main(int argc, char *argv[])
/* error check, process command line input, and call getInvariants */
{
struct snpExceptions *exceptionList = NULL;
struct slName        *chromList     = NULL;

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
