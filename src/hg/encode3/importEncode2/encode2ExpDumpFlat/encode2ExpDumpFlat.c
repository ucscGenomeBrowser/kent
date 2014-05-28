/* encode2ExpDumpFlat - Dump the experiment table in a semi-flat way from a relationalized Encode2 metadatabase.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "dystring.h"

char *database = "encode2Meta";
char *tablePrefix = "";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encode2ExpDumpFlat - Dump the experiment table in a semi-flat way from a relationalized Encode2 metadatabase.\n"
  "usage:\n"
  "   encode2ExpDumpFlat output.tab view.sql flatTable.sql\n"
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
"antibody",
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
    char **termsForIds;  /* Given ID, this is corresponding term. */
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
sqlSafef(query, sizeof(query), "select max(id) from %s", sti->tableName);
sti->maxId = sqlQuickNum(conn, query);
verbose(2, "%s maxId %d\n", sti->tableName, sti->maxId);
AllocArray(sti->termsForIds, sti->maxId+1);
sqlSafef(query, sizeof(query), "select id,term from %s", sti->tableName);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    unsigned id = sqlUnsigned(row[0]);
    char *term = row[1];
    sti->termsForIds[id] = cloneString(term);
    }
sqlFreeResult(&sr);
return sti;
}

void makeViewSql(char *outFile)
/* Write out monster view creation statement to outfile. */
{
FILE *f = mustOpen(outFile, "w");
fprintf(f, "create view experimentTerms as\n");
fprintf(f, "select ");
int i;
for (i=0; i<ArraySize(flatFields); ++i)
    {
    char *field = flatFields[i];
    fprintf(f, "  %s.%s as %s,\n", "experiment", field, field);
    }
for (i=0; i<ArraySize(starFields); ++i)
    {
    char *field = starFields[i];
    fprintf(f, "  %s.term as %s", field, field);
    if (i != ArraySize(starFields)-1)
        fprintf(f, ",");
    fprintf(f, "\n");
    }
fprintf(f, "from experiment");
for (i=0; i<ArraySize(starFields); ++i)
    {
    char *field = starFields[i];
    fprintf(f, ",\n  %s",  field);
    }
fprintf(f, "\n");
fprintf(f, "where ");
for (i=0; i<ArraySize(starFields); ++i)
    {
    char *field = starFields[i];
    fprintf(f, "  %s.%s = %s.id", "experiment", field, field);
    if (i != ArraySize(starFields)-1)
        fprintf(f, " and ");
    fprintf(f, "\n");
    }

carefulClose(&f);
}

void makeFlatTableSql(char *fileName)
/* Write out SQL creation statement for flat table. */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "create table expFlat (\n");
fprintf(f, "    id integer NOT NULL PRIMARY KEY,\n");
fprintf(f, "    updateTime varchar(40) NOT NULL,\n");
fprintf(f, "    series varchar(50) NOT NULL,\n");
fprintf(f, "    accession varchar(16) NOT NULL,\n");
fprintf(f, "    version integer NOT NULL,\n");
int i;
for (i=0; i<ArraySize(starFields); ++i)
    {
    char *field = starFields[i];
    fprintf(f, "    %s varchar(32) NOT NULL", field);
    if (i != ArraySize(starFields)-1)
        fprintf(f, ",");
    fprintf(f, "\n");
    }
fprintf(f, ");\n");
carefulClose(&f);
}

void encode2ExpDumpFlat(char *outFile, char *viewSql, char *flatTable)
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
sqlDyStringPrintf(query, "select ");
for (i=0; i<ArraySize(flatFields); ++i)
    {
    if (i != 0)
       dyStringAppendC(query, ',');
    sqlDyStringPrintf(query, "%s", flatFields[i]);
    }
for (i=0; i<ArraySize(starFields); ++i)
    {
    dyStringAppendC(query, ',');
    sqlDyStringPrintf(query, "%s", starFields[i]);
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
	    fputs(stiArray[i]->termsForIds[id], f);
	}
    fputc('\n', f);
    }


sqlFreeResult(&sr);
sqlDisconnect(&conn);
carefulClose(&f);

makeViewSql(viewSql);
makeFlatTableSql(flatTable);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
encode2ExpDumpFlat(argv[1], argv[2], argv[3]);
return 0;
}
