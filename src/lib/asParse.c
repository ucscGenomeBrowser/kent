/* asParse - parse out an autoSql .as file. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "tokenizer.h"
#include "dystring.h"
#include "asParse.h"
#include "sqlNum.h"


/* n.b. switched double/float from %f to %g to partially address losing
 * precision.  Values like 2e-12 were being rounded to 0.0 with %f.  While %g
 * doesn't match the precision of the database fields, specifying a larger
 * precision with %g resulted in numbers like 1.9999999999999999597733e-12,
 *  which might impact load time.  This issue needs more investigation.*/
struct asTypeInfo asTypes[] = {
    {t_double,  "double",  FALSE, FALSE, "double",            "double",        "Double",   "Double",   "%g",   "FloatField"},
    {t_float,   "float",   FALSE, FALSE, "float",             "float",         "Float",    "Float",    "%g",   "FloatField"},
    {t_char,    "char",    FALSE, FALSE, "char",              "char",          "Char",     "Char",     "%c",   "CharField"},
    {t_int,     "int",     FALSE, FALSE, "int",               "int",           "Signed",   "Signed",   "%d",   "IntegerField"},
    {t_uint,    "uint",    TRUE,  FALSE, "int unsigned",      "unsigned",      "Unsigned", "Unsigned", "%u",   "PositiveIntegerField"},
    {t_short,   "short",   FALSE, FALSE, "smallint",          "short",         "Short",    "Signed",   "%d",   "SmallIntegerField"},
    {t_ushort,  "ushort",  TRUE,  FALSE, "smallint unsigned", "unsigned short","Ushort",   "Unsigned", "%u",   "SmallPositiveIntegerField"},
    {t_byte,    "byte",    FALSE, FALSE, "tinyint",           "signed char",   "Byte",     "Signed",   "%d",   "SmallIntegerField"},
    {t_ubyte,   "ubyte",   TRUE,  FALSE, "tinyint unsigned",  "unsigned char", "Ubyte",    "Unsigned", "%u",   "SmallPositiveIntegerField"},
    {t_off,     "bigint",  FALSE, FALSE, "bigint",            "long long",     "LongLong", "LongLong", "%lld", "BigIntegerField"},
    {t_string,  "string",  FALSE, TRUE,  "varchar(255)",      "char *",        "String",   "String",   "%s",   "CharField"},
    {t_lstring, "lstring", FALSE, TRUE,  "longblob",          "char *",        "String",   "String",   "%s",   "TextField"},
    {t_enum,    "enum",    FALSE, FALSE, "enum",              "!error!",       "Enum",     "Enum",     NULL,   "CharField"},
    {t_set,     "set",     FALSE, FALSE, "set",               "unsigned",      "Set",      "Set",      NULL,   NULL},
    {t_object,  "object",  FALSE, FALSE, "longblob",          "!error!",       "Object",   "Object",   NULL,   "TextField"},
    {t_object,  "table",   FALSE, FALSE, "longblob",          "!error!",       "Object",   "Object",   NULL,   "TextField"},
    {t_simple,  "simple",  FALSE, FALSE, "longblob",          "!error!",       "Simple",   "Simple",   NULL,   "TextField"},
};

struct asTypeInfo *asTypeFindLow(char *name)
/* Return asType for a low level type of given name.  (Low level because may be decorated 
 * with array or pointer  stuff at a higher level).  Returns NULL if not found. */
{
int i;
for (i=0; i<ArraySize(asTypes); ++i)
    {
    if (sameWord(asTypes[i].name, name))
	return &asTypes[i];
    }
return NULL;
}

static struct asTypeInfo *findLowType(struct tokenizer *tkz)
/* Return low type info.  Squawk and die if s doesn't
 * correspond to one. */
{
struct asTypeInfo *type = asTypeFindLow(tkz->string);
if (type == NULL)
    tokenizerErrAbort(tkz, "Unknown type '%s'", tkz->string);
return type;
}

