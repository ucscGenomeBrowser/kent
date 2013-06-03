/* jksql - some code to get to know mySQL */
#include "common.h"
#include "dlist.h"
#include "hash.h"
#include <mysql.h>

static struct hash *bacAccessionHash;

struct sqlConnection
/* This is an item on a list of sql open connections. */
    {
    MYSQL *conn;			    /* Connection. */
    struct dlNode *node;		    /* Pointer to list node. */
    struct dlList *resultList;		    /* Any open results. */
    };

struct dlList *sqlOpenConnections;

struct sqlResult
/* This is an item on a list of sql open results. */
    {
    MYSQL_RES *result;			/* Result. */
    struct dlNode *node;		/* Pointer to list node we're on. */
    struct sqlConnection *conn;		/* Pointer to connection. */
    };

void sqlFreeResult(struct sqlResult **pRes)
/* Free up a result. */
{
struct sqlResult *res = *pRes;
if (res != NULL)
    {
    if (res->result != NULL)
	mysql_free_result(res->result);
    if (res->node != NULL)
	{
	dlRemove(res->node);
	freeMem(res->node);
	}
    freez(pRes);
    }
}

void sqlDisconnect(struct sqlConnection **pSc)
/* Close down connection. */
{
struct sqlConnection *sc = *pSc;
if (sc != NULL)
    {
    MYSQL *conn = sc->conn;
    struct dlList *resList = sc->resultList;
    struct dlNode *node = sc->node;
    if (resList != NULL)
	{
	struct dlNode *resNode, *resNext;
	for (resNode = resList->head; resNode->next != NULL; resNode = resNext)
	    {
	    struct sqlResult *res = resNode->val;
	    resNext = resNode->next;
	    sqlFreeResult(&res);
	    }
	freeDlList(&resList);
	}
    if (conn != NULL)
	{
	mysql_close(conn);
	}
    if (node != NULL)
	{
	dlRemove(node);
	freeMem(node);
	}
    freez(pSc);
    }
}

void sqlCleanupAll()
/* Cleanup all open connections and resources. */
{
if (sqlOpenConnections)
    {
    struct dlNode *conNode, *conNext;
    struct sqlConnection *conn;
    for (conNode = sqlOpenConnections->head; conNode->next != NULL; conNode = conNext)
	{
	conn = conNode->val;
	conNext = conNode->next;
	sqlDisconnect(&conn);
	}
    freeDlList(&sqlOpenConnections);
    }
}

void sqlInitTracking()
/* Initialize tracking and freeing of resources. */
{
if (sqlOpenConnections == NULL)
    {
    sqlOpenConnections = newDlList();
    atexit(sqlCleanupAll);
    }
}

struct sqlConnection *sqlConnect(char *database)
/* Connect to database. */
{
struct sqlConnection *sc;
MYSQL *conn;

sqlInitTracking();

AllocVar(sc);
sc->resultList = newDlList();
sc->node = dlAddValTail(sqlOpenConnections, sc);

if ((sc->conn = conn = mysql_init(NULL)) == NULL)
    errAbort("Couldn't connect to mySQL.");
if (mysql_real_connect(
	conn,
	NULL, /* host */
	NULL,	/* user name */
	NULL,	/* password */
	database, /* database */
	0,	/* port */
	NULL,	/* socket */
	0)	/* flags */  == NULL)
    {
    mysql_close(conn);
    errAbort("Couldn't connect to database %s.\n%s", 
	database, mysql_error(conn));
    }
return sc;
}

void sqlVaWarn(struct sqlConnection *sc, char *format, va_list args)
/* Default error message handler. */
{
MYSQL *conn = sc->conn;
if (format != NULL) {
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    }
warn("mySQL error %d: %s", mysql_errno(conn), mysql_error(conn));
}

void sqlWarn(struct sqlConnection *sc, char *format, ...)
/* Printf formatted error message that adds on sql 
 * error message. */
{
va_list args;
va_start(args, format);
sqlVaWarn(sc, format, args);
va_end(args);
}

void sqlAbort(struct sqlConnection  *sc, char *format, ...)
/* Printf formatted error message that adds on sql 
 * error message and abort. */
{
va_list args;
va_start(args, format);
sqlVaWarn(sc, format, args);
va_end(args);
noWarnAbort();
}


struct sqlResult *sqlQueryUse(struct sqlConnection *sc, char *query)
/* Returns NULL if result was empty.  Otherwise returns a structure
 * that you can do sqlRow() on. */
{
MYSQL *conn = sc->conn;
if (mysql_real_query(conn, query, strlen(query)) != 0)
    {
    sqlAbort(sc, "Can't start query:\n");
    return NULL;
    }
else
    {
    struct sqlResult *res;
    MYSQL_RES *resSet;
    if ((resSet = mysql_use_result(conn)) == NULL)
	{
	if (mysql_errno(conn) != 0)
	    {
	    sqlAbort(sc, "Can't use query:\n%s", query);
	    }
	else
	    return NULL;
	}
    AllocVar(res);
    res->conn = sc;
    res->result = resSet;
    res->node = dlAddValTail(sc->resultList, res);
    return res;
    }
}

