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
"The spec.txt file contains one blank line separated stanza per output table.\n"
"Each stanza should look like:\n"
"        tableName    key-column\n"
"        columnName1	sourceField1\n"
"        columnName2	sourceField2\n"
"              ...\n"
"if the sourceField is missing it is assumed to be a column of the same name in in.tsv\n"
"The sourceField can either be a column name in the in.tsv, or a string enclosed literal\n"
"or an @ followed by a table name, in which case it refers to the key of that table\n"
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

enum fieldValType
/* A type */
    {
    fvVar, fvLink, fvConst, 
    };

struct newFieldInfo
/* An expression that can define what fits in a field */
    {
    struct newFieldInfo *next;	/* Might want to hang these on a list. */
    char *name;			/* Name of field in new table */
    enum fieldValType type;	/* Constant, link, or variable */
    int oldIx;			/* For variable and link ones where field is in old table */
    char *val;			/* For constant ones the string value */
    };

struct newFieldInfo *findField(struct newFieldInfo *list, char *name)
/* Find named element in list, or NULL if not found. */
{
struct newFieldInfo *el;
for (el = list; el != NULL; el = el->next)
    if (sameString(name, el->name))
        return el;
return NULL;
}

struct newTableInfo
/* Info on a new table we are making */
    {
    struct newTableInfo *next;	/* Next in list */
    char *name;			/* Name of table */
    char *keyField;		/* Key field within table */
    int keyFieldIx;		/* Index of key field */
    struct newFieldInfo *fieldList; /* List of fields */
    struct fieldedTable *table;	    /* Table to fill in. */
    };

struct newTableInfo *findTable(struct newTableInfo *list, char *name)
/* Find named element in list, or NULL if not found. */
{
struct newTableInfo *el;
for (el = list; el != NULL; el = el->next)
    if (sameString(name, el->name))
        return el;
return NULL;
}

struct newFieldInfo *parseFieldVal(char *name, char *input)
/* return a newFieldInfo based on the contents of input, which are not destroyed */
{
/* Make up return structure. */

struct newFieldInfo *fv;
AllocVar(fv);
fv->name = cloneString(name);

char *s = skipLeadingSpaces(input);
char c = s[0];
if (c == 0)
    {
    fv->type = fvVar;
    fv->val = cloneString(name);
    }
else
    {
    if (c == '"' || c == '\'')
	{
	char *val = fv->val = cloneString(s);
	if (!parseQuotedString(val, val, NULL))
	    errAbort("in %s", input);
	fv->type = fvConst;
	}
    else if (c == '@')
	{
	char *val = fv->val = cloneString(skipLeadingSpaces(s+1));
	trimSpaces(val);
	if (isEmpty(val))
	    errAbort("Nothing following %c", c);
	fv->type = fvLink;
	}
    else
        {
	char *val = fv->val = cloneString(s);
	trimSpaces(val);
	fv->type = fvVar;
	}
    }
return fv;
}

void selectUniqueIntoTable(struct fieldedTable *inTable, struct newFieldInfo *fieldList,
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
    struct newFieldInfo *fv;
    for (i=0, fv=fieldList; i<outFieldCount && fv != NULL; ++i, fv = fv->next)
	{
	if (fv->type == fvConst)
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

/* Read in file as ra file stanzas that we convert into tableInfos. */
struct newTableInfo *newTableList = NULL, *newTable;
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

    /* Have dealt with first line of stanza, which is about table,  rest of lines are fields */
    struct slPair *fieldList = specStanza->next;
    int fieldCount = slCount(fieldList);

    /* Create empty output table and track which fields of input go to output. */
    char *fieldNames[fieldCount];
    int i;
    struct slPair *field;
    struct newFieldInfo *fvList = NULL;
    for (i=0, field=fieldList; i<fieldCount; ++i, field=field->next)
        {
	char *newName = field->name;
	struct newFieldInfo *fv = parseFieldVal(newName, field->val);
	if (fv->type == fvVar)
	    fv->oldIx = fieldedTableMustFindFieldIx(inTable, fv->val);
	fieldNames[i] = newName;
	slAddHead(&fvList, fv);
	}
    slReverse(&fvList);
    struct fieldedTable *outTable = fieldedTableNew(tableName, fieldNames, fieldCount);
    outTable->startsSharp = inTable->startsSharp;

    /* Make sure that key field is actually in field list */
    struct newFieldInfo *keyField = findField(fvList, keyFieldName);
    if (keyField == NULL)
       errAbort("key field %s is not found in field list for %s\n", tableName, keyFieldName);

    /* Allocate structure to save results of this pass in and so so. */
    AllocVar(newTable);
    newTable->name = tableName;
    newTable->keyField = keyFieldName;
    newTable->fieldList = fvList;
    newTable->table = outTable;
    newTable->keyFieldIx = keyField->oldIx;
    slAddHead(&newTableList, newTable);
    }
slReverse(&newTableList);

/* Do links between tables */
for (newTable = newTableList; newTable != NULL; newTable = newTable->next)
    {
    struct newFieldInfo *field;
    for (field = newTable->fieldList; field != NULL; field = field->next)
      {
      if (field->type == fvLink)
          {
	  struct newTableInfo *linkedTable = findTable(newTableList, field->val);
	  if (linkedTable == NULL)
	     errAbort("@%s doesn't exist", field->name);
	  field->oldIx = linkedTable->keyFieldIx;
	  field->type = fvVar;
	  }
      }
    }

/* Output tables */
for (newTable = newTableList; newTable != NULL; newTable = newTable->next)
    {
    /* Populate table */
    struct fieldedTable *outTable = newTable->table;
    selectUniqueIntoTable(inTable, newTable->fieldList, newTable->keyFieldIx, outTable);

    /* Create output file name and save file. */
    char outTabName[FILENAME_LEN];
    safef(outTabName, sizeof(outTabName), "%s/%s.tsv", outDir, newTable->name);
    verbose(1, "Writing %s of %d fields %d rows\n",  
	outTabName, outTable->fieldCount, outTable->rowCount);
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