static void sqlSymDef(struct asColumn *col, struct dyString *dy)
/* print symbolic column definition for sql */
{
dyStringPrintf(dy, "%s(", col->lowType->sqlName);
struct slName *val;
for (val = col->values; val != NULL; val = val->next)
    {
    dyStringPrintf(dy, "\"%s\"", val->name);
    if (val->next != NULL)
        dyStringAppend(dy, ", ");
    }
dyStringPrintf(dy, ")");
}

struct dyString *asColumnToSqlType(struct asColumn *col)
/* Convert column to a sql type spec in returned dyString */
{
struct asTypeInfo *lt = col->lowType;
struct dyString *type = dyStringNew(32);
if ((lt->type == t_enum) || (lt->type == t_set))
    sqlSymDef(col, type);
else if (col->isList || col->isArray)
    dyStringPrintf(type, "longblob");
else if (lt->type == t_char)
    dyStringPrintf(type, "char(%d)", col->fixedSize ? col->fixedSize : 1);
else
    dyStringPrintf(type, "%s", lt->sqlName);
return type;
}

char *asTypeNameFromSqlType(char *sqlType)
/* Return the autoSql type name (not enum) for the given SQL type, or NULL.
 * Don't attempt to free result. */
// Unfortunately, when sqlType is longblob, we don't know whether it's a list
// of some type or an lstring.  :(
{
if (sqlType == NULL)
    return NULL;
// For comparison with asTypes[*], we need to strip '(...)' strings from all types
// except 'varchar' which must be 'varchar(255)'.  For 'char', we need to remember
// what was in the '(...)' so we can add back the '[...]' after type comparison.
boolean isArray = FALSE;
int arraySize = 0;
static char buf[1024];
if (startsWith("varchar", sqlType))
    safecpy(buf, sizeof(buf), "varchar(255)");
else
    {
    safecpy(buf, sizeof(buf), sqlType);
    char *leftParen = strstr(buf, " (");
    if (leftParen == NULL)
	leftParen = strchr(buf, '(');
    if (leftParen != NULL)
	{
	isArray = startsWith("char", sqlType);
	char *rightParen = strrchr(leftParen, ')');
	if (rightParen != NULL)
	    {
	    *rightParen = '\0';
	    arraySize = atoi(leftParen+1);
	    strcpy(leftParen, rightParen+1);
	    }
	else
	    errAbort("asTypeNameFromSqlType: mismatched ( in sql type def'%s'", sqlType);
	}
    }
int i;
for (i = 0;  i < ArraySize(asTypes);  i++)
    if (sameString(buf, asTypes[i].sqlName))
	{
	if (isArray)
	    {
	    int typeLen = strlen(buf);
	    safef(buf+typeLen, sizeof(buf)-typeLen, "[%d]", arraySize);
	    return buf;
	    }
	else
	    return asTypes[i].name;
	}
return NULL;
}

static struct asColumn *mustFindColumn(struct asObject *table, char *colName)
/* Return column or die. */
{
struct asColumn *col;

for (col = table->columnList; col != NULL; col = col->next)
    {
    if (sameWord(col->name, colName))
	return col;
    }
errAbort("Couldn't find column %s", colName);
return NULL;
}

static struct asObject *findObType(struct asObject *objList, char *obName)
/* Find object with given name. */
{
struct asObject *obj;
for (obj = objList; obj != NULL; obj = obj->next)
    {
    if (sameWord(obj->name, obName))
	return obj;
    }
return NULL;
}

static void asParseColArraySpec(struct tokenizer *tkz, struct asObject *obj,
                                struct asColumn *col)
