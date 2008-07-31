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

static char const rcsid[] = "$Id: snpException.c,v 1.4.188.1 2008/07/31 05:21:42 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort("snpException - Get exceptions to a snp invariant rule.\n"
	 "Usage:  \tsnpException database exceptionId fileBase\n"
	 "Example:\tsnpException hg17     1           hg17snpException\n"
	 "where the exceptionId is the primary key from the snpExceptions table,\n"
	 "\tand using exceptionId=0 will process all invariant rules. \n");
}

struct slName *getChromList(char *db)
/* Get list of all chromosomes. */
{
struct sqlConnection *conn  = hAllocConn(db);
struct sqlResult     *sr    = NULL;
char                **row   = NULL;
struct slName        *list  = NULL;
struct slName        *el    = NULL;
char                 *query = "select chrom from chromInfo";

sr = sqlGetResult(conn, query);
while ((row=sqlNextRow(sr))!=NULL)
    {
    el = newSlName(row[0]);
    slAddHead(&list, el);
    }
slReverse(&list);
sqlFreeResult(&sr);
hFreeConn(&conn);
return list;
}

struct slName *getGroupList(char *db, char *group)
/* Get list of all rsIds from where clause. */
{
struct sqlConnection *conn  = hAllocConn(db);
struct sqlResult     *sr    = NULL;
char                **row   = NULL;
struct slName        *list  = NULL;
struct slName        *el    = NULL;
char                  query[256];

safef(query,sizeof(query),"select name from snp %s", group);
sr = sqlGetResult(conn, query);
while ((row=sqlNextRow(sr))!=NULL)
    {
    el = newSlName(row[0]);
    slAddHead(&list, el);
    }
slReverse(&list);
sqlFreeResult(&sr);
hFreeConn(&conn);
return list;
}

struct snpExceptions *getExceptionList(char *db, int inputExceptionId)
/* Get list of all exceptions to be tested. */
{
struct sqlConnection *conn       = hAllocConn(db);
struct sqlResult     *sr         = NULL;
char                **row        = NULL;
struct snpExceptions *list       = NULL;
struct snpExceptions *el         = NULL;
char                  query[256] = "select * from snpExceptions";

if (inputExceptionId>0)
    safef(query, sizeof(query), 
	  "select * from snpExceptions where exceptionId=%d", inputExceptionId);
sr = sqlGetResult(conn, query);
while ((row=sqlNextRow(sr))!=NULL)
    {
    el = snpExceptionsLoad(row);
    slAddHead(&list, el);
    }
slReverse(&list);
sqlFreeResult(&sr);
hFreeConn(&conn);
return list;
}

void getInvariants(char *db, struct snpExceptions *exceptionList, 
		   struct slName *chromList, char *fileBase)
/* write list of invariants to output file */
{
struct sqlConnection *conn     = hAllocConn(db);
struct sqlResult     *sr       = NULL;
struct snpExceptions *el       = NULL;
struct slName        *chrom    = NULL;
char                **row      = NULL;
char                  query[1024];
unsigned long int     invariantCount;
char                  thisFile[64];
FILE                 *outFile;
int                   colCount, i;
char                  idString[3];

for (el=exceptionList; el!=NULL; el=el->next)
    {
    if (el->exceptionId<10)
	safef(idString,sizeof(idString), "0%d", el->exceptionId);
    else
	safef(idString,sizeof(idString), "%d",  el->exceptionId);
    invariantCount = 0;
    if (startsWith("select",el->query))
	{
	safef(thisFile, sizeof(thisFile), "%s.%s.bed", fileBase, idString);
	outFile = mustOpen(thisFile, "w");
	fprintf(outFile, "# exceptionId:\t%d\n# query:\t%s;\n", el->exceptionId, el->query);
	for (chrom=chromList; chrom!=NULL; chrom=chrom->next)
	    {
	    fflush(outFile); /* to keep an eye on output progress */
	    safef(query, sizeof(query),
		  "%s and chrom='%s'", el->query, chrom->name);
	    sr = sqlGetResult(conn, query);
	    colCount = sqlCountColumns(sr);
	    while ((row = sqlNextRow(sr))!=NULL)
		{
		invariantCount++; 
		fprintf(outFile, "%s", row[0]);
		for (i=1; i<colCount; i++)
		    fprintf(outFile, "\t%s", row[i]);
		fprintf(outFile, "\n");
		}
	    }
	}
    else if (startsWith("group",el->query))
	{
	struct slName *nameList = NULL;
	struct slName *name     = NULL;
	safef(thisFile, sizeof(thisFile), "%s.%s.bed", fileBase, idString);
	outFile = mustOpen(thisFile, "w");
	fprintf(outFile, "# exceptionId:\t%d\n# query:\t%s;\n", el->exceptionId, el->query);
	nameList = getGroupList(db, el->query);
	for (name=nameList; name!=NULL; name=name->next)
	    {
	    safef(query, sizeof(query),
		  "select chrom,chromStart,chromEnd,name,%d as score,class,locType,observed "
		  "from snp where name='%s'", el->exceptionId, name->name);
	    sr = sqlGetResult(conn, query);
	    colCount = sqlCountColumns(sr);
	    while ((row = sqlNextRow(sr))!=NULL)
		{
		invariantCount++; 
		fprintf(outFile, "%s", row[0]);
		for (i=1; i<colCount; i++)
		    fprintf(outFile, "\t%s", row[i]);
		fprintf(outFile, "\n");
		}
	    }
	}
    else
	{
	printf("Invariant %d has no query string\n", el->exceptionId);
	continue;
	}
    carefulClose(&outFile);
    printf("Invariant %d has %lu exceptions, written to this file: %s\n", 
	   el->exceptionId, invariantCount, thisFile);
    fflush(stdout);
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
char *db= argv[1];;
exceptionList=getExceptionList(db, atoi(argv[2]));
chromList=getChromList(db);
getInvariants(db, exceptionList, chromList, argv[3]);
return 0;
}
