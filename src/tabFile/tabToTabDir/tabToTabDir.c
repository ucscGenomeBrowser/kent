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
#include "localmem.h"

char *clId = NULL;  // Flag set from command line to add an id column
int clStartId = 1;  // What number id column should start with

void usage()
/* Explain usage and exit. */
{
errAbort(
"tabToTabDir - Convert a large tab-separated table to a directory full of such tables according\n"
"to a specification.\n"
"command line:\n"
"   tabToTabDir in.tsv spec.x outDir\n"
"options:\n"
"   -id=fieldName - Add a numeric id field of given name that starts at 1 and autoincrements \n"
"                   for each table\n"
"   -startId=fieldName - sets starting ID to be something other than 1\n"
"usage:\n"
"   in.tsv is a tab-separated input file.  The first line is the label names and may start with #\n"
"   spec.txt is a file that says what columns to put into the output, described in more detail below\n"
"   outDir is a directory that will be populated with tab-separated files with no duplicate records\n"
"The spec.x file contains one blank line separated stanza per output table.\n"
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
"\n"
"If there is a '?' in front of the column name it is taken to mean an optional field.\n"
"if the corresponding source field does not exist then there's no error (and no output)\n"
"for that column\n"
"\n"
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
   {"id", OPTION_STRING},
   {"startId", OPTION_INT},
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
    boolean optional;		/* If true, then skip rather than stop if old field doesn't exist */
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
    boolean unroll;		    /* If true it's a table we unroll from arrays */
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
    struct hash *varHash;	    /* Variables with varVal values */
    struct varVal *varList;	    /* List of all variables, same info as in hash above. */
    struct lm *lm;		    /* Local memory to use during eval phase */
    char *fileName;		    /* File name of big input tab file */
    int lineIx;			    /* Line number of big input tab file */
    };

struct symRec *symRecNew(struct hash *rowHash, struct hash *varHash, char *fileName, int lineIx)
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
    rec->fileName = fileName;
    rec->lineIx = lineIx;
    }
return rec;
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

int gTotalFields = 0, gStrexFields = 0, gLinkFields = 0;

struct newFieldInfo *parseFieldVal(char *name, 
    char *input, char *fileName, int fileLineNumber, struct symRec  *symbols, StrexLookup lookup)
/* return a newFieldInfo based on the contents of input, which are not destroyed */
{
/* Make up return structure. */

struct newFieldInfo *fv;
AllocVar(fv);
char c = name[0];
if (c == '?')
    {
    fv->optional = TRUE;
    name += 1;
    }
else if (!isalpha(c) && (c != '_'))
    {
    errAbort("Strange character %c starting line %d of %s", c, fileLineNumber, fileName);
    }
fv->name = cloneString(name);

char *s = trimSpaces(input);
if (isEmpty(s))
    {
    fv->type = fvVar;
    s = fv->val = cloneString(name);
    }
c = s[0];
if (c == '@')
    {
    char *val = fv->val = cloneString(skipLeadingSpaces(s+1));
    if (isEmpty(val))
	errAbort("Nothing following %c", c);
    fv->type = fvLink;
    ++gLinkFields;
    }
else 
    {
    if (isTotallySimple(s) && hashLookup(symbols->varHash, s) == NULL)
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
	gStrexFields += 1;
	}
    }
gTotalFields += 1;
return fv;
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

