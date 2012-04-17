/* verticalSplitSqlTable - Split a database table into two new related tables that share a field. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "ra.h"
#include "portable.h"
#include "obscure.h"
#include "asParse.h"

/* Command line options. */
boolean partialOk = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "verticalSplitSqlTable - Split a database table into two new related tables that share a field.\n"
  "Note, this program just creates tab-separated files for the split tables, it does not actually\n"
  "alter the database.\n"
  "usage:\n"
  "   verticalSplitSqlTable oldTab oldAs splitSpec outDir\n"
  "where:\n"
  "   oldTab is a tab-separated file for the unsplit table\n"
  "   oldAs is an autoSql file describing the unsplit table\n"
  "   splitSpec is a text file saying how to split the fields, see more below\n"
  "   outDir is a directory that will get populated with files for creating split tables\n"
  "The splitSpec is a .ra formatted file with fields: table1 table2 fields1 fields2 sharedKey\n"
  "Here's an example of this that splits a 'driver' tabel into a 'person' and 'car' table\n"
  "Where the original driver table has the fields name, phone, ssn, carMake, carModel\n"
  "   table1 person\n"
  "   fields1 name=name phone=phone ssn=ssn\n"
  "   description1 A person's name, phone, and social security number\n"
  "   table2 car\n"
  "   fields2 ownerSsn=ssn make=carMake model=carModel\n"
  "   description2 Information on a car including its owner\n"
  "   sharedKey ssn\n"
  "The result will be .as and .tab files for table1 and table2 in outDir\n"
  "options:\n"
  "   -partialOk - if set, ok for not all fields in old table to be in output\n"
  );
}

static struct optionSpec options[] = {
   {"partialOk", OPTION_BOOLEAN},
   {NULL, 0},
};

char *mustFindInSplitSpec(char *key, struct hash *hash, char *fileName)
/* Look up key in hash and return value.  If not found complain in a message that
 * includes file name. */
{
char *val = hashFindVal(hash, key);
if (val == NULL)
    errAbort("Required field '%s' missing from %s\n", key, fileName);
return val;
}

void checkFieldsInAs(struct slPair *list, char *listFile, struct asObject *as, char *asFile)
/* Make sure that all fields in list values are actually in the as.  File names are just
 * for error reporting. */
{
struct slPair *el;
for (el = list; el != NULL; el = el->next)
    {
    char *field = el->val;
    if (asColumnFind(as, field) == NULL)
        errAbort("Field %s is in %s but not %s\n", field, listFile, asFile);
    }
}

void outputPartialAs(struct asObject *as, char *newTable, struct slPair *newFieldList,
    char *newDescription, char *outDir)
/* Create outdir/newTable.as based on a subset of as. */
{
/* Create output file. */
char outPath[PATH_LEN];
safef(outPath, sizeof(outPath), "%s/%s.as", outDir, newTable);
FILE *f = mustOpen(outPath, "w");

/* Print table header. */
fprintf(f, "table %s\n", newTable);
fprintf(f, "\"%s\"\n", newDescription);
char *indent = "    ";
fprintf(f, "%s(\n", indent);

/* Print selecte columns. */
struct slPair *fieldPair;
for (fieldPair = newFieldList; fieldPair != NULL; fieldPair = fieldPair->next)
    {
    char *newField = fieldPair->name;
    char *oldField = fieldPair->val;
    struct asColumn *col = asColumnFind(as, oldField);
    fprintf(f, "%s%s %s; \"%s\"\n", indent, col->lowType->name, newField, col->comment);
    }

/* Close out table and file. */
fprintf(f, "%s)\n", indent);
carefulClose(&f);
}

void verticalSplitSqlTable(char *oldTab, char *oldAs, char *splitSpec, char *outDir)
/* verticalSplitSqlTable - Split a database table into two new related tables that share a field. */
{
struct asObject *as = asParseFile(oldAs);
if (as->next != NULL)
    errAbort("%d records in %s, only 1 allowed\n", slCount(as), oldAs);
uglyf("Read %s from %s\n", as->name, oldAs);

/* Read fields from splitSpec, and make sure there are no extra. */
struct hash *ra = raReadSingle(splitSpec);
char *table1 = mustFindInSplitSpec("table1", ra, splitSpec);
char *fields1 = mustFindInSplitSpec("fields1", ra, splitSpec);
char *description1 = mustFindInSplitSpec("description1", ra, splitSpec);
char *table2 = mustFindInSplitSpec("table2", ra, splitSpec);
char *fields2 = mustFindInSplitSpec("fields2", ra, splitSpec);
char *description2 = mustFindInSplitSpec("description2", ra, splitSpec);
char *sharedKey = mustFindInSplitSpec("table1", ra, splitSpec);
if (ra->elCount > 7)
    errAbort("Extra fields in %s", splitSpec);

if (sameString(table1, table2))
    errAbort("Error: table1 and table2 are the same (%s) in %s", table1, splitSpec);

/* Convert fieEld this=that strings to lists of pairs. */
struct slPair *fieldList1 = slPairFromString(fields1);
struct slPair *fieldList2 = slPairFromString(fields2);

uglyf("%s - %s\n", table1, description1);
uglyf(" %s has %d fields\n", table1, slCount(fieldList1));
uglyf("%s - %s\n", table2, description2);
uglyf(" %s has %d fields\n", table2, slCount(fieldList2));
uglyf("sharedKey %s\n", sharedKey);

/* Make sure that all fields in splitSpec are actually in the oldAs file. */
checkFieldsInAs(fieldList1, splitSpec, as, oldAs);
checkFieldsInAs(fieldList2, splitSpec, as, oldAs);

/* Make sure that all old table fields are covered */
if (!partialOk)
    {
    struct hash *covered = hashNew(0);
    struct slPair *field;
    for (field = fieldList1; field != NULL; field = field->next)
        hashAdd(covered, field->val, NULL);
    for (field = fieldList2; field != NULL; field = field->next)
        hashAdd(covered, field->val, NULL);
    struct asColumn *col;
    for (col = as->columnList; col != NULL; col = col->next)
        {
	if (!hashLookup(covered, col->name))
	    errAbort("Field %s in %s not output, use -partialOk flag if this is intentional",
		col->name, oldAs);
	}
    }

/* Ok, input is checked, start on output.. */
if (lastChar(outDir) == '/')
    trimLastChar(outDir);
makeDirsOnPath(outDir);

outputPartialAs(as, table1, fieldList1, description1, outDir);
outputPartialAs(as, table2, fieldList2, description2, outDir);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
partialOk = optionExists("partialOk");
if (argc != 5)
    usage();
verticalSplitSqlTable(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
