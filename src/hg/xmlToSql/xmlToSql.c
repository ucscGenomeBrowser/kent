/* xmlToSql - Convert XML dump into a fairly normalized relational database.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dtdParse.h"
#include "portable.h"
#include "elStat.h"

static char const rcsid[] = "$Id: xmlToSql.c,v 1.4 2005/11/28 22:03:22 kent Exp $";

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
    struct elStat *elStat;	/* Associated elStat structure. */
    struct dtdElement *dtdElement; /* Associated dtd element. */
    struct field *primaryKey;	/* Primary key if any. */
    boolean madeUpPrimary;	/* True if we are creating primary key. */
    int lastId;			/* Last id value if we create key. */
    struct field *parentKey;	/* If non-null, field that links to parent. */
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
}

struct table *elsIntoTables(struct elStat *elList, struct hash *dtdHash)
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
    table->dtdElement = hashFindVal(dtdHash, table->name);
    if (table->dtdElement == NULL)
        errAbort("Table %s is in .spec but not in .dtd file", table->name);
    for (att = el->attList; att != NULL; att = att->next)
        {
	AllocVar(field);
	field->name = att->name;
	if (sameString(field->name, "<text>"))
	    field->name = textField;
	field->table = table;
	field->attStat = att;
	field->dtdAttribute = findDtdAttribute(table->dtdElement, field->name);
	if (field->dtdAttribute == NULL)
	    errAbort("%s.%s is in .spec but not in .dtd file", 
	    	table->name, field->name);
	slAddTail(&table->fieldList, field);
	}
    makePrimaryKey(table);
    slAddHead(&tableList, table);
    uglyf("%s primary key is %s (%d)\n", table->name, table->primaryKey->name, table->madeUpPrimary);
    }
slReverse(&tableList);
return tableList;
}

void xmlToSql(char *xmlFileName, char *dtdFileName, char *statsFileName,
	char *outDir)
/* xmlToSql - Convert XML dump into a fairly normalized relational database. */
{
struct elStat *elStatList = elStatLoadAll(statsFileName);
struct dtdElement *dtdList;
struct hash *dtdHash;
struct table *tableList = NULL, *table;

verbose(1, "%d elements in %s\n", slCount(elStatList), statsFileName);
dtdParse(dtdFileName, globalPrefix, textField,
	&dtdList, &dtdHash);
verbose(1, "%d elements in %s\n", dtdHash->elCount, dtdFileName);
tableList = elsIntoTables(elStatList, dtdHash);
verbose(1, "%d elements in tableList\n", slCount(tableList));


makeDir(outDir);


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
