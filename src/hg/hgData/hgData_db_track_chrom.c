/* hgData_db_track_chrom - functions dealing with databases, tracks, and chromosomes. */
#include "common.h"
#include "hgData.h"
#include "hdb.h"
#include "chromInfo.h"
#include "trackDb.h"

static char const rcsid[] = "$Id: hgData_db_track_chrom.c,v 1.1.2.2 2009/01/09 07:41:36 mikep Exp $";

static struct dbDbClade *dbDbCladeLoad(char **row)
/* Load a dbDbClade from row fetched with select * from dbDb
 * from database.  Dispose of this with dbDbCladeFree(). */
{
struct dbDbClade *ret;

AllocVar(ret);
ret->name = cloneString(row[0]);
ret->description = cloneString(row[1]);
ret->organism = cloneString(row[2]);
ret->genome = cloneString(row[3]);
ret->scientificName = cloneString(row[4]);
ret->sourceName = cloneString(row[5]);
ret->clade = cloneString(row[6]);
ret->defaultPos = cloneString(row[7]);
ret->orderKey = sqlSigned(row[8]);
ret->priority = atof(row[9]);
return ret;
}

static void dbDbCladeFree(struct dbDbClade **pEl)
/* Free a single dynamically allocated dbDbClade such as created
 * with dbDbCladeLoad(). */
{
struct dbDbClade *el;

if ((el = *pEl) == NULL) return;
freeMem(el->name);
freeMem(el->description);
freeMem(el->organism);
freeMem(el->genome);
freeMem(el->scientificName);
freeMem(el->sourceName);
freeMem(el->clade);
freeMem(el->defaultPos);
freez(pEl);
}

void dbDbCladeFreeList(struct dbDbClade **pList)
/* Free a list of dynamically allocated dbDbClade's */
{
struct dbDbClade *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dbDbCladeFree(&el);
    }
*pList = NULL;
}

