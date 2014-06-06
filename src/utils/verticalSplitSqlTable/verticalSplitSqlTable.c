/* verticalSplitSqlTable - Split a database table into two new related tables that share a field. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "ra.h"
#include "portable.h"
#include "obscure.h"
#include "errAbort.h"
#include "asParse.h"

/* Command line options. */
boolean partialOk = FALSE;
boolean mergeOk = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "verticalSplitSqlTable - Split a database table into two new related tables that share a field.\n"
  "Note, this program just works on tab-separated files for the split tables, it does not actually\n"
  "use a database.\n"
  "usage:\n"
  "   verticalSplitSqlTable oldTab oldAs splitSpec outDir\n"
  "where:\n"
  "   oldTab is a tab-separated file for the unsplit table\n"
  "   oldAs is an autoSql file describing the unsplit table\n"
  "   splitSpec is a text file saying how to split the fields, see more below\n"
  "   outDir is a directory that will get populated with files for creating split tables\n"
  "The splitSpec is a .ra formatted file with tags: table1 table2 fields1 fields2 sharedKey\n"
  "Here's an example of this that splits a 'driver' tabel into a 'person' and 'car' table\n"
  "Where the original driver table has the fields name, phone, ssn, carLicense, carMake, carModel\n"
  "   table1 person\n"
  "   fields1 name=name phone=phone ssn=ssn car=carLicense\n"
  "   description1 A person's name, phone, and social security number\n"
  "   table2 car\n"
  "   fields2 license=carLicense make=carMake model=carModel\n"
  "   description2 Basic info on a car\n"
  "   sharedKey carLicense\n"
  "The result will be .as and .tab files for table1 and table2 in outDir\n"
  "Note that the new table1 will have as many fields as the old table, while the\n"
  "new one will just have one row for each unique sharedKey\n"
  "options:\n"
  "   -partialOk - if set, ok for not all fields in old table to be in output\n"
  "   -mergeOk - if set, won't be considered an error to have multiple rows\n"
  "              corresponding to one sharedKey in table2.  Just first row will\n"
  "              be output, rest will be in outDir/mergeErrs.txt\n"
  );
}