/* parse the array length specification for a column */
{
if (col->lowType->type == t_simple)
    col->isArray = TRUE;
else
    col->isList = TRUE;
tokenizerMustHaveNext(tkz);
if (isdigit(tkz->string[0]))
    {
    col->fixedSize = atoi(tkz->string);
    tokenizerMustHaveNext(tkz);
    }
else if (isalpha(tkz->string[0]))
    {
#ifdef OLD
    if (obj->isSimple)
        tokenizerErrAbort(tkz, "simple objects can't include variable length arrays\n");
#endif /* OLD */
    col->linkedSizeName = cloneString(tkz->string);
    col->linkedSize = mustFindColumn(obj, col->linkedSizeName);
    col->linkedSize->isSizeLink = TRUE;
    tokenizerMustHaveNext(tkz);
    }
else
    tokenizerErrAbort(tkz, "must have column name or integer inside []'s\n");
tokenizerMustMatch(tkz, "]");
}

static void asParseColSymSpec(struct tokenizer *tkz, struct asObject *obj,
                              struct asColumn *col)
/* parse the enum or set symbolic values for a column */
{
tokenizerMustHaveNext(tkz);
while (tkz->string[0] != ')')
    {
    slSafeAddHead(&col->values, slNameNew(tkz->string));
    /* look for `,' or `)', but allow `,' after last token */
    tokenizerMustHaveNext(tkz);
    if (!((tkz->string[0] == ',') || (tkz->string[0] == ')')))
        tokenizerErrAbort(tkz, "expected `,' or `)' got `%s'", tkz->string);
    if (tkz->string[0] != ')')
        tokenizerMustHaveNext(tkz);
    }
tokenizerMustMatch(tkz, ")");
slReverse(&col->values);
}

int tokenizerUnsignedVal(struct tokenizer *tkz)
/* Ensure current token is an unsigned integer and return value */
{
if (!isdigit(tkz->string[0]))
    {
    struct lineFile *lf = tkz->lf;
    errAbort("expecting number got %s line %d of %s", tkz->string, lf->lineIx, lf->fileName);
    }
return sqlUnsigned(tkz->string);
}

struct asIndex *asParseIndex(struct tokenizer *tkz, struct asColumn *col)
/* See if there's an index key word and if so parse it and return an asIndex
 * based on it.  If not an index key word then just return NULL. */
{
struct asIndex *index = NULL;
if (sameString(tkz->string, "primary") || sameString(tkz->string, "unique")
	|| sameString(tkz->string, "index") )
    {
    AllocVar(index);
    index->type = cloneString(tkz->string);
    tokenizerMustHaveNext(tkz);
    if (tkz->string[0] == '[')
	{
	tokenizerMustHaveNext(tkz);
	index->size = tokenizerUnsignedVal(tkz);
	tokenizerMustHaveNext(tkz);
	tokenizerMustMatch(tkz, "]");
	}
    }
return index;
}

static void asParseColDef(struct tokenizer *tkz, struct asObject *obj)
/* Parse a column definition */
{
struct asColumn *col;
AllocVar(col);

col->lowType = findLowType(tkz);
tokenizerMustHaveNext(tkz);

if (col->lowType->type == t_object || col->lowType->type == t_simple)
    {
    col->obName = cloneString(tkz->string);
    tokenizerMustHaveNext(tkz);
    }

if (tkz->string[0] == '[')
    asParseColArraySpec(tkz, obj, col);
else if (tkz->string[0] == '(')
    asParseColSymSpec(tkz, obj, col);

col->name = cloneString(tkz->string);
tokenizerMustHaveNext(tkz);
col->index = asParseIndex(tkz, col);
if (sameString(tkz->string, "auto"))
    {
    col->autoIncrement = TRUE;
    if (!asTypesIsInt(col->lowType->type))
        errAbort("error - auto with non-integer type for field %s", col->name);
    tokenizerMustHaveNext(tkz);
    }
tokenizerMustMatch(tkz, ";");
col->comment = cloneString(tkz->string);
tokenizerMustHaveNext(tkz);
if (col->lowType->type == t_char && col->fixedSize != 0)
    col->isList = FALSE;	/* It's not really a list... */
slAddHead(&obj->columnList, col);
}

