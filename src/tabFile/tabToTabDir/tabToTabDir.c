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
"to a specification. The program is designed to make it relatively easy to unpack overloaded\n"
"single fields into multiple fields, and to created normalized less redundant representations.\n"
"The command line is:\n"
"   tabToTabDir in.tsv spec.x outDir\n"
"options:\n"
"   -id=fieldName - Add a numeric id field of given name that starts at 1 and autoincrements \n"
"                   for each table\n"
"   -startId=fieldName - sets starting ID to be something other than 1\n"
"usage:\n"
"   in.tsv is a tab-separated input file.  The first line is the label names and may start with #\n"
"   spec.x is a file that says what columns to put into the output, described in more detail below.\n"
"The spec.x file contains one blank line separated stanza per output table.\n"
"Each stanza should look like:\n"
"        table tableName    key-column\n"
"        columnName1	sourceExpression1\n"
"        columnName2	sourceExpression2\n"
"              ...\n"
"if the sourceExpression is missing it is assumed to be a just a field of the same name from in.tsv\n"
"Otherwise the sourceExpression can be a strex expression involving fields in in.tsv.\n"
"\n"
"Each output table has duplicate rows merged using the key-column to determine uniqueness.\n"
"Please see tabToTabDir.doc in the source code for more information on what can go into spec.x.\n"
);
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"id", OPTION_STRING},
   {"startId", OPTION_INT},
   {NULL, 0},
};


enum fieldValType
/* A type */
    {
    fvVar, fvLink, fvExp, fvCount,
    };

enum combineType
/* A way to combine values from a field */
    {
    ctCount, ctUniq, ctStats,
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
    struct hash *combineHash;     /* If it's type fvCombine an int valued hash here */
    enum combineType combineType;  /* How to combine if multiple values allowed */
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
    s = cloneString(name);
    fv->type = fvVar;
    }
c = s[0];
if (c == '@')
    {
    char *val = fv->val = cloneString(skipLeadingSpaces(s+1));
    if (isEmpty(val))
	errAbort("Nothing following %c line %d of %s", c, fileLineNumber, fileName);
    fv->type = fvLink;
    ++gLinkFields;
    }
