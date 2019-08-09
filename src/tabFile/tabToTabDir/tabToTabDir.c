/* tabToTabDir - Convert a large tab-separated table to a directory full of such tables according 
 * to a specification.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "sqlNum.h"
#include "portable.h"
#include "ra.h"
#include "csv.h"
#include "fieldedTable.h"
#include "strex.h"

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
"or an @ followed by a table name, in which case it refers to the key of that table.\n"
"If the source column is in comma-separated-values format then the sourceField can include a\n"
"constant array index to pick out an item from the csv list.\n"
"You can also use strex expressions for more complicated situations.  See src/lib/strex.doc\n"
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
    fvVar, fvLink, fvExp,
    };

struct newFieldInfo
/* An expression that can define what fits in a field */
    {
    struct newFieldInfo *next;	/* Might want to hang these on a list. */
    char *name;			/* Name of field in new table */
    enum fieldValType type;	/* Constant, link, or variable */
    int oldIx;			/* For variable and link ones where field is in old table */
    int newIx;			/* Where field is in new table. */
    char *val;			/* For constant ones the string value */
    int arrayIx;		/* If it's an array then the value */
    struct newFieldInfo *link;	/* If it's fvLink then pointer to the linked field */
    struct strexParse *exp;	/* A parsed out string expression */
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
    struct newFieldInfo *keyField;	/* Key field within table */
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

boolean isTotallySimple(char *s)
/* We are only alphanumerical and dotty things, we even begin with a alnum or _*/
{
char c = *s++;
if (!isalpha(c) && (c != '_'))
    return FALSE;
while ((c = *s++) != 0)
    {
    if (!(isalnum(c) || (c == '_') || (c == '.')))
	return FALSE;
    }
return TRUE;
}

struct newFieldInfo *parseFieldVal(char *name, char *input, char *fileName, int fileLineNumber)
/* return a newFieldInfo based on the contents of input, which are not destroyed */
{
/* Make up return structure. */

struct newFieldInfo *fv;
AllocVar(fv);
fv->name = cloneString(name);

char *s = skipLeadingSpaces(input);
if (isEmpty(s))
    {
    fv->type = fvVar;
    fv->val = cloneString(name);
    }
else
    {
    char c = s[0];
    if (c == '@')
	{
	char *val = fv->val = cloneString(skipLeadingSpaces(s+1));
	trimSpaces(val);
	if (isEmpty(val))
	    errAbort("Nothing following %c", c);
	fv->type = fvLink;
	}
    else 
        {
	if (isTotallySimple(s))
	    {
	    fv->val = cloneString(skipLeadingSpaces(s));
	    eraseTrailingSpaces(fv->val);
	    fv->type = fvVar;
	    }
	else
	    {
	    fv->val = cloneString(s);
	    fv->exp = strexParseString(fv->val, fileName, fileLineNumber);
	    fv->type = fvExp;
	    }
	}
    }
return fv;
}

struct symRec
/* Something we pass as a record to symLookup */
    {
    struct hash *hash;	    /* The hash with symbol to row index */
    char **row;		    /* The row we are working on */
    };

static char *symLookup(void *record, char *key)
/* Lookup symbol in hash */
{
struct symRec *rec = record;
struct hash *hash = rec->hash;
char **row = rec->row;
int rowIx = hashIntVal(hash, key);
return row[rowIx];
}


void selectUniqueIntoTable(struct fieldedTable *inTable,  struct hash *inFieldHash,
    struct newFieldInfo *fieldList, int keyFieldIx, struct fieldedTable *outTable)
/* Populate out table with selected rows from newTable */
{
struct hash *uniqHash = hashNew(0);
struct fieldedRow *fr;
int outFieldCount = outTable->fieldCount;
char *outRow[outFieldCount];

if (slCount(fieldList) != outFieldCount)	// A little cheap defensive programming on inputs
    internalErr();

struct dyString *csvScratch = dyStringNew(0);
for (fr = inTable->rowList; fr != NULL; fr = fr->next)
    {
    /* Create new row from a scan through old table */
    char **inRow = fr->row;
    int i;
    struct newFieldInfo *unlinkedFv;
    for (i=0, unlinkedFv=fieldList; i<outFieldCount && unlinkedFv != NULL; 
	++i, unlinkedFv = unlinkedFv->next)
	{
	/* Skip through links. */
	struct newFieldInfo *fv = unlinkedFv;
	while (fv->type == fvLink)
	    fv = fv->link;
	
	if (fv->type == fvVar)
	    outRow[i] = inRow[fv->oldIx];
	else if (fv->type == fvExp)
	    {
	    struct symRec symRec = {inFieldHash, inRow};
	    outRow[i] = strexEvalAsString(fv->exp, &symRec, symLookup);
	    verbose(2, "evaluated %s to %s\n", fv->val, outRow[i]);
	    }
	}

    char *key = outRow[keyFieldIx];
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
		key, outTable->fields[keyFieldIx], outTable->name);
	}
    }
dyStringFree(&csvScratch);
}



struct hash *hashFieldIx(char **fields, int fieldCount)
/* Create a hash filled with fields with integer valued indexes */
{
int i;
struct hash *hash = hashNew(0);
for (i=0; i<fieldCount; ++i)
   hashAdd(hash, fields[i], intToPt(i));
return hash;
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
    struct slPair *tableSl = specStanza;
    if (!sameString(tableSl->name, "table"))
        errAbort("stanza that doesn't start with 'table' ending line %d of %s",
	    lf->lineIx, lf->fileName);
    char *tableSpec = tableSl->val;
    char *tableName = nextWord(&tableSpec);
    char *keyFieldName = nextWord(&tableSpec);
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
	struct newFieldInfo *fv = parseFieldVal(newName, field->val, lf->fileName, lf->lineIx);
	fv->newIx = i;
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
    newTable->keyField = keyField;
    newTable->fieldList = fvList;
    newTable->table = outTable;
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
	  field->link = linkedTable->keyField;
	  }
      }
    }

struct hash *inFieldHash = hashFieldIx(inTable->fields, inTable->fieldCount);

/* Output tables */
for (newTable = newTableList; newTable != NULL; newTable = newTable->next)
    {
    /* Populate table */
    struct fieldedTable *outTable = newTable->table;
    selectUniqueIntoTable(inTable, inFieldHash,
	newTable->fieldList, newTable->keyField->newIx, outTable);

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
