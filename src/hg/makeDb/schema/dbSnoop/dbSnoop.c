/* dbSnoop - Produce an overview of a database.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "jksql.h"
#include "obscure.h"
#include "tableStatus.h"

static char const rcsid[] = "$Id: dbSnoop.c,v 1.12 2008/09/11 17:34:23 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dbSnoop - Produce an overview of a database.\n"
  "usage:\n"
  "   dbSnoop database output\n"
  "options:\n"
  "   -unsplit - if set will merge together tables split by chromosome\n"
  "   -noNumberCommas - if set will leave out commas in big numbers\n"
  );
}

static struct optionSpec options[] = {
   {"unsplit", OPTION_BOOLEAN},
   {"noNumberCommas", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean noNumberCommas = FALSE;
boolean unsplit = FALSE;
struct slName *chromList;	/* List of chromosomes in unsplit case. */

struct fieldDescription
/* Information on a field. */
    {
    struct fieldDescription *next;	/* Next in list. */
    char *name;			/* Field name. */
    char *type;			/* SQL type. */
    char *key;			/* How is it indexed? PRI/MUL/etc. */
    };

struct tableInfo
/* Information on a table. */
    {
    struct tableInfo *next;  /* Next in list. */
    char *name;    	     /* Name of table. */
    struct tableStatus *status;	/* Result of table status call. */
    struct fieldDescription *fieldList;/* List of fields in table. */
    };

int tableInfoCmpSize(const void *va, const void *vb)
/* Compare two tableInfo. */
{
const struct tableInfo *a = *((struct tableInfo **)va);
const struct tableInfo *b = *((struct tableInfo **)vb);
long long aSize = a->status->dataLength + a->status->indexLength;
long long bSize = b->status->dataLength + b->status->indexLength;
long long diff = bSize - aSize;
if (diff < 0) return -1;
else if (diff > 0) return 1;
else return 0;
}

int tableInfoCmpName(const void *va, const void *vb)
/* Compare two tableInfo on name. */
{
const struct tableInfo *a = *((struct tableInfo **)va);
const struct tableInfo *b = *((struct tableInfo **)vb);
return strcmp(a->name, b->name);
}

int tableInfoCmpUpdateTime(const void *va, const void *vb)
/* Compare two tableInfo on update time. */
{
const struct tableInfo *a = *((struct tableInfo **)va);
const struct tableInfo *b = *((struct tableInfo **)vb);
char *aT = a->status->updateTime;
char *bT = b->status->updateTime;
if (aT == bT)
    return FALSE;
else if (aT == NULL)	/* Update_time is NULL for InnoDB tables */
    return -1;
else if (bT == NULL)
    return 1;
else
    return strcmp(bT,aT);
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


void tableSummary(struct tableStatus *status, 
	struct sqlConnection *conn, FILE *f, 
	struct hash *fieldHash, struct fieldInfo **pFiList,
	struct hash *tableHash, struct tableInfo **pTiList,
	struct hash *typeHash, struct tableType **pTtList)
/* Write out summary of table to file. */
{
char query[256];
struct sqlResult *sr;
char **row;
struct dyString *fields = dyStringNew(0);
struct tableInfo *ti;

/* Make new table info, and add it to hash, list */
AllocVar(ti);
hashAddSaveName(tableHash, status->name, ti, &ti->name);
ti->status = status;
slAddTail(pTiList, ti);

/* List fields. */
safef(query, sizeof(query), "describe %s", ti->name);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct fieldDescription *field;
    AllocVar(field);
    field->name = cloneString(row[0]);
    field->type = cloneString(row[1]);
    if (row[3][0] != 0)
	field->key = cloneString(row[3]);
    slAddHead(&ti->fieldList, field);
    dyStringPrintf(fields, "%s,", field->name);
    noteField(fieldHash, ti->name, field->name, pFiList);
    }

noteType(typeHash, fields->string, ti->name, pTtList);
sqlFreeResult(&sr);
} 

void printLongNumber(FILE *f, long long val)
/* Print long number possibly with commas. */
{
if (noNumberCommas)
    fprintf(f, "%lld", val);
else
    printLongWithCommas(f, val);
}

void printTaggedLong(FILE *f, char *tag, long long val)
/* Print out tagged value, putting commas in number */
{
fprintf(f, "%s:\t", tag);
printLongNumber(f, val);
fprintf(f, "\n");
}

char *findPastChrom(char *table, struct slName *chromList)
/* Return first character in table past the chrN_ prefix if any.
 * if no chrN_ prefix return NULL.  The logic here depends on
 * chromList being sorted with largest name first. */
{
struct slName *chrom;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    if (startsWith(chrom->name, table))
        {
	int len = strlen(chrom->name);
	if (table[len] == '_')
	    return table + len + 1;
	}
    }
return NULL;
}

