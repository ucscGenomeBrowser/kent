/* xmlToSql - Convert XML dump into a fairly normalized relational database. */
/* This file is copyright 2005 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "portable.h"
#include "xap.h"
#include "dtdParse.h"
#include "elStat.h"
#include "rename.h"
#include "tables.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "xmlToSql - Convert XML dump into a fairly normalized relational database\n"
  "   in the form of a directory full of tab-separated files and table\n"
  "   creation SQL.  You'll need to run autoDtd on the XML file first to\n"
  "   get the dtd and stats files.\n"
  "usage:\n"
  "   xmlToSql in.xml in.dtd in.stats outDir\n"
  "options:\n"
  "   -prefix=name - A name to prefix all tables with\n"
  "   -textField=name - Name to use for text field (default 'text')\n"
  "   -maxPromoteSize=N - Maximum size (default 32) for a element that\n"
  "                       just defines a string to be promoted to a field\n"
  "                       in parent table\n"
  );
}

char *globalPrefix = "";
char *textField = "text";
int maxPromoteSize=32;

static struct optionSpec options[] = {
   {"prefix", OPTION_STRING},
   {"textField", OPTION_STRING},
   {"maxPromoteSize", OPTION_STRING},
   {NULL, 0},
};

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
	    && att->maxLen <= 12 && !sameString(field->name, textField))
	    {
	    primaryKey = field;
	    break;
	    }
	}
    }

/* If still no key, we have to make one up */
if (primaryKey == NULL)
    {
    char *fieldName = renameUnique(table->fieldMixedHash, "id");
    primaryKey = addFieldToTable(table, fieldName, fieldName,
    	NULL, TRUE, FALSE, FALSE, textField);
    table->madeUpPrimary = TRUE;
    }
table->primaryKey = primaryKey;
primaryKey->isPrimaryKey = TRUE;
}

boolean attIsString(struct attStat *att)
/* Return TRUE if att is of string type. */
{
return sameString(att->type, "string");
}

struct table *elsIntoTables(struct elStat *elList, struct hash *dtdHash)
/* Create table and field data structures from element/attribute
 * data structures. */
{
struct elStat *el;
struct attStat *att;
struct table *tableList = NULL, *table;
struct field *field = NULL;

for (el = elList; el != NULL; el = el->next)
    {
    struct dtdElement *dtdElement = hashFindVal(dtdHash, el->name);
    int attCount = 0;
    if (dtdElement == NULL)
        errAbort("Element %s is in .stats but not in .dtd file", el->name);
    table = tableNew(dtdElement->mixedCaseName, el, dtdElement);
    for (att = el->attList; att != NULL; att = att->next)
        {
	char *name = att->name;
	char *mixedName = NULL;
	if (sameString(name, "<text>"))
	    name = mixedName = textField;
	else
	    {
	    struct dtdAttribute *dtdAtt = findDtdAttribute(dtdElement, name);
	    if (dtdAtt == NULL)
		errAbort("Element %s attribute %s is in .stats but not in .dtd file", 
			el->name, name);
	    mixedName = dtdAtt->mixedCaseName;
	    }
	field = addFieldToTable(table, name, mixedName, att, 
		FALSE, TRUE, attIsString(att), textField);
	attCount += 1;
	}
    /* If the table is real simple we'll just promote it */
    if (attCount == 1 && sameString(field->name, textField) 
      && (field->attStat->maxLen < maxPromoteSize || !field->isString))
	{
        table->promoted = TRUE;
	table->primaryKey = field;
	}
    else
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
    table = hashFindVal(tableHash, dtdEl->mixedCaseName);
    if (table == NULL)
        errAbort("Table %s is in .dtd but not .stat file", dtdEl->name);

    /* Loop through dtd's children and add counts. */
    for (child = dtdEl->children; child != NULL; child = child->next)
        {
	table = hashMustFindVal(tableHash, child->el->mixedCaseName);
	table->usesAsChild += 1;
	}
    }
}

static struct hash *rUniqParentLinkHash;

void rAddParentKeys(struct dtdElement *parent, struct dtdElChild *elAsChild,
	struct hash *tableHash, struct table **pTableList, int level)
