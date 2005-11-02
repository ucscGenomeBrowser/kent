/* dbSnoop - Produce an overview of a database.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "obscure.h"

static char const rcsid[] = "$Id: dbSnoop.c,v 1.1 2005/11/02 03:44:13 kent Exp $";

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

void tableSummary(char *table, struct sqlConnection *conn, FILE *f, 
	struct hash *fieldHash, struct fieldInfo **pList)
/* Write out summary of table to file. */
{
char query[256];
struct sqlResult *sr;
int rowCount;
char **row;

safef(query, sizeof(query), "select count(*) from %s", table);
rowCount = sqlQuickNum(conn, query);
printLongWithCommas(f, rowCount);
fprintf(f, "\t%s\t", table);
safef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *field = row[0];
    fprintf(f, "%s,", field);
    noteField(fieldHash, table, field, pList);
    }
sqlFreeResult(&sr);
fprintf(f, "\n");
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
struct fieldInfo *fiList = NULL, *fi;

fprintf(f, "TABLE SUMMARY:\n");
fprintf(f, "#rows\tname\tfields\n");
while ((row = sqlNextRow(sr)) != NULL)
    tableSummary(row[0], conn2, f, fieldHash, &fiList);
fprintf(f, "\n");

fprintf(f, "FIELD SUMMARY:\n");
slSort(&fiList, fieldInfoCmp);
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    struct slName *t;
    slReverse(&fi->tableList);
    fprintf(f, "%d\t%s\t", slCount(fi->tableList), fi->name);
    for (t = fi->tableList; t != NULL; t = t->next)
	fprintf(f, "%s,", t->name);
    fprintf(f, "\n");
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
