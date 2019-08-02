/* tabToTabDir - Convert a large tab-separated table to a directory full of such tables according 
 * to a specification.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "portable.h"
#include "ra.h"
#include "fieldedTable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
"tabToTabDir - Convert a large tab-separated table to a directory full of such tables according\n"
"to a specification.\n"
"usage:\n"
"   tabToTabDir in.tsv spec.txt outDir\n"
"where:\n"
"   in.tsv is a tab-separated input file.  The first line is the label names and may start with #\n"
"   spec.txt is a file that says what columns to put into the output, described in more detail below\n"
"   outDir is a directory that will be populated with tab-separated files\n"
"spec.ra file format:\n"
"   This is in a ra format with one stanza per output table.\n"
"   Each stanza should look like:\n"
"        tableName    key-column\n"
"        columnName1	sourceField1\n"
"        columnName2	sourceField2\n"
"              ...\n"
"   if the sourceField is missing it is assumed it is named to be a column of the same name in in.tsv\n"
"   The sourceField can either be a column name in the in.tsv, or a string enclosed literal or something\n"
"   more complicated enclosed in an eval() call\n"
"options:\n"
"   -xxx=XXX\n"
);
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};


boolean allStringsSame(char **aa, char **bb, int count)
/* Return true if first count of strings between aa and bb are the same */
{
int i;
for (i=0; i<count; ++i)
    if (!sameString(aa[i], bb[i]))
        return FALSE;
return TRUE;
}

enum valType
/* A type */
    {
    vtVar, vtLink, vtConst, 
    };

struct fieldVal
/* An expression that can define what fits in a field */
    {
    struct fieldVal *next;	/* Might want to hang these on a list. */
    char *name;	
    enum valType type;			/* Constant, link, or variable */
    int oldIx;				/* For variable and link ones where field is in old table */
    char *val;			/* For constant ones the string value */
    };

struct fieldVal *parseFieldVal(char *name, char *input)
/* return a fieldVal based on the contents of input, which are not destroyed */
{
/* Make up return structure. */

struct fieldVal *fv;
AllocVar(fv);
fv->name = cloneString(name);

char *s = skipLeadingSpaces(input);
char c = s[0];
if (c == 0)
    {
    fv->type = vtVar;
    fv->val = cloneString(name);
    }
else
    {
    if (c == '"' || c == '\'')
	{
	char *val = fv->val = cloneString(s);
	if (!parseQuotedString(val, val, NULL))
	    errAbort("in %s", input);
	fv->type = vtConst;
	}
    else if (c == '=')
	{
	char *val = fv->val = cloneString(skipLeadingSpaces(s+1));
	trimSpaces(val);
	if (isEmpty(val))
	    errAbort("Empty line following equals");
	fv->type = vtLink;
	}
    else
        {
	char *val = fv->val = cloneString(s);
	trimSpaces(val);
	fv->type = vtVar;
	}
    }
return fv;
}

void selectUniqueIntoTable(struct fieldedTable *inTable, struct fieldVal *fieldList,
    int keyFieldIx, struct fieldedTable *outTable)
/* Populate out table with selected rows from newTable */
{
struct hash *uniqHash = hashNew(0);
struct fieldedRow *fr;
int outFieldCount = outTable->fieldCount;
char *outRow[outFieldCount];

if (slCount(fieldList) != outFieldCount)	// A little cheap defensive programming on inputs
    internalErr();

for (fr = inTable->rowList; fr != NULL; fr = fr->next)
    {
    /* Create new row from a scan through old table */
    char **inRow = fr->row;
    char *key = inRow[keyFieldIx];
    int i;
    struct fieldVal *fv;
    for (i=0, fv=fieldList; i<outFieldCount && fv != NULL; ++i, fv = fv->next)
	{
	if (fv->type == vtConst)
	    outRow[i] = fv->val;
	else
	    outRow[i] = inRow[fv->oldIx];
	}

    struct fieldedRow *uniqFr = hashFindVal(uniqHash, key);
    if (uniqFr == NULL)
        {
	uniqFr = fieldedTableAdd(outTable, outRow, outFieldCount, 0);
	hashAdd(uniqHash, key, uniqFr);
	}
    else    /* Do error checking for true uniqueness of key */
        {
	if (!allStringsSame(outRow, uniqFr->row, outFieldCount))
	    errAbort("Duplicate id %s but different data in key field %s of %s.",
		key, inTable->fields[keyFieldIx], outTable->name);
	}
    }
}

void tabToTabDir(char *inTabFile, char *specFile, char *outDir)
/* tabToTabDir - Convert a large tab-separated table to a directory full of such tables 
 * according to a specification.. */
{
struct fieldedTable *inTable = fieldedTableFromTabFile(inTabFile, inTabFile, NULL, 0);
verbose(1, "Read %d columns, %d rows from %s\n", inTable->fieldCount, inTable->rowCount,
    inTabFile);
struct lineFile *lf = lineFileOpen(specFile, TRUE);
makeDirsOnPath(outDir);

struct slPair *specStanza = NULL;
while ((specStanza = raNextStanzAsPairs(lf)) != NULL)
    {
    /* Parse out table name and key field name. */
    verbose(2, "Processing spec stanza of %d lines\n",  slCount(specStanza));
    struct slPair *table = specStanza;
    char *tableName = table->name;
    char *keyFieldName = trimSpaces(table->val);
    if (isEmpty(keyFieldName))
       errAbort("No key field for table %s.", tableName);

    /* Make sure that key field is actually in field list */
    struct slPair *fieldList = table->next;
    int keyFieldIx = fieldedTableMustFindFieldIx(inTable, keyFieldName);
    if (keyFieldIx < 0)
       errAbort("key field %s is not found in field list for %s\n", tableName, keyFieldName);


    /* Create empty output table and track which fields of input go to output. */
    int fieldCount = slCount(fieldList);
    char *fieldNames[fieldCount];
    int i;
    struct slPair *field;
    struct fieldVal *fvList = NULL;
    for (i=0, field=fieldList; i<fieldCount; ++i, field=field->next)
        {
	char *newName = field->name;
	struct fieldVal *fv = parseFieldVal(newName, field->val);
	if (fv->type == vtVar)
	    fv->oldIx = fieldedTableMustFindFieldIx(inTable, fv->val);
	else if (fv->type == vtLink)
	    errAbort("Can't handle links yet for %s", fv->val);
	fieldNames[i] = newName;
	slAddHead(&fvList, fv);
	}
    slReverse(&fvList);
    struct fieldedTable *outTable = fieldedTableNew(tableName, fieldNames, fieldCount);
    outTable->startsSharp = inTable->startsSharp;

    /* Populate table */
    selectUniqueIntoTable(inTable, fvList, keyFieldIx, outTable);

    /* Create output file name and save file. */
    char outTabName[FILENAME_LEN];
    safef(outTabName, sizeof(outTabName), "%s/%s.tsv", outDir, tableName);
    verbose(1, "Writing %s of %d fields %d rows\n",  outTabName, outTable->fieldCount, outTable->rowCount);
    fieldedTableToTabFile(outTable, outTabName);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
tabToTabDir(argv[1], argv[2], argv[3]);
return 0;
}