void unsplitTables(struct sqlConnection *conn, struct tableInfo **pList)
/* Merge together information on tables that are split by chromosome. */
{
/* Note, this routine leaks a little memory, but in this context that's
 * that's ok. */
struct tableInfo *ti, *newList = NULL, *next;
struct hash *splitHash = hashNew(0);

chromList = sqlQuickList(conn,
	"select chrom from chromInfo order by length(chrom) desc");
if (chromList == NULL)
    errAbort("Can't use unsplit because database has no chromInfo table");

for (ti = *pList; ti != NULL; ti = next)
    {
    char *pastChrom = findPastChrom(ti->name, chromList);
    next = ti->next;
    if (pastChrom != NULL)
        {
	struct tableInfo *splitTi;
	if ((splitTi = hashFindVal(splitHash, pastChrom)) != NULL)
	    {
	    splitTi->status->rows += ti->status->rows;
	    splitTi->status->dataLength += ti->status->dataLength;
	    splitTi->status->indexLength += ti->status->indexLength;
	    }
	else
	    {
	    ti->name = pastChrom;
	    hashAdd(splitHash, pastChrom, ti);
	    slAddHead(&newList, ti);
	    }
	}
    else
        {
	slAddHead(&newList, ti);
	}
    }
slSort(&newList, tableInfoCmpName);
*pList = newList;
}

struct indexInfo 
/* Stores pertinent info from row returned by mysql in response to
 *   show indexes from table */
    {
    struct indexInfo *next;
    char *table;		/* Name of table. */
    boolean nonUnique;		/* True if can have duplicates. */
    char *name;			/* Name of this index. */
    int seqInIndex;		/* For multiple part indexes, which part. */
    char *field;		/* Name of field . */
    long long cardinality;		/* Cardinality of index. */
    };

struct indexInfo *indexInfoLoad(char **row)
/* Allocate and return index info based on row. */
{
struct indexInfo *ii;
AllocVar(ii);
ii->table = cloneString(row[0]);
ii->nonUnique = sqlUnsigned(row[1]);
ii->name = cloneString(row[2]);
ii->seqInIndex = sqlUnsigned(row[3]);
ii->field = cloneString(row[4]);
if (row[6] != NULL)
    ii->cardinality = sqlLongLong(row[6]);
return ii;
}

struct indexGroup
/* Info about a group of indexes, that is various parts of
 * a multi-column index. */
    {
    struct indexGroup *next;
    char *name;				/* Name of index. */
    struct indexInfo *fieldList;	/* Various fields involved in index. */
    };

void printIndexes(FILE *f, struct sqlConnection *conn, struct tableInfo *ti)
/* Print info about indexes on table to file. */
{
char query[256], **row;
struct sqlResult *sr;
struct indexGroup *groupList = NULL, *group;
struct hash *groupHash = newHash(8);
struct indexInfo *ii;
char *tableName = ti->name;
char splitTable[256];
boolean isSplit = FALSE;

if (unsplit)
    {
    struct slName *chrom;
    if (!sqlTableExists(conn, tableName))
	{
	for (chrom = chromList; chrom != NULL; chrom = chrom->next)
	    {
	    safef(splitTable, sizeof(splitTable), "%s_%s", chrom->name, tableName);
	    if (sqlTableExists(conn, splitTable))
		{
		tableName = splitTable;
		isSplit = TRUE;
		break;
		}
	    }
	}
    }

/* Build up information on indexes in this table by processing
 * mysql show indexes command results.  This command returns 
 * a separate row for each field in each index.  We'll group these. */
safef(query, sizeof(query), "show indexes from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ii = indexInfoLoad(row);
    group = hashFindVal(groupHash, ii->name);
    if (group == NULL)
        {
	AllocVar(group);
	hashAddSaveName(groupHash, ii->name, group, &group->name);
	slAddTail(&groupList, group);
	}
    slAddTail(&group->fieldList, ii);
    }
sqlFreeResult(&sr);

fprintf(f, "%s has %d rows and %d indexes\n", ti->name, ti->status->rows, 
	slCount(groupList)); 
for (group = groupList; group != NULL; group = group->next)
    {
    long long maxCardinality = 0, nonUnique = FALSE;
    fprintf(f, "\t");
    for (ii=group->fieldList; ii != NULL; ii = ii->next)
        {
	fprintf(f, "%s.%s", ti->name, ii->field);
	if (ii->next != NULL)
	    fprintf(f, ",");
	if (ii->cardinality > maxCardinality)
	    maxCardinality = ii->cardinality;
	nonUnique = ii->nonUnique;
	}
    fprintf(f, "\t%s", (nonUnique ? "MUL" : "PRI") );
    if (maxCardinality > 0 && !isSplit)
	fprintf(f, "\t%lld", maxCardinality);
    else
        fprintf(f, "\tn/a");
    fprintf(f, "\n");
    }
hashFree(&groupHash);
}

