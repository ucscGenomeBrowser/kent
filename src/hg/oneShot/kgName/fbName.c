/* fbName - builds association table between knownPep and gene common name. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hCommon.h"
#include "hdb.h"
#include "spDb.h"

char *database = "hg16";
void usage()
/* Explain usage and exit. */
{
errAbort(
  "fbName - builds association table between bdgpGenePep and gene common name\n"
  "usage:\n"
  "   fbName database referencePsl outAssoc\n"
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
safef(infoTable, sizeof(infoTable), "%sInfo","bdgpGene");
if (hTableExists(infoTable))
    {
    struct sqlConnection *conn = hAllocConn();
    char *symbol = NULL;
    char *ptr = strchr(name, '-');
    char query[256];
    char buf[64];
    if (ptr != NULL)
	*ptr = 0;
    sqlSafef(query, sizeof(query),
	  "select symbol from %s where bdgpName = '%s';", infoTable, name);
    symbol = sqlQuickQuery(conn, query, buf, sizeof(buf));
    hFreeConn(&conn);
    if (symbol != NULL)
	{
	char *ptr = stringIn("{}", symbol);
	if (ptr != NULL)
	    *ptr = 0;
	freeMem(name);
	name = cloneString(symbol);
	}
    }
return(name);
}

char *getSwiss( struct sqlConnection *conn , char *id)
{
char cond_str[256];

sqlSafefFrag(cond_str, sizeof cond_str, "mrnaID='%s'", id);
return sqlGetField(conn, database, "spMrna", "spID", cond_str);
}

void fbName(char *database, char *protDb,  char *refPsl, char *outAssoc)
/* fbName - builds association table between knownPep and gene common name. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlConnection *conn2 = sqlConnect("swissProt");
char *words[1], **row;
FILE *f = mustOpen(outAssoc, "w");
struct lineFile *pslLf = pslFileOpen(refPsl);
int count = 0, found = 0;
char query[256];
struct psl *psl;

while ((psl = pslNext(pslLf)) != NULL)
    fprintf(f,"%s\t%s\t%s:%d-%d\n",psl->qName, lookupName(conn,psl->qName), 
	psl->tName, psl->tStart, psl->tEnd);
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
fbName(argv[1], protDbName, argv[2], argv[3]);
return 0;
}
