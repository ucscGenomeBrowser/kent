/* sgName - builds association table between sgbPep and gene common name. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hCommon.h"
#include "hdb.h"
#include "spDb.h"

char *database = "sacCer1";
void usage()
/* Explain usage and exit. */
{
errAbort(
  "sgName - builds association table between sgdPep and gene common name\n"
  "usage:\n"
  "   sgName database referencePsl outAssoc\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

char *protDbName = NULL;

char* lookupName(  struct sqlConnection *conn , char *id)
{
char *name = cloneString(id);
char infoTable[128];
safef(infoTable, sizeof(infoTable), "sgdToName");
if (hTableExists(infoTable))
    {
    struct sqlConnection *conn = hAllocConn();
    char *symbol = NULL;
    //char *ptr = strchr(name, '-');
    char query[256];
    char buf[64];
    //if (ptr != NULL)
//	*ptr = 0;
    sqlSafef(query, sizeof(query),
	  "select value from %s where name = '%s';", infoTable, name);
    if ((symbol = sqlQuickQuery(conn, query, buf, sizeof(buf))) != NULL)
	name = symbol;
    hFreeConn(&conn);
    }
return(name);
}

char *getSwiss( struct sqlConnection *conn , char *id)
{
char *name = cloneString(id);
char infoTable[128];
safef(infoTable, sizeof(infoTable), "sgdToSwissProt");
if (hTableExists(infoTable))
    {
    struct sqlConnection *conn = hAllocConn();
    char *symbol = NULL;
    //char *ptr = strchr(name, '-');
    char query[256];
    char buf[64];
    //if (ptr != NULL)
//	*ptr = 0;
    sqlSafef(query, sizeof(query),
	  "select value from %s where name = '%s';", infoTable, name);
    if ((symbol = sqlQuickQuery(conn, query, buf, sizeof(buf))) != NULL)
	name = symbol;
    hFreeConn(&conn);
    }
return(name);
}

void sgName(char *database, char *protDb,  char *refPsl, char *outAssoc)
/* sgName - builds association table between knownPep and gene common name. */
{
struct sqlConnection *conn = sqlConnect(database);
//struct sqlConnection *conn2 = sqlConnect("swissProt");
char *words[1], **row;
FILE *f = mustOpen(outAssoc, "w");
struct lineFile *pslLf = pslFileOpen(refPsl);
int count = 0, found = 0;
char query[256];
struct psl *psl;
char *swiss = NULL;

while ((psl = pslNext(pslLf)) != NULL)
    {
    fprintf(f,"%s\t%s\t%s:%d-%d\t",psl->qName, lookupName(conn,psl->qName), 
	psl->tName, psl->tStart, psl->tEnd);
    fprintf(f,"%s\n", swiss = getSwiss(conn, psl->qName));
    }
/*
while (lineFileRow(lf, words))
    {
    fprintf(f,"%s\t%s\n",words[0], lookupName(conn,words[0])); //, getSwiss(conn, words[0]));
    }
    */
hFreeConn(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
database=argv[1];
hSetDb(argv[1]);
protDbName = hPdbFromGdb(database);
sgName(argv[1], protDbName, argv[2], argv[3]);
return 0;
}
