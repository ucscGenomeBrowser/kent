/* encode2ExpDumpFlat - Dump the experiment table in a semi-flat way from a relationalized Encode2 metadatabase.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "dystring.h"

char *database = "kentDjangoTest2";
char *tablePrefix = "cvDb_";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encode2ExpDumpFlat - Dump the experiment table in a semi-flat way from a relationalized Encode2 metadatabase.\n"
  "usage:\n"
  "   encode2ExpDumpFlat output.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

char *flatFields[] = {
"id",
"updateTime",
"series",
"accession",
"version",
};

char *starFields[] = {
"organism",
"lab",
"dataType",
"cellType",
"ab",
"age",
"attic",
"category",
"control",
"fragSize",
"grantee",
"insertLength",
"localization",
"mapAlgorithm",
"objStatus",
"phase",
"platform",
"promoter",
"protocol",
"readType",
"region",
"restrictionEnzyme",
"rnaExtract",
"seqPlatform",
"sex",
"strain",
"tissueSourceType",
"treatment",
};

struct starTableInfo
/* Information about one of our "star" tables. */
    {
    char *tableName;	/* Includes prefix. */
    char *fieldName;	/* No prefix. */
    int maxId;		/* Maximum of ID field. */
    char **tagsForIds;  /* Given ID, this is corresponding tag. */
    };

struct starTableInfo *starTableInfoNew(struct sqlConnection *conn, char *name)
/* Create new starTable,  reading information about it from database. */
{
struct starTableInfo *sti;
AllocVar(sti);
char buf[256];
safef(buf, sizeof(buf), "%s%s", tablePrefix, name);
sti->tableName = cloneString(buf);
sti->fieldName = cloneString(name);
char query[256];
safef(query, sizeof(query), "select max(id) from %s", sti->tableName);
sti->maxId = sqlQuickNum(conn, query);
uglyf("%s maxId %d\n", sti->tableName, sti->maxId);
AllocArray(sti->tagsForIds, sti->maxId+1);
safef(query, sizeof(query), "select id,tag from %s", sti->tableName);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    unsigned id = sqlUnsigned(row[0]);
    char *tag = row[1];
    sti->tagsForIds[id] = cloneString(tag);
    }
sqlFreeResult(&sr);
return sti;
}

void encode2ExpDumpFlat(char *outFile)
/* encode2ExpDumpFlat - Dump the experiment table in a semi-flat way from a relationalized Encode2 metadatabase.. */
{
FILE *f = mustOpen(outFile, "w");
struct sqlConnection *conn = sqlConnect(database);
struct starTableInfo **stiArray;
AllocArray(stiArray, ArraySize(starFields));
int i;
for (i=0; i<ArraySize(starFields); ++i)
    stiArray[i] = starTableInfoNew(conn, starFields[i]);

struct dyString *query = dyStringNew(2000);
dyStringPrintf(query, "select ");
for (i=0; i<ArraySize(flatFields); ++i)
    {
    if (i != 0)
       dyStringAppendC(query, ',');
    dyStringAppend(query, flatFields[i]);
    }
for (i=0; i<ArraySize(starFields); ++i)
    {
    dyStringAppendC(query, ',');
    dyStringAppend(query, starFields[i]);
    }
dyStringPrintf(query, " from %s%s", tablePrefix, "experiment");

struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    int flatSize = ArraySize(flatFields);
    int starSize = ArraySize(starFields);
    int i;
    for (i=0; i<flatSize; ++i)
        {
	if (i != 0)
	    fputc('\t', f);
	fputs(row[i], f);
	}
    for (i=0; i<starSize; ++i)
        {
	int id = sqlUnsigned(row[i+flatSize]);
	fputc('\t', f);
	if (id == 0)
	    fputs("n/a", f);
	else
	    fputs(stiArray[i]->tagsForIds[id], f);
	}
    fputc('\n', f);
    }


sqlFreeResult(&sr);

sqlDisconnect(&conn);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
encode2ExpDumpFlat(argv[1]);
return 0;
}