struct ensFeature
/* An ensemble feature. */
    {
    struct ensFeature *next;
    char *tName;
    int tContig;
    int tStart, tEnd;
    int score;
    char strand;
    int type;
    char *typeName;
    int qStart, qEnd;
    char *qName;
    };

unsigned numString(char *s)
{
unsigned res = 0;
char c;

res = *s - '0';
while ((c = *(++s)) != 0)
    {
    res *= 10;
    res += c;
    }
return res;
}
    

struct ensFeature *featFromRow(MYSQL_RES *res, MYSQL_ROW row)
/* Turn feature from row representation to structure. */
{
struct ensFeature *ef;
MYSQL_FIELD *field;
char *s, *e;
static struct hash *typeNames = NULL;
struct hashEl *hel;

AllocVar(ef);
s = row[1];
e = strchr(s, '.');
if (e == NULL)
    ef->tContig = 0;
else
    {
    ef->tContig = numString(e+1);
    *e = 0;
    }
ef->tName = hashStoreName(bacAccessionHash, s);
ef->tStart = numString(row[2]);
ef->tEnd = numString(row[3]);
ef->score = numString(row[4]);
if (row[5][0] == '-')
    ef->strand = '-';
else
    ef->strand = '+';
ef->type = numString(row[5]);
if (typeNames == NULL)
    typeNames = newHash(0);
ef->typeName = hashStoreName(typeNames, row[6]);
ef->qStart = numString(row[7]);
ef->qEnd = numString(row[8]);
ef->qName = cloneString(s);
return ef;
}

struct ensFeature *ensFeatForBac(struct sqlConnection *conn, char *clone)
/* Get list of features associated with BAC clone. */
{
unsigned int field_count;
char query[256];
struct sqlResult *sr;
struct ensFeature *efList = NULL, *ef;
long startTime, endTime;

sqlSafef(query, sizeof query, "select * from feature where contig like '%s%%'", clone);
startTime = clock1000();
sr = sqlQueryUse(conn, query);
if (sr == NULL)
    printf("empty query\n");
else
    {
    MYSQL_RES *res = sr->result;
    MYSQL_ROW row;
    
    while ((row = mysql_fetch_row(res)) != NULL)
	{
	ef = featFromRow(res, row);
	slAddHead(&efList, ef);
	}
    endTime = clock1000();
    printf("count = %d time = %4.3f\n", slCount(efList), 0.001*(endTime-startTime));
    }
sqlFreeResult(&sr);
return efList;
}

struct slName *ensGenesInBacRange(struct sqlConnection *conn, char *bacName, int start, int end)
/* Get list of genes in bac. */
{
char query[512];
struct sqlResult *sr;
struct slName *geneList = 0, *gene;

sqlSafef(query, sizeof query,
  "SELECT transcript.gene "
  "FROM geneclone_neighbourhood,transcript,translation "
  "WHERE geneclone_neighbourhood.clone = '%s' "
  " AND transcript.gene = geneclone_neighbourhood.gene "
  " AND transcript.translation = translation.id "
  " AND translation.seq_start >= %d "
  " AND translation.seq_end < %d"  ,
  bacName, start, end);
sr = sqlQueryUse(conn, query);
if (sr != NULL)
    {
    MYSQL_RES *res = sr->result;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != NULL)
	{
	gene = newSlName(row[0]);
	slAddHead(&geneList, gene);
	}
    }
sqlFreeResult(&sr);
return geneList;
}

struct slName *ensGenesInBac(struct sqlConnection *conn, char *bacName)
/* Get list of genes in bac. */
{
char query[512];
struct sqlResult *sr;
struct slName *geneList = 0, *gene;

sqlSafef(query,
  "SELECT transcript.gene "
  "FROM geneclone_neighbourhood,transcript,translation "
  "WHERE geneclone_neighbourhood.clone = '%s' "
  " AND transcript.gene = geneclone_neighbourhood.gene "
  " AND transcript.translation = translation.id ",
  bacName);
sr = sqlQueryUse(conn, query);
if (sr != NULL)
    {
    MYSQL_RES *res = sr->result;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != NULL)
	{
	gene = newSlName(row[0]);
	slAddHead(&geneList, gene);
	}
    }
sqlFreeResult(&sr);
return geneList;
}

int main(int argc, char *argv[])
{
char *database = "ensdev";
struct sqlConnection *conn = NULL;
struct slName *geneNameList, *gn;
char *cloneName = "AC000001";

printf("Hello world\n");
if (argc > 1)
    database = argv[1];
bacAccessionHash = newHash(14);
conn = sqlConnect(database);
printf("Connected ok to %s database\n", 
	(database == NULL ? "default" : database));
ensFeatForBac(conn, cloneName);
geneNameList = ensGenesInBac(conn, cloneName);
printf("%d genes in %s\n", slCount(geneNameList), cloneName); 
for (gn = geneNameList; gn != NULL; gn = gn->next)
    printf("%s\n", gn->name);

sqlDisconnect(&conn);
}
