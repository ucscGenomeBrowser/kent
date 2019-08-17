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
"        table tableName    key-column\n"
"        columnName1	sourceField1\n"
"        columnName2	sourceField2\n"
"              ...\n"
"if the sourceField is missing it is assumed to be a column of the same name in in.tsv\n"
"The sourceField can either be a column name in the in.tsv, or a string enclosed literal\n"
"or an @ followed by a table name, in which case it refers to the key of that table.\n"
"If the source column is in comma-separated-values format then the sourceField can include a\n"
"constant array index to pick out an item from the csv list.\n"
"You can also use strex expressions for more complicated situations.\n"
"            See src/lib/strex.doc\n"
"In addition to the table stanza there can be a 'define' stanza that defines variables\n"
"that can be used in sourceFields for tables.  This looks like:\n"
"         define\n"
"         variable1 sourceField1\n"
"         variable2 sourceField2\n"
);
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};


static int firstDifferentIx(char **aa, char **bb, int count)
/* Return true if first count of strings between aa and bb are the same */
{
int i;
for (i=0; i<count; ++i)
    if (!sameString(aa[i], bb[i]))
        return i;
return -1;
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

struct newFieldInfo *parseFieldVal(char *name, 
    char *input, char *fileName, int fileLineNumber, void *symbols, StrexLookup lookup)
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
	if (isTotallySimple(s) && lookup(symbols, s) == NULL)
	    {
	    fv->val = cloneString(skipLeadingSpaces(s));
	    eraseTrailingSpaces(fv->val);
	    fv->type = fvVar;
	    }
	else
	    {
	    fv->val = cloneString(s);
	    fv->exp = strexParseString(fv->val, fileName, fileLineNumber-1, symbols, lookup);
	    fv->type = fvExp;
	    }
	}
    }
return fv;
}

struct varVal
/* A variable, what we need to compute it, and it's value */
     {
     struct varVal *next;   /* Next in list */
     char *name;	    /* Variable name */
     struct strexParse *exp;  /* Parsed out expression. */
     char *val;		    /* Computed value - not owned by us. */
     };

struct varVal *varValNew(char *name, struct strexParse *exp)
/* Allocate new varVal structure */
{
struct varVal *v;
AllocVar(v);
v->name = cloneString(name);
v->exp = exp;
return v;
}


struct symRec
/* Something we pass as a record to symLookup */
    {
    struct hash *rowHash;	    /* The hash with symbol to row index */
    char **tableRow;		    /* The input row we are working on. You own.*/
    char **varRow;		    /* A slot for each computed variables results. We own */
    struct hash *varHash;	    /* Variables with varVal values */
    struct varVal *varList;	    /* List of all variables, same info as in hash above. */
    struct lm *lm;		    /* Local memory to use during eval phase */
    };

struct symRec *symRecNew(struct hash *rowHash, struct hash *varHash)
/* Return a new symRec. The rowHash is required and contains a hash with
 * values that are indexes into the table row.  The varHash is optional,
 * and if present should have variable names keying parseExp values. */
{
struct symRec *rec;
AllocVar(rec);
rec->rowHash = rowHash;
if (varHash != NULL)
    {
    rec->varHash = varHash;
    }
return rec;
}

static void symRecSetupPrecomputes(struct symRec *symbols)
/* Clear out any precomputed variable values - should be
 * executed on each new line of table. */
{
/* Clear up any old precomputes - sort of sad these can't currently
 * be shared between output tables. Probably not enough of a time
 * bottleneck to be worth fixing though. */
struct varVal *v;
for (v = symbols->varList; v != NULL; v = v->next)
    {
    freez(&v->val);
    }
}