static struct optionSpec options[] = {
   {"partialOk", OPTION_BOOLEAN},
   {"mergeOk", OPTION_BOOLEAN},
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

void checkSharedKeyInList(char *sharedKey, char *splitSpec, char *fields, struct slPair *fieldList)
/* Make sure that sharedKey really is in fieldList. splitSpec and fields are just 
 * for error reporting*/
{
struct slPair *el;
for (el = fieldList; el != NULL; el = el->next)
    {
    char *field = el->val;
    if (sameString(field, sharedKey))
        return;  // We are good
    }
errAbort("Error: sharedKey %s not found after equals in '%s'", sharedKey, fields);
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

/* Print selected columns. */
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

int *makeNewToOldArray(struct asObject *as, struct slPair *fieldList)
/* Return an array where we can lookup old index given new index. */
{
int oldFieldCount = slCount(as->columnList);
int newFieldCount = slCount(fieldList);
int *oldIx;
AllocArray(oldIx, newFieldCount);
int i;
struct slPair *fieldPair;
for (i=0, fieldPair = fieldList; i<newFieldCount; ++i, fieldPair = fieldPair->next)
    {
    char *oldName = fieldPair->val;
    struct asColumn *col = asColumnFind(as, oldName);
    assert(col != NULL);  /* We checked earlier but... */
    int ix = slIxFromElement(as->columnList, col);
    assert(ix >= 0 && ix <= oldFieldCount);
    oldIx[i] = ix;
    }
return oldIx;
}

void outputPartialTab(char *inTab, struct asObject *as, struct slPair *fieldList, char *outName)
/* Output columns in fieldList from inTab (described by as) into outName */
{
/* Open input and output. */
struct lineFile *lf = lineFileOpen(inTab, TRUE);
FILE *f = mustOpen(outName, "w");

/* Set up array for input fields with more than we expect for better error reporting. */
int oldFieldCount = slCount(as->columnList);
int newFieldCount = slCount(fieldList);
int allocFields = oldFieldCount+10;
char *words[allocFields];

/* Set up array for output fields that says where to find them in input. */
int *oldIx = makeNewToOldArray(as, fieldList);

/* Go through each line of input, outputting selected columns. */
int fieldCount;
while ((fieldCount = lineFileChopNextTab(lf, words, allocFields)) > 0)
    {
    lineFileExpectWords(lf, oldFieldCount, fieldCount);
    fprintf(f, "%s", words[oldIx[0]]);
    int i;
    for (i=1; i<newFieldCount; ++i)
	fprintf(f, "\t%s", words[oldIx[i]]);
    fprintf(f, "\n");
    }

/* Clean up and go home. */
freez(&oldIx);
carefulClose(&f);
lineFileClose(&lf);
}

void outputUniqueOnSharedKey(char *inTab, struct asObject *as, struct asColumn *keyCol,
    struct slPair *fieldList, char *outTab, char *outErr)
/* Scan through tab-separated file inTab and output fields in fieldList to
 * outTab. Make sure there is only one row for each value of sharedKey field. 
 * If there would be multiple different rows in output with sharedKey, 
 * complain about it in outErr. */
{
/* Open input and output. */
struct lineFile *lf = lineFileOpen(inTab, TRUE);
FILE *f = mustOpen(outTab, "w");
FILE *fErr = mustOpen(outErr, "w");

/* Set up array for input fields with more than we expect for better error reporting. */
int oldFieldCount = slCount(as->columnList);
int newFieldCount = slCount(fieldList);
int allocFields = oldFieldCount+10;
char *words[allocFields];

/* Set up array for output fields that says where to find them in input. */
int *oldIx = makeNewToOldArray(as, fieldList);

/* Figure out index of key field. */
int keyIx = slIxFromElement(as->columnList, keyCol);

/* Go through each line of input, outputting selected columns. */
struct hash *uniqHash = hashNew(18); 
struct hash *errHash = hashNew(0);
struct dyString *dy = dyStringNew(1024);
int fieldCount;
while ((fieldCount = lineFileChopNextTab(lf, words, allocFields)) > 0)
    {
    lineFileExpectWords(lf, oldFieldCount, fieldCount);

    /* Collect possible output into dy. */
    dyStringClear(dy);
    dyStringPrintf(dy, "%s", words[oldIx[0]]);
    int i;
    for (i=1; i<newFieldCount; ++i)
	dyStringPrintf(dy,  "\t%s", words[oldIx[i]]);
    dyStringPrintf(dy, "\n");

    /* Check that this line is either unique for this key, or the same as previous lines
     * for the key. */
    char *key = words[keyIx];
    char *oldVal = hashFindVal(uniqHash, key);
    if (oldVal != NULL)
        {
	if (!sameString(oldVal, dy->string))
	    {
	    /* Error reporting is a little complex.  We want to output all lines associated
	     * with key, including the first one, but we only want to do first line once. */
	    if (!hashLookup(errHash, key))
	        {
		hashAdd(errHash, key, NULL);
		fputs(oldVal, fErr);
		}
	    fputs(dy->string, fErr);
	    }
	}
    else
	{
	hashAdd(uniqHash, key, cloneString(dy->string));
        fputs(dy->string, f);
	}
    }

/* Report error summary */
if (errHash->elCount > 0)
    {
    warn("Warning: %d shared keys have multiple values in table 2. See %s.\n"
         "Only first row for each key put in %s" , errHash->elCount, outErr, outTab);
    if (!mergeOk)
        noWarnAbort();
    }

/* Clean up and go home. */
freez(&oldIx);
carefulClose(&fErr);
carefulClose(&f);
lineFileClose(&lf);
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
char *sharedKey = mustFindInSplitSpec("sharedKey", ra, splitSpec);
if (ra->elCount > 7)
    errAbort("Extra fields in %s", splitSpec);

/* Convert this=that strings to lists of pairs. */
struct slPair *fieldList1 = slPairFromString(fields1);
struct slPair *fieldList2 = slPairFromString(fields2);

/* Do some more checks */
if (sameString(table1, table2))
    errAbort("Error: table1 and table2 are the same (%s) in %s", table1, splitSpec);
checkSharedKeyInList(sharedKey, splitSpec, fields1, fieldList1);
checkSharedKeyInList(sharedKey, splitSpec, fields2, fieldList2);
struct asColumn *keyCol = asColumnFind(as, sharedKey);
if (keyCol == NULL)
    errAbort("The sharedKey '%s' is not in %s", sharedKey, oldAs);

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

/* Output .as files. */
outputPartialAs(as, table1, fieldList1, description1, outDir);
outputPartialAs(as, table2, fieldList2, description2, outDir);

/* Output first split file - a straight up subset of columns. */
char path[PATH_LEN];
safef(path, sizeof(path), "%s/%s.tab", outDir, table1);
outputPartialTab(oldTab, as, fieldList1, path);


/* Output second split file */
char errPath[PATH_LEN];
safef(path, sizeof(path), "%s/%s.tab", outDir, table2);
safef(errPath, sizeof(path), "%s/mergeErrs.txt", outDir);
outputUniqueOnSharedKey(oldTab, as, keyCol, fieldList2, path, errPath);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
partialOk = optionExists("partialOk");
mergeOk = optionExists("mergeOk");
if (argc != 5)
    usage();
verticalSplitSqlTable(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
