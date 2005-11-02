/* dbSnoop - Produce an overview of a database.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "jksql.h"
#include "obscure.h"

static char const rcsid[] = "$Id: dbSnoop.c,v 1.2 2005/11/02 05:23:10 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dbSnoop - Produce an overview of a database.\n"
  "usage:\n"
  "   dbSnoop database output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct tableInfo
/* Information on a table. */
    {
    struct tableInfo *next;  /* Next in list. */
    char *name;    	     /* Name of table. */
    struct slName *fieldList;/* List of fields in table. */
    int rowCount;	     /* Number of rows. */
    };

int tableInfoCmp(const void *va, const void *vb)
/* Compare two tableInfo. */
{
const struct tableInfo *a = *((struct tableInfo **)va);
const struct tableInfo *b = *((struct tableInfo **)vb);
return b->rowCount - a->rowCount;
}

struct fieldInfo
/* Information on a field. */
    {
    struct fieldInfo *next;	/* Next in list. */
    char *name;			/* Name of field. */
    struct slName *tableList;	/* List of tables using name. */
    int tableCount;		/* Count of tables. */
    };

int fieldInfoCmp(const void *va, const void *vb)
/* Compare two fieldInfo. */
{
const struct fieldInfo *a = *((struct fieldInfo **)va);
const struct fieldInfo *b = *((struct fieldInfo **)vb);
return b->tableCount - a->tableCount;
}

void noteField(struct hash *hash, char *table, char *field,
	struct fieldInfo **pList)
/* Keep track of field. */
{
struct fieldInfo *fi = hashFindVal(hash, field);
if (fi == NULL)
    {
    AllocVar(fi);
    hashAddSaveName(hash, field, fi, &fi->name);
    slAddHead(pList, fi);
    }
slNameAddHead(&fi->tableList, table);
fi->tableCount += 1;
}

struct tableType
/* Information on a type. */
    {
    struct tableType *next;	/* Next in list. */
    char *fields;		/* Comma-separated list of fields. */
    struct slName *tableList;	/* List of tables using this type. */
    int tableCount;		/* Count of tables. */
    };

int tableTypeCmp(const void *va, const void *vb)
/* Compare two tableType. */
{
const struct tableType *a = *((struct tableType **)va);
const struct tableType *b = *((struct tableType **)vb);
return b->tableCount - a->tableCount;
}

void noteType(struct hash *hash, char *fields, char *table,
	struct tableType **pList)
/* Keep track of tables of given type. */
{
struct tableType *tt = hashFindVal(hash, fields);
if (tt == NULL)
    {
    AllocVar(tt);
    hashAddSaveName(hash, fields, tt, &tt->fields);
    slAddHead(pList, tt);
    }
slNameAddHead(&tt->tableList, table);
tt->tableCount += 1;
}


void tableSummary(char *table, struct sqlConnection *conn, FILE *f, 
	struct hash *fieldHash, struct fieldInfo **pFiList,
	struct hash *tableHash, struct tableInfo **pTiList,
	struct hash *typeHash, struct tableType **pTtList)
/* Write out summary of table to file. */
{
char query[256];
struct sqlResult *sr;
int rowCount;
char **row;
struct dyString *fields = dyStringNew(0);
struct tableInfo *ti;

/* Make new table info, and add it to hash, list */
AllocVar(ti);
hashAddSaveName(tableHash, table, ti, &ti->name);
slAddTail(pTiList, ti);

/* Count rows. */
safef(query, sizeof(query), "select count(*) from %s", table);
ti->rowCount = sqlQuickNum(conn, query);

/* List fields. */
safef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *field = row[0];
    slNameAddHead(&ti->fieldList, field);
    dyStringPrintf(fields, "%s,", field);
    noteField(fieldHash, table, field, pFiList);
    }

noteType(typeHash, fields->string, table, pTtList);
sqlFreeResult(&sr);
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void dbSnoop(char *database, char *output)
/* dbSnoop - Produce an overview of a database.. */
{
FILE *f = mustOpen(output, "w");
struct sqlConnection *conn = sqlConnect(database);
struct sqlConnection *conn2 = sqlConnect(database);
struct sqlResult *sr = sqlGetResult(conn, "show tables");
char **row;
struct hash *fieldHash = hashNew(0);
struct hash *typeHash = hashNew(0);
struct hash *tableHash = hashNew(0);
struct fieldInfo *fiList = NULL, *fi;
struct tableInfo *tiList = NULL, *ti;
struct tableType *ttList = NULL, *tt;

/* Collect info from database. */
while ((row = sqlNextRow(sr)) != NULL)
    tableSummary(row[0], conn2, f, 
    	fieldHash, &fiList, tableHash, &tiList, typeHash, &ttList);

/* Print summary of rows and fields in each table ordered by
 * number of rows. */
slSort(&tiList, tableInfoCmp);
fprintf(f, "TABLE SUMMARY (%d):\n", slCount(tiList));
fprintf(f, "#rows\tname\tfields\n");
for (ti = tiList; ti != NULL; ti = ti->next)
    {
    struct slName *t;
    slReverse(&ti->fieldList);
    printLongWithCommas(f, ti->rowCount);
    fprintf(f, "\t%s\t", ti->name);
    for (t = ti->fieldList; t != NULL; t = t->next)
	fprintf(f, "%s,", t->name);
    fprintf(f, "\n");
    }
fprintf(f, "\n");

/* Print summary of fields and tables fields are used in 
 * ordered by the number of tables a field is in. */
slSort(&fiList, fieldInfoCmp);
fprintf(f, "FIELD SUMMARY (%d):\n", slCount(fiList));
fprintf(f, "#uses\tname\ttables\n");
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    struct slName *t;
    slReverse(&fi->tableList);
    fprintf(f, "%d\t%s\t", fi->tableCount, fi->name);
    for (t = fi->tableList; t != NULL; t = t->next)
	fprintf(f, "%s,", t->name);
    fprintf(f, "\n");
    }
fprintf(f, "\n");

/* Print summary of types and tables that use types 
 * ordered by the number of tables a type is used by. */
slSort(&ttList, tableTypeCmp);
fprintf(f, "TABLE TYPE SUMMARY (%d):\n", slCount(ttList));
fprintf(f, "#uses\ttables\tfields\n");
for (tt = ttList; tt != NULL; tt = tt->next)
    {
    struct slName *t;
    slReverse(&tt->tableList);
    fprintf(f, "%d\t", tt->tableCount);
    for (t = tt->tableList; t != NULL; t = t->next)
	fprintf(f, "%s,", t->name);
    fprintf(f, "\t%s\n", tt->fields);
    }

carefulClose(&f);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
dbSnoop(argv[1], argv[2]);
return 0;
}