/* Recursively add parentKeys. */
{
struct dtdElement *element = elAsChild->el;
struct dtdElChild *child;
struct table *table = hashMustFindVal(tableHash, element->mixedCaseName);
struct table *parentTable = hashMustFindVal(tableHash, parent->mixedCaseName);
struct field *field;
char linkUniqName[256];
int i;
static struct dtdElChild *parentStack[256];

if (level >= ArraySize(parentStack))
    errAbort("Recursion too deep in rAddParentKeys");
parentStack[level] = elAsChild;

for (i=level-1; i>= 0; i -= 1)
    if (elAsChild == parentStack[i])
        return;	/* Avoid cycling on self. */

/* Add new field in parent. */
safef(linkUniqName, sizeof(linkUniqName), "%s.%s", 
	parentTable->name, table->name);
if (!hashLookup(rUniqParentLinkHash, linkUniqName))
    {
    hashAdd(rUniqParentLinkHash, linkUniqName, NULL);
    verbose(3, "Linking %s to parent %s\n", table->name, parentTable->name);
    if (elAsChild->copyCode == '1' || elAsChild->copyCode == '?')
	{
	struct fieldRef *ref;
	char *fieldName = renameUnique(parentTable->fieldHash, element->name);
	char *fieldMixedName = renameUnique(parentTable->fieldMixedHash, 
		element->mixedCaseName);
	field = addFieldToTable(parentTable, 
		    fieldName, fieldMixedName,
		    table->primaryKey->attStat, TRUE, TRUE,
		    table->primaryKey->isString, textField);
	AllocVar(ref);
	ref->field = field;
	slAddHead(&table->parentKeys, ref);
	}
    else
	{
	/* Need to handle association here. */
	struct table *assocTable;
	struct assocRef *ref;
	char joinedName[256];
	char *assocTableName;
	int upperAt;
	safef(joinedName, sizeof(joinedName), "%sTo%s", parentTable->name,
	  table->name);
	upperAt = strlen(parentTable->name) + 2;
	joinedName[upperAt] = toupper(joinedName[upperAt]);
	assocTableName = renameUnique(tableHash, joinedName);
	hashAdd(tableHash, assocTableName, NULL);
	assocTable = tableNew(assocTableName, NULL, NULL);
	assocTable->isAssoc = TRUE;
	addFieldToTable(assocTable, parentTable->name, parentTable->name,
	    parentTable->primaryKey->attStat, TRUE, TRUE, 
	    parentTable->primaryKey->isString, textField);
	addFieldToTable(assocTable, table->name, table->name,
	    table->primaryKey->attStat, TRUE, TRUE, 
	    table->primaryKey->isString, textField);
	slAddHead(pTableList, assocTable);
	AllocVar(ref);
	ref->assoc = assocTable;
	ref->parent = parentTable;
	slAddHead(&table->parentAssocs, ref);
	}
    table->linkedParents = TRUE;
    }
else
    verbose(3, "skipping link %s to parent %s\n", table->name, parentTable->name);
for (child = element->children;  child != NULL; child = child->next)
    rAddParentKeys(element, child, tableHash, pTableList, level+1);
}

void addParentKeys(struct dtdElement *dtdRoot, struct hash *tableHash,
	struct table **pTableList)
/* Use dtd to guide creation of field->parentKeys  */
{
struct dtdElChild *topLevel;

rUniqParentLinkHash = newHash(0);
for (topLevel = dtdRoot->children; topLevel != NULL; topLevel = topLevel->next)
    {
    struct dtdElement *mainIterator = topLevel->el;
    struct dtdElChild *child;
    for (child = mainIterator->children; child != NULL; child = child->next)
        {
	rAddParentKeys(mainIterator, child, tableHash, pTableList, 0);
	}
    }
hashFree(&rUniqParentLinkHash);
}