else 
    {
    if (c == '$')
	{
	char *command = skipLeadingSpaces(s+1);
	s = skipToSpaces(command);
	fv->combineHash = hashNew(0);
	if (startsWithWord("count", command))
	    {
	    if (!isEmpty(s))
		errAbort("Something following $count line %d of %s", fileLineNumber, fileName);
	    fv->combineType = ctCount;
	    fv->type = fvCount;
	    }
        else if (startsWithWord("list", command))
	    {
	    fv->combineType = ctUniq;
	    if (isEmpty(skipLeadingSpaces(s)))
	        errAbort("Missing parameters to $list line %d of %s", fileLineNumber, fileName);
	    }
        else if (startsWithWord("stats", command))
	    {
	    fv->combineType = ctStats;
	    if (isEmpty(skipLeadingSpaces(s)))
	        errAbort("Missing parameters to $stats line %d of %s", fileLineNumber, fileName);
	    }
	else
	    {
	    errAbort("Unrecognized command $%s line %d of %s", command, fileLineNumber, fileName);
	    }
	}
    if (fv->combineHash == NULL || fv->combineType != ctCount)
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


struct uniqValLister
/* A list of unique values */
   {
   struct uniqValLister *next;
   struct dyString *csv;    // Comma separated list of values seen so far
   struct hash *uniq;	    // Hash of values seen so far.
   };

struct oneValCount
/* Counts occurences of one */
    {
    struct oneValCount *next;
    char *name;		// Name - not allocated here
    int count;		// Number of times seen
    };

int oneValCountCmp(const void *va, const void *vb)
/* Compare two oneValCounts. */
{
const struct oneValCount *a = *((struct oneValCount **)va);
const struct oneValCount *b = *((struct oneValCount **)vb);
return b->count - a->count;
}

struct uniqValCounter
/* A list of unique values and how often they occur */
    {
    struct uniqValCounter *next;
    struct hash *uniq;	    // Integer valued list of values seen so far - oneValCount values
    struct oneValCount *list;    // List of uniq values seen so far
    int total;	    /* Total of counts in list */
    };


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

for (fr = inTable->rowList; fr != NULL; fr = fr->next)
    {
    symbols->lineIx = fr->id;
    /* Create new row from a scan through old table */
    char **inRow = fr->row;
    int i;
    struct newFieldInfo *unlinkedFv;
    boolean firstSymInRow = TRUE;  // Avoid updating symbol table until we have to

    /* Fill out the normal fields */
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
	else
	    outRow[i] = NULL;
	}

    char *key = outRow[keyFieldIx];
    if (!isEmpty(key))
	{
	/* Do any aggregate fields */
	struct newFieldInfo *fv;
	for (fv = fieldList; fv != NULL; fv = fv->next)
	    {
	    if (fv->combineHash != NULL)
		{
		switch (fv->combineType)
		    {
		    case ctCount:
			hashIncInt(fv->combineHash, key);
			break;
		    case ctUniq:
		        {
			struct uniqValLister *lister = hashFindVal(fv->combineHash, key);
			if (lister == NULL)
			    {
			    AllocVar(lister);
			    lister->csv = dyStringNew(0);
			    lister->uniq = hashNew(0);
			    hashAdd(fv->combineHash, key, lister);
			    }
			char *val = outRow[fv->newIx];
			if (hashLookup(lister->uniq, val) == NULL)
			    {
			    hashAdd(lister->uniq, val, NULL);
			    csvEscapeAndAppend(lister->csv, val);
			    }
			break;
			}
		    case ctStats:
		        {
			struct uniqValCounter *counter = hashFindVal(fv->combineHash, key);
			if (counter == NULL)
			    {
			    AllocVar(counter);
			    counter->uniq = hashNew(0);
			    hashAdd(fv->combineHash, key, counter);
			    }
			char *val = outRow[fv->newIx];
			struct oneValCount *one = hashFindVal(counter->uniq, val);
			if (one == NULL)
			    {
			    AllocVar(one);
			    hashAddSaveName(counter->uniq, val, one, &one->name);
			    slAddHead(&counter->list, one);
			    }
			one->count += 1;
			counter->total += 1;
			break;
			}
		    }
		}
	    }

	struct fieldedRow *uniqFr = hashFindVal(uniqHash, key);
	if (uniqFr == NULL)
	    {
	    uniqFr = fieldedTableAdd(outTable, outRow, outFieldCount, 0);
	    hashAdd(uniqHash, key, uniqFr);
	    }
	else    /* Do error checking for true uniqueness of key */
	    {
	    int i;
	    char **uniqRow = uniqFr->row;
	    for (i=0,fv=fieldList; fv != NULL; fv = fv->next, ++i)
	        {
		if (fv->combineHash == NULL)
		     {
		     if (!sameString(uniqRow[i], outRow[i]))
		         {
			 uglyf("fv->type of %s is %d\n", fv->name, (int)fv->type);
			 warn("There is a problem with the key to table %s in %s", 
			     outTable->name, specFile);
			 warn("%s %s", uniqFr->row[keyFieldIx], uniqFr->row[i]);
			 warn("%s %s", outRow[keyFieldIx], outRow[i]);
			 warn("both exist, so key doesn't specify a unique %s field", 
			     outTable->fields[i]);
			 errAbort("line %d of %s", fr->id, inTable->name);
			 }
		     }
		}
	    }
	}
    }

/* Make a loop through output table fixing up aggregation-oriented fields */
    {
    struct newFieldInfo *fv;
    for (fv = fieldList; fv != NULL; fv = fv->next)
        {
	if (fv->combineHash != NULL)
	    {
	    for (fr = outTable->rowList; fr != NULL; fr = fr->next)
	        {
		char *key = fr->row[keyFieldIx];
		switch (fv->combineType)
		    {
		    case ctCount:
			{
			char countBuf[16];
			safef(countBuf, sizeof(countBuf), "%d", hashIntVal(fv->combineHash, key));
			fr->row[fv->newIx] = lmCloneString(outTable->lm, countBuf);
			break;
			}
		    case ctUniq:
			{
			struct uniqValLister *lister = hashMustFindVal(fv->combineHash, key);
			fr->row[fv->newIx] = lister->csv->string;
			break;
			}
		    case ctStats:
		        {
			struct uniqValCounter *counter = hashMustFindVal(fv->combineHash, key);
			struct dyString *dy = dyStringNew(0);
			struct oneValCount *el;
			slSort(&counter->list, oneValCountCmp);
			for (el = counter->list; el != NULL; el = el->next)
			    {
			    dyStringPrintf(dy, "%s(%d %d%%),", el->name, el->count, 
				round(100.0 * el->count / counter->total));
			    }
			fr->row[fv->newIx] = dyStringCannibalize(&dy);
			break;
			}
		    }
		}
	    }
	}
    }
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
	       errAbort("'%s' doesn't exist in the %d fields of %s line %d of %s", 
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
