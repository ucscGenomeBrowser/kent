/* asParse - parse out an autoSql .as file. */

#include "common.h"
#include "linefile.h"
#include "tokenizer.h"
#include "asParse.h"

static char const rcsid[] = "$Id: asParse.c,v 1.5 2006/07/17 19:35:30 markd Exp $";

/* n.b. switched double/float from %f to %g to partially address losing
 * precision.  Values like 2e-12 were being rounded to 0.0 with %f.  While %g
 * doesn't match the precision of the database fields, specifying a larger
 * precision with %g resulted in numbers like 1.9999999999999999597733e-12,
 *  which might impact load time.  THis issue needs more investigation.*/
struct asTypeInfo asTypes[] = {
    {t_double,  "double",  FALSE, FALSE, "double",           "double",        "Double", "Double", "%g"},
    {t_float,   "float",   FALSE, FALSE, "float",            "float",         "Float",  "Float",  "%g"},
    {t_char,    "char",    FALSE, FALSE, "char",             "char",          "Char",   "Char",   "%c"},
    {t_int,     "int",     FALSE, FALSE, "int",              "int",           "Signed", "Signed", "%d"},
    {t_uint,    "uint",    TRUE,  FALSE, "int unsigned",     "unsigned",      "Unsigned","Unsigned", "%u"},
    {t_short,   "short",   FALSE, FALSE, "smallint",         "short",         "Short",  "Signed", "%d"},
    {t_ushort,  "ushort",  TRUE,  FALSE, "smallint unsigned","unsigned short","Ushort", "Unsigned", "%u"},
    {t_byte,    "byte",    FALSE, FALSE, "tinyint",          "signed char",   "Byte",   "Signed", "%d"},
    {t_ubyte,   "ubyte",   TRUE,  FALSE, "tinyint unsigned", "unsigned char", "Ubyte",  "Unsigned", "%u"},
    {t_off,     "bigint",  FALSE,  FALSE,"bigint",           "long long",     "LongLong", "LongLong", "%lld"},
    {t_string,  "string",  FALSE, TRUE,  "varchar(255)",     "char *",        "String", "String", "%s"},
    {t_lstring,    "lstring",    FALSE, TRUE,  "longblob",   "char *",        "String", "String", "%s"},
    {t_enum,    "enum",    FALSE, FALSE, "enum",             "!error!",       "Enum",   "Enum", NULL},
    {t_set,     "set",     FALSE, FALSE, "set",              "unsigned",      "Set",    "Set", NULL},
    {t_object,  "object",  FALSE, FALSE, "longblob",         "!error!",       "Object", "Object", NULL},
    {t_object,  "table",   FALSE, FALSE, "longblob",         "!error!",       "Object", "Object", NULL},
    {t_simple,  "simple",  FALSE, FALSE, "longblob",         "!error!",       "Simple", "Simple", NULL},
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
    if (obj->isSimple)
        tokenizerErrAbort(tkz, "simple objects can't include variable length arrays\n");
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

static struct asObject *asParseLineFile(struct lineFile *lf)
/* Parse open line file.  Closes lf as a side effect. */
{
struct tokenizer *tkz = tokenizerOnLineFile(lf);
struct asObject *objList = asParseTokens(tkz);
tokenizerFree(&tkz);
return objList;
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
return objList;
}