static char *symLookup(void *record, char *key)
/* Lookup symbol in hash */
{
struct symRec *rec = record;
char *value = NULL;
struct varVal *v = hashFindVal(rec->varHash, key);
if (v != NULL)
    {
    if (v->val == NULL)
       {
       v->val = strexEvalAsString(v->exp, record, symLookup, NULL, NULL);
       }
    value = v->val;
    }
else
    {
    int rowIx = hashIntValDefault(rec->rowHash, key, -1);
    if (rowIx >= 0)
	value = rec->tableRow[rowIx];
    }
return value;
}


void selectUniqueIntoTable(struct fieldedTable *inTable,  struct symRec *symbols,
    char *specFile,  // Just for error reporting
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
    boolean firstSymInRow = TRUE;  // Avoid updating symbol table until we have to

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
	    if (firstSymInRow)
	        {
		symbols->tableRow = inRow;
		symRecSetupPrecomputes(symbols);
		firstSymInRow = FALSE;
		}
	    outRow[i] = strexEvalAsString(fv->exp, symbols, symLookup, NULL, NULL);
	    verbose(2, "evaluated %s to %s\n", fv->val, outRow[i]);
	    }
	}

    char *key = outRow[keyFieldIx];
    if (!isEmpty(key))
	{
	struct fieldedRow *uniqFr = hashFindVal(uniqHash, key);
	if (uniqFr == NULL)
	    {
	    uniqFr = fieldedTableAdd(outTable, outRow, outFieldCount, 0);
	    hashAdd(uniqHash, key, uniqFr);
	    }
	else    /* Do error checking for true uniqueness of key */
	    {
	    int differentIx = firstDifferentIx(outRow, uniqFr->row, outFieldCount);
	    if (differentIx >= 0)
		{
		warn("There is a problem with the key to table %s in %s", outTable->name, specFile);
		warn("%s %s", uniqFr->row[keyFieldIx], uniqFr->row[differentIx]);
		warn("%s %s", outRow[keyFieldIx], outRow[differentIx]);
		warn("both exist, so key doesn't specify a unique %s field", 
		    outTable->fields[differentIx]);
		errAbort("line %d of %s", fr->id, inTable->name);
		}
	    }
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
/* Read input table */
struct fieldedTable *inTable = fieldedTableFromTabFile(inTabFile, inTabFile, NULL, 0);
verbose(1, "Read %d columns, %d rows from %s\n", inTable->fieldCount, inTable->rowCount,
    inTabFile);

/* Create what we need for managing strex's symbol table. */
struct hash *inFieldHash = hashFieldIx(inTable->fields, inTable->fieldCount);
struct hash *varHash = hashNew(5);
struct symRec *symbols = symRecNew(inFieldHash, varHash); 
symbols->tableRow = inTable->fields;   // During parse pass fields will act as proxy for tableRow
/* Open spec file, check first real line, and maybe start defining variables. */

/* Snoop for a define stanza first that'll hold our variables. */
struct lineFile *lf = lineFileOpen(specFile, TRUE);
char *defLine;
if (!lineFileNextReal(lf, &defLine))
     errAbort("%s is empty", specFile);
if (startsWithWord("define",  defLine))  // Whee, we got vars! 
    {
    char *varName, *varSpec;
    while (raNextTagVal(lf, &varName, &varSpec, NULL))
        {
	if (varSpec == NULL)
	    errAbort("Expecting expression for variable %s line %d of %s", varName,
		lf->lineIx, lf->fileName);
	verbose(2, "var %s (%s)\n", varName, varSpec);
	struct strexParse *exp = strexParseString(varSpec, lf->fileName, lf->lineIx-1, 
	    symbols, symLookup);
	struct varVal *v = varValNew(varName, exp);
	hashAdd(varHash, varName, v);
	slAddHead(&symbols->varList, v);
	}
    slReverse(&symbols->varList);
    }
else
    lineFileReuse(lf);


/* Read in rest of spec file as ra stanzas full of tables more or less */
struct newTableInfo *newTableList = NULL, *newTable;
while (raSkipLeadingEmptyLines(lf, NULL))
    {
    /* Read first tag, which we know is there because it's right after raSkipLeadingEmptyLines.
     * Make sure the tag is table, and that there is a following table name and key field name. */
    char *tableString, *tableSpec;
    raNextTagVal(lf, &tableString, &tableSpec, NULL);
    verbose(2, "Processing table %s '%s' line %d of %s\n",  tableString, tableSpec, 
	lf->lineIx, lf->fileName);
    if (!sameString(tableString, "table"))
        errAbort("stanza that doesn't start with 'table' ending line %d of %s",
	    lf->lineIx, lf->fileName);
    char *tableName = nextWord(&tableSpec);
    char *keyFieldName = cloneString(nextWord(&tableSpec));
    if (isEmpty(keyFieldName))
       errAbort("No key field for table %s line %d of %s", tableName, lf->lineIx, lf->fileName);

    /* Start filling out newTable with these fields */
    AllocVar(newTable);
    newTable->name = cloneString(tableName);
    tableName = newTable->name;  /* Keep this handy variable. */

    /* Make up field list out of rest of the stanza */
    struct newFieldInfo *fvList = NULL;
    char *fieldName, *fieldSpec;
    int fieldCount = 0;
    while (raNextTagVal(lf, &fieldName, &fieldSpec, NULL))
        {
	verbose(2, "  fieldName %s fieldSpec (%s)\n", fieldName, fieldSpec);
	struct newFieldInfo *fv = parseFieldVal(fieldName, 
	    fieldSpec, lf->fileName, lf->lineIx, symbols, symLookup);
	if (fv->type == fvVar)
	    {
	    char *oldName = fieldSpec;
	    if (isEmpty(oldName))
	       oldName = fieldName;
	    int oldIx = stringArrayIx(oldName, inTable->fields, inTable->fieldCount);
	    if (oldIx < 0)
	       errAbort("%s doesn't exist in the %d fields of %s line %d of %s", 
		oldName, inTable->fieldCount, inTable->name,
		    lf->lineIx, lf->fileName);
	    fv->oldIx = oldIx;
	    }
	fv->newIx = fieldCount++;
	slAddHead(&fvList, fv);
	}
    slReverse(&fvList);

    /* Create array of field names for output. */
    char *fieldNames[fieldCount];
    int i;
    struct newFieldInfo *fv = NULL;
    for (i=0, fv=fvList; i<fieldCount; ++i, fv=fv->next)
	fieldNames[i] = fv->name;

    /* Create empty output table and track which fields of input go to output. */
    struct fieldedTable *outTable = fieldedTableNew(tableName, fieldNames, fieldCount);
    outTable->startsSharp = inTable->startsSharp;

    /* Make sure that key field is actually in field list */
    struct newFieldInfo *keyField = findField(fvList, keyFieldName);
    if (keyField == NULL)
       errAbort("key field %s is not found in field list for %s in %s\n", 
	keyFieldName, tableName, lf->fileName);

    /* Allocate structure to save results of this pass in and so so. */
    newTable->keyField = keyField;
    newTable->fieldList = fvList;
    newTable->table = outTable;
    slAddHead(&newTableList, newTable);

    /* Clean up */
    freez(&keyFieldName);
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

makeDirsOnPath(outDir);

/* Output tables */
verbose(1, "Outputting %d tables to %s\n", slCount(newTableList), outDir);
for (newTable = newTableList; newTable != NULL; newTable = newTable->next)
    {
    /* Populate table */
    struct fieldedTable *outTable = newTable->table;
    selectUniqueIntoTable(inTable, symbols, specFile,
	newTable->fieldList, newTable->keyField->newIx, outTable);

    /* Create output file name and save file. */
    char outTabName[FILENAME_LEN];
    safef(outTabName, sizeof(outTabName), "%s/%s.tsv", outDir, newTable->name);
    verbose(1, "Writing %s of %d columns %d rows\n",  
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