static void warnHandler(void *record, char *message)
/* Our warn handler keeps a little hash to keep from repeating
 * messages for every row of the input sometimes. */
{
struct symRec *rec = record;
static struct hash *uniq = NULL;
if (uniq == NULL) uniq = hashNew(0);
if (hashLookup(uniq, message) == NULL)
    {
    hashAdd(uniq, message, NULL);
    warn("%s line %d of %s", message, rec->lineIx, rec->fileName);
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
       v->val = strexEvalAsString(v->exp, record, symLookup, warnHandler, NULL);
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

static char *symExists(void *record, char *key)
/* Lookup symbol in hash to see if a variable is there but not to 
 * calculate it's values. */
{
struct symRec *rec = record;
struct varVal *v = hashFindVal(rec->varHash, key);
if (v != NULL)
    {
    return v->name;
    }
else
    {
    int rowIx = hashIntValDefault(rec->rowHash, key, -1);
    if (rowIx < 0)
        return NULL;
    return rec->tableRow[rowIx];
    }
}



void selectUniqueIntoTable(struct fieldedTable *inTable,  struct symRec *symbols,
    char *specFile,  // Just for error reporting
    struct newFieldInfo *fieldList, int keyFieldIx, struct fieldedTable *outTable)
/* Populate out table with selected unique rows from newTable */
{
struct hash *uniqHash = hashNew(0);
struct fieldedRow *fr;
int outFieldCount = outTable->fieldCount;
char *outRow[outFieldCount];

if (slCount(fieldList) != outFieldCount)  // A little cheap defensive programming on inputs
    internalErr();

struct dyString *csvScratch = dyStringNew(0);
for (fr = inTable->rowList; fr != NULL; fr = fr->next)
    {
    symbols->lineIx = fr->id;
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
	    outRow[i] = strexEvalAsString(fv->exp, symbols, symLookup, warnHandler, NULL);
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

struct fieldedTable *unrollTable(struct fieldedTable *input)
/* Unroll input table,  which has to be filled with lockstepped CSV fields */
{
/* Make output table with fields matching input */
int fieldCount = input->fieldCount;
struct fieldedTable *output = fieldedTableNew(input->name, input->fields, fieldCount);
output->startsSharp = input->startsSharp;

/* We are going to be lots of splicing and dicing, so have some scratch space,
 * including some we'll store with the output tables local memory pool. */
struct lm *lm = output->lm;
struct dyString *scratch = dyStringNew(0);

struct fieldedRow *inRow;
for (inRow = input->rowList; inRow != NULL; inRow = inRow->next)
    {
    /* We are going to parse a bunch of csv's in parallel */
    char *inPos[fieldCount];
    int i;
    for (i=0; i<fieldCount; ++i)
	inPos[i] = inRow->row[i];

    /* With this loop we parse out the next csv from all fields, and make sure that
     * they all actually do have the same number of values */
    int unrollCount = 0;
    for (;;)
       {
       char *uncsvRow[fieldCount];
       boolean anyNull = FALSE, allNull = TRUE;
       for (i=0; i<fieldCount; ++i)
           {
	   char *oneVal = csvParseNext(&inPos[i], scratch);
	   if (oneVal == NULL)
	       anyNull = TRUE;
	   else
	       allNull = FALSE;
	   uncsvRow[i] = lmCloneString(lm, oneVal);
	   }
       if (anyNull)
           {
	   if (allNull)
	        break;	    // All is good!
	   else
	        errAbort("Can't unroll %s since not all fields have the same numbers of values.\n"
		         "In row %d some have %d values, some more", 
			 input->name, inRow->id, unrollCount);
	   }
       ++unrollCount;
       fieldedTableAdd(output, uncsvRow, fieldCount, unrollCount);
       }
    }
return output;
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
struct symRec *symbols = symRecNew(inFieldHash, varHash, inTabFile, 0); 
symbols->tableRow = inTable->fields;   // During parse pass fields will act as proxy for tableRow

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
	    symbols, symExists);
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
    boolean unroll = FALSE;
    raNextTagVal(lf, &tableString, &tableSpec, NULL);
    verbose(2, "Processing table %s '%s' line %d of %s\n",  tableString, tableSpec, 
	lf->lineIx, lf->fileName);
    if (sameString(tableString, "unroll"))
        unroll = TRUE;
    else if (!sameString(tableString, "table"))
        errAbort("stanza that doesn't start with 'table' or 'unroll' ending line %d of %s",
	    lf->lineIx, lf->fileName);
    char *tableName = nextWord(&tableSpec);
    char *keyFieldName = cloneString(nextWord(&tableSpec));
    if (isEmpty(keyFieldName))
       errAbort("No key field for table %s line %d of %s", tableName, lf->lineIx, lf->fileName);

    /* Start filling out newTable with these fields */
    AllocVar(newTable);
    newTable->unroll = unroll;
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
	    fieldSpec, lf->fileName, lf->lineIx, symbols, symExists);
	if (fv->type == fvVar)
	    {
	    char *oldName = fieldSpec;
	    if (isEmpty(oldName))
	       oldName = fieldName;
	    int oldIx = stringArrayIx(oldName, inTable->fields, inTable->fieldCount);
	    if (oldIx < 0)
	       {
	       if (fv->optional)
	           continue;	    // Just skip optional ones we don't have
	       errAbort("%s doesn't exist in the %d fields of %s line %d of %s", 
		oldName, inTable->fieldCount, inTable->name,
		    lf->lineIx, lf->fileName);
	       }
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

    /* If need be unroll table */
    if (newTable->unroll)
        {
	outTable = unrollTable(outTable);
	}

    /* Create output file name and save file. */
    char outTabName[FILENAME_LEN];
    safef(outTabName, sizeof(outTabName), "%s/%s.tsv", outDir, newTable->name);
    verbose(1, "Writing %s of %d columns %d rows\n",  
	outTabName, outTable->fieldCount, outTable->rowCount);
    fieldedTableToTabFileWithId(outTable, outTabName, clId, clStartId);
    }
verbose(1, "%d fields, %d (%g%%) evaluated with strex, %d (%.2f) links\n", 
    gTotalFields,  gStrexFields, 100.0 * gStrexFields / gTotalFields,
    gLinkFields, 100.0 * gLinkFields/gTotalFields);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clId = optionVal("id", clId);
clStartId = optionInt("startId", clStartId);
if (argc != 4)
    usage();
tabToTabDir(argv[1], argv[2], argv[3]);
return 0;
}