static struct asObject *asParseTableDef(struct tokenizer *tkz)
/* Parse a table or object definintion */
{
struct asObject *obj;
AllocVar(obj);
if (sameWord(tkz->string, "table"))
    obj->isTable = TRUE;
else if (sameWord(tkz->string, "simple"))
    obj->isSimple = TRUE;
else if (sameWord(tkz->string, "object"))
    ;
else
    tokenizerErrAbort(tkz, "Expecting 'table' or 'object' got '%s'", tkz->string);
tokenizerMustHaveNext(tkz);
obj->name = cloneString(tkz->string);
tokenizerMustHaveNext(tkz);
obj->comment = cloneString(tkz->string);

/* parse columns */
tokenizerMustHaveNext(tkz);
tokenizerMustMatch(tkz, "(");
while (tkz->string[0] != ')')
    asParseColDef(tkz, obj);
slReverse(&obj->columnList);
return obj;
}

static void asLinkEmbeddedObjects(struct asObject *obj, struct asObject *objList)
/* Look up any embedded objects. */
{
struct asColumn *col;
for (col = obj->columnList; col != NULL; col = col->next)
    {
    if (col->obName != NULL)
        {
        if ((col->obType = findObType(objList, col->obName)) == NULL)
            errAbort("%s used but not defined", col->obName);
        if (obj->isSimple)
            {
            if (!col->obType->isSimple)
                errAbort("Simple object %s with embedded non-simple object %s",
                    obj->name, col->name);
            }
        }
    }
}

static struct asObject *asParseTokens(struct tokenizer *tkz)
/* Parse file into a list of objects. */
{
struct asObject *objList = NULL;
struct asObject *obj;

while (tokenizerNext(tkz))
    {
    obj = asParseTableDef(tkz);
    if (findObType(objList, obj->name))
        tokenizerErrAbort(tkz, "Duplicate definition of %s", obj->name);
    slAddTail(&objList, obj);
    }

for (obj = objList; obj != NULL; obj = obj->next)
    asLinkEmbeddedObjects(obj, objList);

return objList;
}

char *asTypesIntSizeDescription(enum asTypes type)
/* Return description of integer size.  Do not free. */
{
int size = asTypesIntSize(type);
switch (size)
    {
    case 1:
	return "byte";
    case 2:
	return "short integer";
    case 4:
	return "integer";
    case 8:
	return "long long integer";
    default:
        errAbort("Unexpected error in asTypesIntSizeDescription: expecting integer type size of 1, 2, 4, or 8.  Got %d.", size);
	return NULL; // happy compiler, never gets here
    
    }
}

int asTypesIntSize(enum asTypes type)
/* Return size in bytes of any integer type - short, long, unsigned, etc. */
{
switch (type)
    {
    case t_int:
    case t_uint:
	return 4;
    case t_short:
    case t_ushort:
	return 2;
    case t_byte:
    case t_ubyte:
	return 1;
    case t_off:
	return 8;
    default:
        errAbort("Unexpected error in  asTypesIntSize: expecting integer type.  Got %d.", type);
	return 0; // happy compiler, never gets here
    }
}

boolean asTypesIsUnsigned(enum asTypes type)
/* Return TRUE if it's any integer type - short, long, unsigned, etc. */
{
switch (type)
    {
    case t_uint:
    case t_ushort:
    case t_ubyte:
       return TRUE;
    default:
       return FALSE;
    }
}

boolean asTypesIsInt(enum asTypes type)
/* Return TRUE if it's any integer type - short, long, unsigned, etc. */
{
switch (type)
    {
    case t_int:
    case t_uint:
    case t_short:
    case t_ushort:
    case t_byte:
    case t_ubyte:
    case t_off:
       return TRUE;
    default:
       return FALSE;
    }
}

