/* xmlToSql - Convert XML dump into a fairly normalized relational database.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "portable.h"
#include "xap.h"
#include "dtdParse.h"
#include "elStat.h"

static char const rcsid[] = "$Id: xmlToSql.c,v 1.7 2005/11/29 00:55:25 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "xmlToSql - Convert XML dump into a fairly normalized relational database.\n"
  "usage:\n"
  "   xmlToSql in.xml in.dtd in.stats outDir\n"
  "options:\n"
  "   -prefix=name - A name to prefix all tables with\n"
  "   -textField=name - Name to use for text field (default 'text')\n"
  );
}

char *globalPrefix = "";
char *textField = "text";

static struct optionSpec options[] = {
   {"prefix", OPTION_STRING},
   {"textField", OPTION_STRING},
   {NULL, 0},
};

struct table
/* Information about one of the tables we are making. */
    {
    struct table *next;		/* Next in list. */
    char *name;			/* Name of table. */
    struct field *fieldList;	/* Information about each field. */
    struct hash *fieldHash;	/* Fields keyed by field name. */
    struct elStat *elStat;	/* Associated elStat structure. */
    struct dtdElement *dtdElement; /* Associated dtd element. */
    struct field *primaryKey;	/* Primary key if any. */
    boolean madeUpPrimary;	/* True if we are creating primary key. */
    int lastId;			/* Last id value if we create key. */
    struct table *parentAssociation;  /* Association table linking to parent. /
    struct field *parentKey;	/* If non-null, field that links to parent. */
    int usesAsChild;		/* Number of times this is a child of another
                                 * table. */
    FILE *tabFile;		/* Tab oriented file associated with table */
    struct hash *uniqHash;	/* Table to insure unique output. */
    struct dyString *uniqString;/* Key into unique hash. Also most of output */
    };

struct field
/* Information about a field. */
    {
    struct field *next;		/* Next in list. */
    char *name;			/* Name of field. */
    struct table *table;	/* Table this is part of. */
    struct field *foreignKey;	/* Field this is a foreign key for. */
    struct attStat *attStat;	/* Associated attStat structure. */
    struct dtdAttribute *dtdAttribute;	/* Associated dtd attribute. */
    boolean isMadeUpKey;	/* True if it's a made up key. */
    boolean isPrimaryKey;	/* True if it's table's primary key. */
    struct dyString *dy;	/* Current value of field during parsing. */
    };

struct field *findField(struct table *table, char *name)
/* Return named field in table,  or NULL if no such field. */
{
struct field *field;
for (field = table->fieldList; field != NULL; field = field->next)
    if (sameString(field->name, name))
	break;
return field;
}

struct dtdAttribute *findDtdAttribute(struct dtdElement *element, char *name)
/* Find named attribute in element, or NULL if no such attribute. */
{
struct dtdAttribute *dtdAtt;
for (dtdAtt = element->attributes; dtdAtt != NULL; dtdAtt = dtdAtt->next)
    {
    if (sameString(dtdAtt->name, name))
        break;
    }
return dtdAtt;
}

char *makeUpFieldName(struct table *table, char *name)
/* See if the field name is already in table.  If not
 * then return it.  Else try appending numbers to name
 * until we find one that's not in table. */
{
char buf[128];
int i;
if (findField(table, name) == NULL)
    return name;
for (i=2; ; ++i)
    {
    safef(buf, sizeof(buf), "%s%d", name, i);
    if (findField(table, buf) == NULL)
        return cloneString(buf);
    }
}

void makePrimaryKey(struct table *table)
/* Figure out primary key, using an existing field if
 * possible, otherwise making one up. */
{
struct field *primaryKey = NULL, *field;
int rowCount = table->elStat->count;

/* First scan for an int key, which we prefer. */
for (field = table->fieldList; field != NULL; field = field->next)
    {
    struct attStat *att = field->attStat;
    if (att->uniqCount == rowCount && sameString(att->type, "int"))
        {
	primaryKey = field;
	break;
	}
    }

/* If no int key, try for a short string key */
if (primaryKey == NULL)
    {
    for (field = table->fieldList; field != NULL; field = field->next)
	{
	struct attStat *att = field->attStat;
	if (att->uniqCount == rowCount && sameString(att->type, "string")
	    && att->maxLen <= 12)
	    {
	    primaryKey = field;
	    break;
	    }
	}
    }

/* If still no key, we have to make one up */
if (primaryKey == NULL)
    {
    AllocVar(primaryKey);
    primaryKey->name = makeUpFieldName(table, "id");
    primaryKey->table = table;
    primaryKey->isMadeUpKey = TRUE;
    slAddHead(&table->fieldList, primaryKey);
    table->madeUpPrimary = TRUE;
    }
table->primaryKey = primaryKey;
primaryKey->isPrimaryKey = TRUE;
}