struct dbDbClade *hGetIndexedDbClade(char *db)
/* Get list of active databases and clade
 * Only get details for one 'db' unless NULL
 * in which case get all databases.
 * Dispose of this with dbDbCladeFreeList. */
{
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr = NULL;
char **row;
struct dbDbClade *dbList = NULL, *dbs;

/* Scan through dbDb table, loading into list */
if (db)
  {
    char query[1024];
    safef(query, sizeof(query), "SELECT name, description, organism, dbDb.genome, scientificName, sourceName, clade, defaultPos, orderKey, priority FROM dbDb,genomeClade WHERE dbDb.active = 1 and dbDb.genome = genomeClade.genome and dbDb.name=\"%s\"", db);
    sr = sqlGetResult(conn, query);
  }
else
  {
  sr = sqlGetResult(conn, "SELECT name, description, organism, dbDb.genome, scientificName, sourceName, clade, defaultPos, orderKey, priority FROM dbDb,genomeClade WHERE dbDb.active = 1 and dbDb.genome = genomeClade.genome ORDER BY clade,priority,orderKey");
  }
while ((row = sqlNextRow(sr)) != NULL)
    {
    dbs = dbDbCladeLoad(row);
    slAddHead(&dbList, dbs);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
slReverse(&dbList);
return dbList;
}

struct chromInfo *getAllChromInfo(char *db)
/* Query db.chromInfo for all chrom info. */
{
struct chromInfo *ci = NULL;
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr = NULL;
char **row = NULL;
sr = sqlGetResult(conn, "select * from chromInfo order by chrom desc");
while ((row = sqlNextRow(sr)) != NULL)
    {
    slSafeAddHead(&ci, chromInfoLoad(row));
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ci;
}


void printDb(struct dbDbClade *db)
// print an array of all active data in dbDb table
{
struct dbDbClade *t;
printf("\"dbColumns\" : [\"name\",\"description\",\"organism\",\"genome\",\"scientificName\",\"sourceName\",\"clade\",\"defaultPos\",\"orderKey\",\"priority\"],\n");
printf("\"db\" : [\n");
for (t = db ; t ; t = t->next)
	{
	printf("[%s,", quote(t->name));
	printf("%s,",  quote(t->description));
	printf("%s,",  quote(t->organism));
	printf("%s,",  quote(t->genome));
	printf("%s,",  quote(t->scientificName));
	printf("%s,",  quote(t->sourceName));
	printf("%s,",  quote(t->clade));
	printf("%s,",  quote(t->defaultPos));
	printf("%d,",  t->orderKey);
	printf("%f],\n", t->priority);
	}
printf("],\n");
}

void printOneDb(struct dbDbClade *db)
// print one db from dbDbClade table
{
printf("\"name\" : %s,\n", quote(db ? db->name : NULL));
printf("\"description\" : %s,\n", quote(db ? db->description : NULL));
printf("\"organism\" : %s,\n", quote(db ? db->organism : NULL));
printf("\"genome\" : %s,\n", quote(db ? db->genome: NULL));
printf("\"scientificName\" : %s,\n", quote(db ? db->scientificName : NULL));
printf("\"sourceName\" : %s,\n", quote(db ? db->sourceName : NULL));
printf("\"clade\" : %s,\n", quote(db ? db->sourceName : NULL));
printf("\"defaultPos\" : %s,\n", quote(db ? db->sourceName : NULL));
printf("\"orderKey\" : %d,\n", db->orderKey);
printf("\"priority\" : %f,\n", db->priority);
}

static void printDbOneColumn(struct dbDbClade *db, char *column)
// print an array of all values for one column in the db table
{
struct dbDbClade *t;
printf("\"%s\" : [", column);
for (t = db ; t ; t = t->next)
  {
  if (sameOk(column,"orderKey"))
    printf("%d,", t->orderKey);
  else if(sameOk(column,"priority"))
    printf("%f,", t->priority);
  else
    printf("%s,", sameOk(column, "name") ? quote(t->name) : 
      (sameOk(column, "description")   ? quote(t->description) : 
      (sameOk(column, "organism")      ? quote(t->organism) :
      (sameOk(column, "genome")        ? quote(t->genome) :
      (sameOk(column, "scientificName") ? quote(t->scientificName) :
      (sameOk(column, "sourceName")    ? quote(t->sourceName) : 
      (sameOk(column, "clade")         ? quote(t->clade) : 
      (sameOk(column, "defaultPos")    ? quote(t->defaultPos) : 
      ""))))))));
  }
printf("],\n");
}

void printDbByColumn(struct dbDbClade *db)
// print an array of all values in the db table by column
{
	printDbOneColumn(db, "name");
	printDbOneColumn(db, "description");
	printDbOneColumn(db, "organism");
	printDbOneColumn(db, "genome");
	printDbOneColumn(db, "scientificName");
	printDbOneColumn(db, "sourceName");
	printDbOneColumn(db, "clade");
	printDbOneColumn(db, "defaultPos");
	printDbOneColumn(db, "orderKey");
	printDbOneColumn(db, "priority");
}

void printTrackDb(struct trackDb *tdb)
// print a list of rows of trackDb data for every track
{
struct trackDb *t;
printf("\"trackDbColumns\" : [\"tableName\",\"type\",\"parentName\",\"shortLabel\",\"longLabel\",\"grp\"],\n");
printf("\"trackDb\" : [\n");
for (t = tdb ; t ; t = t->next)
    {
    printf("[%s,", quote(t->tableName));
    printf("%s,", quote(t->type));
    printf("%s,", quote(t->parentName));
    printf("%s,", quote(t->shortLabel));
    printf("%s,", quote(t->longLabel));
    printf("%s],\n", quote(t->grp));
    }
printf("],\n");
}

void printOneTrackDb(struct trackDb *tdb)
// print a single row of trackDb data
{
printf("\"tableName\" : %s,\n", quote(tdb ? tdb->tableName : NULL));
printf("\"type\" : %s,\n",      quote(tdb ? tdb->type : NULL));
printf("\"parentName\" : %s,\n",quote(tdb ? tdb->parentName : NULL));
printf("\"shortLabel\" : %s,\n",quote(tdb ? tdb->shortLabel : NULL));
printf("\"longLabel\" : %s,\n", quote(tdb ? tdb->longLabel : NULL));
printf("\"grp\" : %s,\n",       quote(tdb ? tdb->grp : NULL));
}

static void printTrackDbOneColumn(struct trackDb *db, char *column)
// print a column of trackDb data 
{
struct trackDb *t;
printf("\"%s\" : [", column);
for (t = db ; t ; t = t->next)
        printf(" %s,", sameOk(column, "tableName") ? quote(t->tableName) :
            (sameOk(column, "type") ? quote(t->type) :
            (sameOk(column, "parentName") ? quote(t->parentName) :
            (sameOk(column, "shortLabel") ? quote(t->shortLabel) :
            (sameOk(column, "longLabel") ? quote(t->longLabel) :
            (sameOk(column, "grp") ? quote(t->grp) : 
	    ""))))));
printf("]\n");
}

void printTrackDbByColumn(struct trackDb *db)
// print a list of trackDb data by column
{
printTrackDbOneColumn(db, "tableName");
printTrackDbOneColumn(db, "type");
printTrackDbOneColumn(db, "parentName");
printTrackDbOneColumn(db, "shortLabel");
printTrackDbOneColumn(db, "longLabel");
printTrackDbOneColumn(db, "grp");
}

void printChroms(struct chromInfo *ci)
// print a list of rows of chromInfo for every chrom
{
struct chromInfo *t;
if (!ci)
    return;
printf("\"chromsColumns\" : [\"chrom\",\"size\"],\n");
printf("\"chroms\" : [");
for (t = ci ; t ; t = t->next)
    printf("[%s, %u],\n", quote(t->chrom), t->size);
printf("],\n");
}

void printOneChrom(struct chromInfo *ci)
// print chromInfo for a single chrom
{
printf("\"chrom\" : %s,\n\"size\" : %u,\n", quote(ci ? ci->chrom : NULL), ci ? ci->size : 0);
}

static void printChromsOneColumn(struct chromInfo *ci, char *column)
// print a list of chromInfo data by column
{
struct chromInfo *t;
printf("\"%s\" : [", column);
for (t = ci ; t ; t = t->next)
    {
    if (sameOk("size", column))
        printf("%u,", t->size);
    else
        printf("%s,", sameOk("chrom", column) ? quote(t->chrom) : "" );
    }
printf("]\n");
}

void printChromsByColumn(struct chromInfo *ci)
// print a list of chromInfo data by column
{
  printChromsOneColumn(ci, "chrom");
  printChromsOneColumn(ci, "size");
}