boolean asTypesIsFloating(enum asTypes type)
/* Return TRUE if it's any floating point type - float or double. */
{
switch (type)
    {
    case t_float:
    case t_double:
       return TRUE;
    default:
       return FALSE;
    }
}

static struct asObject *asParseLineFile(struct lineFile *lf)
/* Parse open line file.  Closes lf as a side effect. */
{
struct tokenizer *tkz = tokenizerOnLineFile(lf);
struct asObject *objList = asParseTokens(tkz);
tokenizerFree(&tkz);
return objList;
}


void asColumnFree(struct asColumn **pAs)
/* free a single asColumn */
{
struct asColumn *as = *pAs;
if (as != NULL)
    {
    freeMem(as->name);
    freeMem(as->comment);
    freez(pAs);
    }
}


void asColumnFreeList(struct asColumn **pList)
/* free a list of asColumn */
{
struct asColumn *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    asColumnFree(&el);
    }
*pList = NULL;
}

void asObjectFree(struct asObject **pAs)
/* free a single asObject */
{
struct asObject *as = *pAs;
if (as != NULL)
    {
    freeMem(as->name);
    freeMem(as->comment);
    asColumnFreeList(&as->columnList);
    freez(pAs);
    }
}


void asObjectFreeList(struct asObject **pList)
/* free a list of asObject */
{
struct asObject *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    asObjectFree(&el);
    }
*pList = NULL;
}

struct asObject *asParseFile(char *fileName)
/* Parse autoSql .as file. */
{
return asParseLineFile(lineFileOpen(fileName, TRUE));
}


struct asObject *asParseText(char *text)
/* Parse autoSql from text (as opposed to file). */
{
char *dupe = cloneString(text);
struct lineFile *lf = lineFileOnString("text", TRUE, dupe);
struct asObject *objList = asParseLineFile(lf);
freez(&dupe);
return objList;
}

struct asColumn *asColumnFind(struct asObject *asObj, char *name)
// Return named column.
{
struct asColumn *asCol = NULL;
if (asObj!= NULL)
    {
    for (asCol = asObj->columnList; asCol != NULL; asCol = asCol->next)
        if (sameString(asCol->name, name))
             break;
    }
return asCol;
}

int asColumnFindIx(struct asColumn *list, char *name)
/* Return index of first element of asColumn list that matches name.
 * Return -1 if not found. */
{
struct asColumn *ac;
int ix = 0;
for (ac = list; ac != NULL; ac = ac->next, ix++)
    if (sameString(name, ac->name))
        return ix;
return -1;
}

boolean asCompareObjs(char *name1, struct asObject *as1, char *name2, struct asObject *as2, int numColumnsToCheck,
 int *retNumColumnsSame, boolean abortOnDifference)