struct table *elsIntoTables(struct elStat *elList, struct hash *dtdMixedHash)
/* Create table and field data structures from element/attribute
 * data structures. */
{
struct elStat *el;
struct attStat *att;
struct table *tableList = NULL, *table;
struct field *field;

for (el = elList; el != NULL; el = el->next)
    {
    AllocVar(table);
    table->name = el->name;
    table->elStat = el;
    table->dtdElement = hashFindVal(dtdMixedHash, table->name);
    table->fieldHash = hashNew(8);
    table->uniqHash = hashNew(17);
    table->uniqString = dyStringNew(0);
    if (table->dtdElement == NULL)
	{
        errAbort("Table %s is in .spec but not in .dtd file", table->name);
	}
    for (att = el->attList; att != NULL; att = att->next)
        {
	AllocVar(field);
	field->name = att->name;
	if (sameString(field->name, "<text>"))
	    field->name = textField;
	field->table = table;
	field->attStat = att;
	field->dtdAttribute = findDtdAttribute(table->dtdElement, field->name);
	field->dy = dyStringNew(16);
	hashAdd(table->fieldHash, field->name, field);
	if (field->dtdAttribute == NULL && !sameString(field->name, textField))
	    errAbort("%s.%s is in .spec but not in .dtd file", 
	    	table->name, field->name);
	slAddTail(&table->fieldList, field);
	}
    makePrimaryKey(table);
    slAddHead(&tableList, table);
    }
slReverse(&tableList);
return tableList;
}

void countUsesAsChild(struct dtdElement *dtdList, struct hash *tableHash)
/* Count up how many times each table is used as a child. */
{
struct dtdElement *dtdEl;
struct table *table;
for (dtdEl = dtdList; dtdEl != NULL; dtdEl = dtdEl->next)
    {
    struct dtdElChild *child;

    /* Make sure table exists in table hash - not really necessary
     * for this function, but provides a further reality check that
     * dtd and spec go together. */
    table = hashFindVal(tableHash, dtdEl->name);
    if (table == NULL)
        errAbort("Table %s is in .dtd but not .stat file", dtdEl->name);

    /* Loop through dtd's children and add counts. */
    for (child = dtdEl->children; child != NULL; child = child->next)
        {
	table = hashMustFindVal(tableHash, child->name);
	table->usesAsChild += 1;
	}
    }
}

struct table *findRootTable(struct table *tableList)
/* Find root table (looking for one that has no uses as child) */
{
struct table *root = NULL, *table;
for (table = tableList; table != NULL; table = table->next)
    {
    if (table->usesAsChild == 0)
        {
        if (root != NULL)
            errAbort(".dtd file has two root tables: %s and %s", 
	      root->name, table->name);
	root = table;
	}
    }
if (root == NULL)
    errAbort("Can't find root table in dtd.  Circular linkage?");
return root;
}

/* Globals used by the streaming parser callbacks. */
struct hash *xmlTableHash;  /* Hash of tables keyed by XML tag names */
struct table *rootTable;    /* Highest level tag. */

void dyStringAppendEscapedForTabFile(struct dyString *dy, char *string)
/* Append string to dy, escaping if need be */
{
char c, *s;
boolean needsEscape = FALSE;

s =  string;
while ((c = *s++) != 0)
    {
    switch (c)
       {
       case '\\':
       case '\t':
       case '\n':
           needsEscape = TRUE;
       }
    }

if (needsEscape)
    {
    s = string;
    while ((c = *s++) != 0)
	{
	switch (c)
	   {
	   case '\\':
	      dyStringAppendN(dy, "\\\\", 2);
	      break;
	   case '\t':
	      dyStringAppendN(dy, "\\t", 2);
	      break;
	   case '\n':
	      dyStringAppendN(dy, "\\n", 2);
	      break;
	   default:
	      dyStringAppendC(dy, c);
	      break;
	   }
	}
    }
else
    dyStringAppend(dy, string);
}