void calcTablePosOfFields(struct table *tableList)
/* Go through all tables and assign field positions. */
{
struct table *table;
for (table = tableList; table != NULL; table = table->next)
    {
    int fieldIx = 0;
    struct field *field;
    for (field = table->fieldList; field != NULL; field = field->next)
        {
	field->tablePos = fieldIx;
	fieldIx += 1;
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

struct dyString *contentBuf[2*1024];
struct dyString **contentStack = contentBuf + ArraySize(contentBuf);

void *startHandler(struct xap *xap, char *tagName, char **atts)
/* Called at the start of a tag after attributes are parsed. */
{
struct table *table = hashFindVal(xmlTableHash, tagName);
struct field *field;
int i;

contentStack -= table->fieldCount;
if (contentStack < contentBuf)
    errAbort("Content nesting too deep, aborting from stack overflow.");
for (i=0; i<table->fieldCount; ++i)
    {
    struct dyString *dy;
    if ((dy = contentStack[i]) == NULL)
        contentStack[i] = dyStringNew(256);
    else
        dyStringClear(dy);
    }

if (table == NULL)
    errAbort("Tag %s is in xml file but not dtd file", tagName);

for (i=0; atts[i] != NULL; i += 2)
    {
    char *name = atts[i], *val = atts[i+1];
    field = hashFindVal(table->fieldHash, name);
    if (field == NULL)
        errAbort("Attribute %s of tag %s not in dtd", name, tagName);
    dyStringAppendEscapedForTabFile(contentStack[field->tablePos], val);
    }
return table;
}

struct assoc
/* List of associations we can't write until we know
 * our key. */
    {
    struct assoc *next;
    FILE *f;		/* File to write to. */
    char *childKey;	/* Key in child (allocated here). */
    };

struct assoc *assocNew(FILE *f, char *childKey)
/* Create new association. */
{
struct assoc *assoc;
AllocVar(assoc);
assoc->f = f;
assoc->childKey = cloneString(childKey);
return assoc;
}

void assocFreeList(struct assoc **pList)
/* Free up list of associations. */
{
struct assoc *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    freeMem(el->childKey);
    freeMem(el);
    }
*pList = NULL;
}

void endHandler(struct xap *xap, char *name)
/* Called at end of a tag */
{
struct table *table = xap->stack->object;
struct table *parentTable = xap->stack[1].object;
struct field *field;
struct fieldRef *fieldRef;
struct assocRef *assocRef;
char *text = skipLeadingSpaces(xap->stack->text->string);
char *primaryKeyVal = NULL;
struct assoc *assoc;
static struct dyString *uniq = NULL;

if (table->promoted)	/* Simple case - copy text to parent table. */
    {
    for (fieldRef = table->parentKeys; fieldRef != NULL; fieldRef = fieldRef->next)
	{
	field = fieldRef->field;
	if (field->table == parentTable)
	    {
	    struct dyString **parentContent = contentStack + table->fieldCount;
	    struct dyString *dy = parentContent[field->tablePos];
	    if (!field->isString && text[0] == 0)
	        text = "0";
	    dyStringAppend(dy, text);
	    break;
	    }
	}
    }
else
    {
    if (text[0] != 0)
	{
	field = hashFindVal(table->fieldHash, textField);
	if (field == NULL)
	    errAbort("No text for %s expected in dtd", table->name);
	dyStringAppendEscapedForTabFile(contentStack[field->tablePos], text);
	}

    /* Construct uniq string from fields, etc. */
    if (uniq == NULL)
	uniq = dyStringNew(0);
    else
	dyStringClear(uniq);
    for (field = table->fieldList; field != NULL; field = field->next)
	{
	if (!(field->isPrimaryKey  && field->isMadeUpKey))
	    {
	    struct dyString *dy = contentStack[field->tablePos];
	    if (dy->stringSize == 0 && !field->isString)
		dyStringAppendC(dy, '0');
	    dyStringAppendN(uniq, dy->string, dy->stringSize);
	    dyStringAppendC(uniq, '\t');
	    }
	}
    for (assoc = table->assocList; assoc != NULL; assoc = assoc->next)
	{
	dyStringPrintf(uniq, "%p\t%s\t", assoc->f, assoc->childKey);
	}

    primaryKeyVal = hashFindVal(table->uniqHash, uniq->string);
    if (primaryKeyVal == NULL)
	{
	struct dyString *priDy = contentStack[table->primaryKey->tablePos];
	if (table->madeUpPrimary)
	    {
	    table->lastId += 1;
	    dyStringPrintf(priDy, "%d", table->lastId);
	    }
	primaryKeyVal = priDy->string;
	for (field = table->fieldList; field != NULL; field = field->next)
	    {
	    struct dyString *dy = contentStack[field->tablePos];
	    fprintf(table->tabFile, "%s", dy->string);
	    if (field->next != NULL)
	       fprintf(table->tabFile, "\t");
	    }
	fprintf(table->tabFile, "\n");
	hashAdd(table->uniqHash, uniq->string, cloneString(primaryKeyVal));
	}
    for (fieldRef = table->parentKeys; fieldRef != NULL; fieldRef = fieldRef->next)
	{
	field = fieldRef->field;
	if (field->table == parentTable)
	    {
	    struct dyString **parentContent = contentStack + table->fieldCount;
	    struct dyString *dy = parentContent[field->tablePos];
	    dyStringAppend(dy, primaryKeyVal);
	    break;
	    }
	}

    for (assocRef = table->parentAssocs; assocRef != NULL; 
	    assocRef = assocRef->next)
	{
	if (assocRef->parent == parentTable)
	    {
	    assoc = assocNew(assocRef->assoc->tabFile,
		primaryKeyVal);
	    slAddHead(&parentTable->assocList, assoc);
	    }
	}

    slReverse(&table->assocList);
    for (assoc = table->assocList; assoc != NULL; assoc = assoc->next)
	fprintf(assoc->f, "%s\t%s\n", primaryKeyVal, assoc->childKey);
    assocFreeList(&table->assocList);
    }
contentStack += table->fieldCount;
}

void printType(FILE *f, struct attStat *att)
/* Print out a good SQL type for attribute */
{
char *type = att->type;
int len = att->maxLen;
if (sameString(type, "int"))
    {
    if (len <= 1)
        fprintf(f, "tinyint");
    else if (len <= 3)
        fprintf(f, "smallint");
    else if (len <= 9)
        fprintf(f, "int");
    else
        fprintf(f, "bigint");
    }
else if (sameString(type, "string"))
    {
    if (len <= 64)
        fprintf(f, "varchar(255)");
    else
        fprintf(f, "longtext");
    }
else
    fprintf(f, "%s", type);
}

void writeCreateSql(char *fileName, struct table *table)
/* Write out table definition. */
{
FILE *f = mustOpen(fileName, "w");
struct field *field;

fprintf(f, "CREATE TABLE %s (\n", table->name);
for (field = table->fieldList; field != NULL; field = field->next)
    {
    struct attStat *att = field->attStat;
    fprintf(f, "    %s ", field->mixedCaseName);
    if (att == NULL)
        fprintf(f, "int");
    else
	printType(f, att);
    fprintf(f, " not null,");
    fprintf(f, "\n");
    }
if (table->isAssoc)
    {
    for (field = table->fieldList; field != NULL; field = field->next)
	{
	if (field->isString)
	    fprintf(f, "    INDEX(%s(12))", field->mixedCaseName);
	else
	    fprintf(f, "    INDEX(%s)", field->mixedCaseName);
	if (field->next != NULL)
	    fprintf(f, ",");
	fprintf(f, "\n");
	}
    }
else
    {
    struct field *primaryKey = table->primaryKey;
    if (primaryKey->isString)
	fprintf(f, "    PRIMARY KEY(%s(12))\n", table->primaryKey->mixedCaseName);
    else
	fprintf(f, "    PRIMARY KEY(%s)\n", table->primaryKey->mixedCaseName);
    }

fprintf(f, ");\n");

carefulClose(&f);
}

void dtdRenameMixedCase(struct dtdElement *dtdList)
/* Rename mixed case names in dtd to avoid conflicts with
 * C and SQL.  Likely will migrate this into dtdParse soon. */
{
struct hash *elHash = newHash(0);
struct dtdElement *el;
renameAddSqlWords(elHash);
renameAddCWords(elHash);

/* First rename tables if need be. */
for (el = dtdList; el != NULL; el = el->next)
    {
    el->mixedCaseName = renameUnique(elHash, el->mixedCaseName);
    hashAdd(elHash, el->mixedCaseName, NULL);
    }

/* Now rename fields in tables if need be. */
for (el = dtdList; el != NULL; el = el->next)
    {
    struct dtdAttribute *att;
    struct dtdElChild *child;
    struct hash *attHash = hashNew(8);
    renameAddSqlWords(attHash);
    renameAddCWords(attHash);

    /* Don't want attributes to conflict with child elements. */
    for (child = el->children; child != NULL; child = child->next)
        hashAdd(attHash, child->el->mixedCaseName, NULL);
    for (att = el->attributes; att != NULL; att = att->next)
	{
        att->mixedCaseName = renameUnique(attHash, att->mixedCaseName);
	hashAdd(attHash, att->mixedCaseName, NULL);
	}
    hashFree(&attHash);
    }
hashFree(&elHash);
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
verbose(2, "%d elements in %s\n", slCount(elStatList), statsFileName);
dtdParse(dtdFileName, globalPrefix, textField,
	&dtdList, &dtdHash);
dtdRenameMixedCase(dtdList);
verbose(1, "%d elements in %s\n", dtdHash->elCount, dtdFileName);

/* Build up hash of dtdElements keyed by mixed name rather
 * than tag name. */
for (dtdEl = dtdList; dtdEl != NULL; dtdEl = dtdEl->next)
    hashAdd(dtdMixedHash, dtdEl->mixedCaseName, dtdEl);

/* Create list of tables that correspond to tag types. 
 * This doesn't include any association tables we create
 * to handle lists of child elements. */
tableList = elsIntoTables(elStatList, dtdHash);
verbose(2, "Made tableList\n");

/* Create hashes of the table lists - one keyed by the
 * table name, and one keyed by the tag name. */
xmlTableHash = hashNew(0);
for (table = tableList; table != NULL; table = table->next)
    {
    hashAdd(tableHash, table->name, table);
    hashAdd(xmlTableHash, table->dtdElement->name, table);
    }
verbose(2, "Made table hashes\n");

/* Find top level tag (which we won't actually output). */
countUsesAsChild(dtdList, tableHash);
verbose(2, "Past countUsesAsChild\n");
rootTable = findRootTable(tableList);
verbose(2, "Root table is %s\n", rootTable->name);

/* Add stuff to support parent-child relationships. */
addParentKeys(rootTable->dtdElement, tableHash, &tableList);
verbose(2, "Added parent keys\n");

/* Now that all the fields, both attributes and made up 
 * keys are in place, figure out index of field in table. */
calcTablePosOfFields(tableList);

/* Make output directory. */
makeDir(outDir);

/* Make table creation SQL files. */
for (table = tableList; table != NULL; table = table->next)
    {
    if (!table->promoted)
	{
	safef(outFile, sizeof(outFile), "%s/%s.sql", 
	  outDir, table->name);
	writeCreateSql(outFile, table);
	}
    }
verbose(2, "Made sql table creation files\n");

/* Set up output directory and open tab-separated files. */
for (table = tableList; table != NULL; table = table->next)
    {
    if (!table->promoted)
	{
	safef(outFile, sizeof(outFile), "%s/%s.tab", 
	  outDir, table->name);
	table->tabFile = mustOpen(outFile, "w");
	}
    }
verbose(2, "Created output files.\n");

/* Stream through XML adding to tab-separated files.. */
xapParseFile(xap, xmlFileName);
verbose(2, "Streamed through XML\n");

/* Close down files */
for (table = tableList; table != NULL; table = table->next)
    carefulClose(&table->tabFile);
verbose(2, "Closed tab files\n");

verbose(1, "All done\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
globalPrefix = optionVal("prefix", globalPrefix);
textField = optionVal("textField", textField);
maxPromoteSize = optionInt("maxPromoteSize", maxPromoteSize);
if (argc != 5)
    usage();
xmlToSql(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