void dbSnoop(char *database, char *output)
/* dbSnoop - Produce an overview of a database.. */
{
FILE *f = mustOpen(output, "w");
struct sqlConnection *conn = sqlConnect(database);
struct sqlConnection *conn2 = sqlConnect(database);
int majorVersion = sqlMajorVersion(conn);
int minorVersion = sqlMinorVersion(conn);
struct sqlResult *sr = sqlGetResult(conn, "show table status");
char **row;
struct hash *fieldHash = hashNew(0);
struct hash *typeHash = hashNew(0);
struct hash *tableHash = hashNew(0);
struct fieldInfo *fiList = NULL, *fi;
struct tableInfo *tiList = NULL, *ti;
struct tableType *ttList = NULL, *tt;
long long totalData = 0, totalIndex = 0, totalRows = 0;
int totalFields = 0;

/* Collect info from database. */
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct tableStatus *status;
    if ((majorVersion > 4) || ((4 == majorVersion) && (minorVersion > 0)))
	memcpy(row+2, row+3, (TABLESTATUS_NUM_COLS-2)*sizeof(char*));
    status = tableStatusLoad(row);
    tableSummary(status, conn2, f, 
    	fieldHash, &fiList, tableHash, &tiList, typeHash, &ttList);
    }
sqlFreeResult(&sr);

if (unsplit)
    unsplitTables(conn, &tiList);

/* Print overall database summary. */
for (ti = tiList; ti != NULL; ti = ti->next)
    {
    totalFields += slCount(ti->fieldList);
    totalData += ti->status->dataLength;
    totalIndex += ti->status->indexLength;
    totalRows += ti->status->rows;
    }
fprintf(f, "DATABASE SUMMARY:\t%s\n", database);
printTaggedLong(f, "tables", slCount(tiList));
printTaggedLong(f, "fields", totalFields);
printTaggedLong(f, "bytes", totalIndex + totalData);
fprintf(f, "\t");
printTaggedLong(f, "data", totalData);
fprintf(f, "\t");
printTaggedLong(f, "index", totalIndex);
printTaggedLong(f, "rows", totalRows);
fprintf(f, "\n");

/* Print summary of table sizes ordered by size. */
slSort(&tiList, tableInfoCmpSize);
fprintf(f, "TABLE SIZE SUMMARY:\n");
fprintf(f, "#bytes\tname\trows\tdata\tindex\n");
for (ti = tiList; ti != NULL; ti = ti->next)
    {
    printLongNumber(f, ti->status->dataLength + ti->status->indexLength);
    fprintf(f, "\t");
    fprintf(f, "%s\t", ti->name);
    printLongNumber(f, ti->status->rows);
    fprintf(f, "\t");
    printLongNumber(f, ti->status->dataLength);
    fprintf(f, "\t");
    printLongNumber(f, ti->status->indexLength);
    fprintf(f, "\n");
    }
fprintf(f, "\n");

/* Print summary of table sizes ordered by size. */
slSort(&tiList, tableInfoCmpUpdateTime);
fprintf(f, "TABLE UPDATE SUMMARY:\n");
fprintf(f, "#table\tupdate time\tcreate time\tcheckTime\t\n");
for (ti = tiList; ti != NULL; ti = ti->next)
    {
    fprintf(f, "%s\t%s\t%s\t%s\n", ti->status->updateTime, ti->name,
    	ti->status->createTime, naForNull(ti->status->checkTime));
    }
fprintf(f, "\n");

/* Print summary of rows and fields in each table ordered alphabetically */
slSort(&tiList, tableInfoCmpName);
fprintf(f, "TABLE FIELDS SUMMARY:\n");
fprintf(f, "#name\tfields\n");
for (ti = tiList; ti != NULL; ti = ti->next)
    {
    struct fieldDescription *field;
    slReverse(&ti->fieldList);
    fprintf(f, "%s\t", ti->name);
    for (field = ti->fieldList; field != NULL; field = field->next)
	fprintf(f, "%s,", field->name);
    fprintf(f, "\n");
    }
fprintf(f, "\n");

/* Print summary of indexes. */
fprintf(f, "TABLE INDEX SUMMARY\n");
for (ti = tiList; ti != NULL; ti = ti->next)
    printIndexes(f, conn, ti);
fprintf(f, "\n");

/* Print summary of fields and tables fields are used in 
 * ordered by the number of tables a field is in. */
slSort(&fiList, fieldInfoCmp);
fprintf(f, "FIELD SUMMARY:\t%d\n", slCount(fiList));
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
fprintf(f, "TABLE TYPE SUMMARY (%d types):\n", slCount(ttList));
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
noNumberCommas = optionExists("noNumberCommas");
unsplit = optionExists("unsplit");
dbSnoop(argv[1], argv[2]);
return 0;
}