void *startHandler(struct xap *xap, char *tagName, char **atts)
/* Called at the start of a tag after attributes are parsed. */
{
struct table *table = hashFindVal(xmlTableHash, tagName);
struct field *field;
int i;
boolean uniq = FALSE;

if (table == NULL)
    errAbort("Tag %s is in xml file but not dtd file", tagName);
/* Clear all fields. */
for (field = table->fieldList; field != NULL; field = field->next)
    {
    if (!field->isMadeUpKey)
	dyStringClear(field->dy);
    }

for (i=0; atts[i] != NULL; i += 2)
    {
    char *name = atts[i], *val = atts[i+1];
    field = hashFindVal(table->fieldHash, name);
    if (field == NULL)
        errAbort("Attribute %s of tag %s not in dtd", name, tagName);
    dyStringAppendEscapedForTabFile(field->dy, val);
    }
return table;
}

void endHandler(struct xap *xap, char *name)
/* Called at end of a tag */
{
struct table *table = xap->stack->object;
struct field *field;
struct dyString *dy = table->uniqString;
char *text = skipLeadingSpaces(xap->stack->text->string);

if (text[0] != 0)
    {
    field = hashFindVal(table->fieldHash, textField);
    if (field == NULL)
        errAbort("No text for %s expected in dtd", table->name);
    dyStringAppendEscapedForTabFile(field->dy, text);
    }

/* Construct uniq string from fields, etc. */
dyStringClear(dy);
for (field = table->fieldList; field != NULL; field = field->next)
    {
    if (!field->isMadeUpKey)
	{
	dyStringAppendN(dy, field->dy->string, field->dy->stringSize);
	if (field->next != NULL)
	    dyStringAppendC(dy, '\t');
	}
    }

if (!hashLookup(table->uniqHash, dy->string))
    {
    hashAdd(table->uniqHash, dy->string, NULL);
    if (table->madeUpPrimary)
	{
	table->lastId += 1;
	fprintf(table->tabFile, "%d\t", table->lastId);
	}
    fprintf(table->tabFile, "%s\n", dy->string);
    }
}

void xmlToSql(char *xmlFileName, char *dtdFileName, char *statsFileName,
	char *outDir)
/* xmlToSql - Convert XML dump into a fairly normalized relational database. */
{
struct elStat *elStatList = NULL;
struct dtdElement *dtdList, *dtdEl;
struct hash *dtdHash, *dtdMixedHash = hashNew(0);
struct table *tableList = NULL, *table;
struct hash *tableHash = hashNew(0);
struct xap *xap = xapNew(startHandler, endHandler, xmlFileName);
char outFile[PATH_LEN];

/* Load up dtd and stats file. */
elStatList = elStatLoadAll(statsFileName);
verbose(1, "%d elements in %s\n", slCount(elStatList), statsFileName);
dtdParse(dtdFileName, globalPrefix, textField,
	&dtdList, &dtdHash);
verbose(1, "%d elements in %s\n", dtdHash->elCount, dtdFileName);

/* Build up hash of dtdElements keyed by mixed name rather
 * than tag name. */
for (dtdEl = dtdList; dtdEl != NULL; dtdEl = dtdEl->next)
    hashAdd(dtdMixedHash, dtdEl->mixedCaseName, dtdEl);

/* Create list of tables that correspond to tag types. 
 * This doesn't include any association tables we create
 * to handle lists of child elements. */
tableList = elsIntoTables(elStatList, dtdMixedHash);
uglyf("Made tableList\n");

/* Create hashes of the table lists - one keyed by the
 * table name, and one keyed by the tag name. */
xmlTableHash = hashNew(0);
for (table = tableList; table != NULL; table = table->next)
    {
    hashAdd(tableHash, table->name, table);
    hashAdd(xmlTableHash, table->dtdElement->name, table);
    }
uglyf("Made table hashes\n");

/* Count up parent/child relationships and find top level
 * tag (which we won't actually output) */
countUsesAsChild(dtdList, tableHash);
uglyf("Past countUsesAsChild\n");
rootTable = findRootTable(tableList);
uglyf("Root table is %s\n", rootTable->name);

/* Set up output directory and create tab-separated files. */
makeDir(outDir);
for (table = tableList; table != NULL; table = table->next)
    {
    safef(outFile, sizeof(outFile), "%s/%s.tab", 
      outDir, table->name);
    table->tabFile = mustOpen(outFile, "w");
    }
uglyf("Created output files.\n");

/* Stream through XML. */
xapParseFile(xap, xmlFileName);
uglyf("Streamed through XML\n");

/* Close down files */
for (table = tableList; table != NULL; table = table->next)
    carefulClose(&table->tabFile);
uglyf("All done\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
globalPrefix = optionVal("prefix", globalPrefix);
textField = optionVal("textField", textField);
if (argc != 5)
    usage();
xmlToSql(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
