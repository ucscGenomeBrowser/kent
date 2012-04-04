/* asParse - parse out an autoSql .as file. */

#include "common.h"
#include "linefile.h"
#include "tokenizer.h"
#include "dystring.h"
#include "asParse.h"


/* n.b. switched double/float from %f to %g to partially address losing
 * precision.  Values like 2e-12 were being rounded to 0.0 with %f.  While %g
 * doesn't match the precision of the database fields, specifying a larger
 * precision with %g resulted in numbers like 1.9999999999999999597733e-12,
 *  which might impact load time.  THis issue needs more investigation.*/
struct asTypeInfo asTypes[] = {
    {t_double,  "double",  FALSE, FALSE, "double",           
            "double",        "Double", "Double", "%g", "FloatField"},
    {t_float,   "float",   FALSE, FALSE, "float",            
            "float",         "Float",  "Float",  "%g", "FloatField"},
    {t_char,    "char",    FALSE, FALSE, "char",             
            "char",          "Char",   "Char",   "%c", "CharField"},
    {t_int,     "int",     FALSE, FALSE, "int",              
            "int",           "Signed", "Signed", "%d", "IntegerField"},
    {t_uint,    "uint",    TRUE,  FALSE, "int unsigned",
            "unsigned",      "Unsigned","Unsigned", "%u", "PositiveIntegerField"},
    {t_short,   "short",   FALSE, FALSE, "smallint",         
            "short",         "Short",  "Signed", "%d", "SmallIntegerField"},
    {t_ushort,  "ushort",  TRUE,  FALSE, "smallint unsigned",
            "unsigned short","Ushort", "Unsigned", "%u", "SmallPositiveIntegerField"},
    {t_byte,    "byte",    FALSE, FALSE, "tinyint",          
            "signed char",   "Byte",   "Signed", "%d", "SmallIntegerField"},
    {t_ubyte,   "ubyte",   TRUE,  FALSE, "tinyint unsigned",
            "unsigned char", "Ubyte",  "Unsigned", "%u", "SmallPositiveIntegerField"},
    {t_off,     "bigint",  FALSE,  FALSE,"bigint",           
            "long long",     "LongLong", "LongLong", "%lld", "BigIntegerField"},
    {t_string,  "string",  FALSE, TRUE,  "varchar(255)",     
            "char *",        "String", "String", "%s", "CharField"},
    {t_lstring,    "lstring",    FALSE, TRUE,  "longblob",   
            "char *",        "String", "String", "%s", "TextField"},
    {t_enum,    "enum",    FALSE, FALSE, "enum",             
            "!error!",       "Enum",   "Enum", NULL, "CharField"},
    {t_set,     "set",     FALSE, FALSE, "set",              
            "unsigned",      "Set",    "Set", NULL, NULL},
    {t_object,  "object",  FALSE, FALSE, "longblob",         
            "!error!",       "Object", "Object", NULL, "TextField"},
    {t_object,  "table",   FALSE, FALSE, "longblob",         
            "!error!",       "Object", "Object", NULL, "TextField"},
    {t_simple,  "simple",  FALSE, FALSE, "longblob",         
            "!error!",       "Simple", "Simple", NULL, "TextField"},
};

static struct asTypeInfo *findLowType(struct tokenizer *tkz)
/* Return low type info.  Squawk and die if s doesn't
 * correspond to one. */
{
char *s = tkz->string;
int i;
for (i=0; i<ArraySize(asTypes); ++i)
    {
    if (sameWord(asTypes[i].name, s))
	return &asTypes[i];
    }
tokenizerErrAbort(tkz, "Unknown type '%s'", s);
return NULL;
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
dyStringPrintf(dy, ") ");
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

static void asParseColDef(struct tokenizer *tkz, struct asObject *obj)
/* Parse a column definintion */
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