/* Compare as-objects as1 and as2 making sure several important fields show they are the same name and type.
 * If difference found, print it to stderr.  If abortOnDifference, errAbort.
 * Othewise, return TRUE if the objects columns match through the first numColumnsToCheck fields. 
 * If retNumColumnsSame is not NULL, then it will be set to the number of contiguous matching columns. */
{
boolean differencesFound = FALSE;
struct asColumn *col1 = as1->columnList, *col2 = as2->columnList;
int checkCount = 0;
int verboseLevel = 2;
if (abortOnDifference)
    verboseLevel = 1;
if (as1->isTable != as2->isTable)
    {
    verbose(verboseLevel,"isTable does not match: %s=[%d]  %s=[%d]", name1, as1->isTable, name2, as2->isTable);
    differencesFound = TRUE;
    }
else if (as1->isSimple != as2->isSimple)
    {
    verbose(verboseLevel,"isSimple does not match: %s=[%d]  %s=[%d]", name1, as1->isSimple, name2, as2->isSimple);
    differencesFound = TRUE;
    }
else
    {
    if (!as1->isTable)
	{
	errAbort("asCompareObjLists only supports Table .as objects at this time.");
	}
    for (col1 = as1->columnList, col2 = as2->columnList; 
	 col1 != NULL && col2 != NULL && checkCount < numColumnsToCheck; 
	 col1 = col1->next, col2 = col2->next, ++checkCount)
	{
	if (!sameOk(col1->name, col2->name))
	    {
	    verbose(verboseLevel,"column #%d names do not match: %s=[%s]  %s=[%s]\n"
		, checkCount+1, name1, col1->name, name2, col2->name);
	    differencesFound = TRUE;
	    break;
	    }
	else if (col1->isSizeLink != col2->isSizeLink)
	    {
	    verbose(verboseLevel,"column #%d isSizeLink do not match: %s=[%d]  %s=[%d]\n"
		, checkCount+1, name1, col1->isSizeLink, name2, col2->isSizeLink);
	    differencesFound = TRUE;
	    break;
	    }
	else if (col1->isList != col2->isList)
	    {
	    verbose(verboseLevel,"column #%d isList do not match: %s=[%d]  %s=[%d]\n"
		, checkCount+1, name1, col1->isList, name2, col2->isList);
	    differencesFound = TRUE;
	    break;
	    }
	else if (col1->isArray != col2->isArray)
	    {
	    verbose(verboseLevel,"column #%d isArray do not match: %s=[%d]  %s=[%d]\n"
		, checkCount+1, name1, col1->isArray, name2, col2->isArray);
	    differencesFound = TRUE;
	    break;
	    }
	else if (!sameOk(col1->lowType->name, col2->lowType->name))
	    {
	    verbose(verboseLevel,"column #%d type names do not match: %s=[%s]  %s=[%s]\n"
		, checkCount+1, name1, col1->lowType->name, name2, col2->lowType->name);
	    differencesFound = TRUE;
	    break;
	    }
	else if (col1->fixedSize != col2->fixedSize)
	    {
	    verbose(verboseLevel,"column #%d fixedSize do not match: %s=[%d]  %s=[%d]\n"
		, checkCount+1, name1, col1->fixedSize, name2, col2->fixedSize);
	    differencesFound = TRUE;
	    break;
	    }
	else if (!sameOk(col1->linkedSizeName, col2->linkedSizeName))
	    {
	    verbose(verboseLevel,"column #%d linkedSizeName do not match: %s=[%s]  %s=[%s]\n"
		, checkCount+1, name1, col1->linkedSizeName, name2, col2->linkedSizeName);
	    differencesFound = TRUE;
	    break;
	    }
	}
    if (!differencesFound && checkCount < numColumnsToCheck)
	errAbort("Unexpected error in asCompareObjLists: asked to compare %d columns in %s and %s, but only found %d in one or both asObjects."
	    , numColumnsToCheck, name1, name2, checkCount);
    }
if (differencesFound)
    {
    if (abortOnDifference)
    	errAbort("asObjects differ.");
    else
    	verbose(verboseLevel,"asObjects differ. Matching field count=%d\n", checkCount);
    }
if (retNumColumnsSame)
    *retNumColumnsSame = checkCount;
return (!differencesFound);
}

boolean asColumnNamesMatchFirstN(struct asObject *as1, struct asObject *as2, int n)
/* Compare only the column names of as1 and as2, not types because if an asObj has been
 * created from sql type info, longblobs are cast to lstrings but in the proper autoSql
 * might be lists instead (e.g. longblob in sql, uint exonStarts[exonCount] in autoSql. */
{
struct asColumn *col1 = as1->columnList, *col2 = as2->columnList;
int checkCount = 0;
for (col1 = as1->columnList, col2 = as2->columnList;
     col1 != NULL && col2 != NULL && checkCount < n;
     col1 = col1->next, col2 = col2->next, ++checkCount)
    {
    if (!sameOk(col1->name, col2->name))
	return FALSE;
    }
return TRUE;
}
