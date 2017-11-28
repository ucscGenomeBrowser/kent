/* tabQuery - Run sql-like query on a tab separated file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "dystring.h"
#include "fieldedTable.h"
#include "rql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tabQuery - Run sql-like query on a tab separated file.\n"
  "usage:\n"
  "   tabQuery rqlStatement\n"
  "where rqlStatement is much like a SQL statement, but with no joins and no commands\n"
  "other than select allowed.  The input file name is taken from the 'from' clause.\n"
  "examples\n"
  "    tabQuery select file,date from manifest.tsv\n"
  "This will output the file and date fields from the manifest.tsv file\n"
  "    tabQuery select file,date,lab from manifest.tsv where lab like 'myLab%%'\n"
  "This will output the selected three fields from the file where the lab starts with myLab\n"
  "    tabQuery select file,data from manifest.tsv where lab='myLab'\n"
  "This will output the selected two fields where the lab field is exactly myLab.\n"
  "    tabQuery select * from manifest.tsv where lab='myLab'\n"
  "This will output all fields where the lab field is exactly myLab.\n"
  "    tabQuery select a*,b* from manifest.tsv where lab='myLab'\n"
  "This will output all fields starting with a or b where the lab field is exactly myLab.\n"
  "    tabQuery select count(*) from manifest.tsv where type='fastq' and size < 1000\n"
  "This will count the number of records where type is fastq and size less than 1000\n"
  );
}

struct fieldedTable *gTable;
struct hash *gFieldHash;

char *lookup(void *record, char *key)
/* Lookup key in record */
{
struct fieldedRow *row = record;
int fieldIx = hashIntValDefault(gFieldHash, key, -1);
if (fieldIx < 0)
    errAbort("Field %s isn't found in %s", key, gTable->name);
return row->row[fieldIx];
}

void tabQuery(char *query)
/* tabQuery - Run sql-like query on a tab separated file.. */
{
/* Parse statement and make sure that it just references one table */
struct rqlStatement *rql = rqlStatementParseString(query);
verbose(2, "parsed %s ok\n", query);
int tableCount = slCount(rql->tableList);
if (tableCount != 1)
    errAbort("One and only one file allowed in the from clause\n");

boolean doCount = FALSE;
if (sameWord(rql->command, "count"))
    doCount = TRUE;
else if (sameWord(rql->command, "select"))
    doCount = FALSE;
else
    errAbort("Unrecognized rql command %s", rql->command);

/* Read in tab separated value file */
char *tabFile = rql->tableList->name;
gTable = fieldedTableFromTabFile(tabFile, tabFile, NULL, 0);

/* Make an integer valued hash of field indexes */
gFieldHash = hashNew(0);
int i;
for (i=0; i<gTable->fieldCount; ++i)
    hashAddInt(gFieldHash, gTable->fields[i], i);

/* Make sure all fields in query exist */
struct slName *field;
for (field = rql->fieldList; field != NULL; field = field->next)
    if (!hashLookup(gFieldHash, field->name))
	{
	if (!anyWild(field->name))
	    errAbort("field %s doesn't exist in %s", field->name, tabFile);
	}

/* Make list of fields as opposed to array  */
struct slName *allFieldList = NULL;
for (i=0; i<gTable->fieldCount; ++i)
    slNameAddHead(&allFieldList, gTable->fields[i]);
slReverse(&allFieldList);

/* Expand any field names with wildcards. */
rql->fieldList = wildExpandList(allFieldList, rql->fieldList, TRUE);


/* Print out label row. */
if (!doCount)
    {
    printf("#");
    char *sep = "";
    for (field = rql->fieldList; field != NULL; field = field->next)
	{
	printf("%s%s", sep, field->name);
	sep = "\t";
	}
    printf("\n");
    }

/* Print out or just count selected fields that match query */
int matchCount = 0;
struct lm *lm = lmInit(0);
struct fieldedRow *row;
int limit = rql->limit;
for (row = gTable->rowList; row != NULL; row = row->next)
    {
    boolean pass = TRUE;
    if (rql->whereClause != NULL)
        {
	struct rqlEval res = rqlEvalOnRecord(rql->whereClause, row, lookup, lm);
	res = rqlEvalCoerceToBoolean(res);
	pass = res.val.b;
	}
    if (pass)
        {
	++matchCount;
	if (!doCount)
	    {
	    if (limit > 0 && matchCount > limit)
	        break;
	    char *sep = "";
	    for (field = rql->fieldList; field != NULL; field = field->next)
		{
		int fieldIx = hashIntVal(gFieldHash, field->name);
		printf("%s%s", sep, row->row[fieldIx]);
		sep = "\t";
		}
	    printf("\n");
	    }
	}
    }

if (doCount)
    printf("%d\n", matchCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 2)
    usage();
struct dyString *query = dyStringNew(0);
int i;
for (i=1; i<argc; ++i)
    {
    if (i != 1)
        dyStringAppendC(query, ' ');
    dyStringAppend(query, argv[i]);
    }
tabQuery(query->string);
return 0;
}
