/* Tables - the data structures for holding all the info about
 * a table, it's fields, and it's relationships to other tables. */
/* This file is copyright 2005 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "hash.h"
#include "tables.h"
#include "dtdParse.h"

struct table *tableNew(char *name, struct elStat *elStat,
	struct dtdElement *dtdElement)
/* Create a new table structure. */
{
struct table *table;
AllocVar(table);
table->name = cloneString(name);
table->elStat = elStat;
table->dtdElement = dtdElement;
table->fieldHash = hashNew(8);
table->fieldMixedHash = hashNew(8);
table->uniqHash = hashNew(17);
return table;
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

struct field *addFieldToTable(struct table *table, 
	char *name, char *mixedCaseName,
	struct attStat *att, boolean isMadeUpKey, boolean atTail, 
	boolean isString, char *textFieldName)
/* Add field to end of table.  Use proposedName if it's unique,
 * otherwise proposed name with some numerical suffix. */
{
struct field *field;

AllocVar(field);
field->name = cloneString(name);
field->mixedCaseName = cloneString(mixedCaseName);
field->table = table;
field->attStat = att;
field->isMadeUpKey = isMadeUpKey;
field->isString = isString;
if (!isMadeUpKey)
    {
    field->dtdAttribute = findDtdAttribute(table->dtdElement, name);
    if (field->dtdAttribute == NULL)
        {
	if (att != NULL && !sameString(name, textFieldName))
	    errAbort("%s.%s is in .stats but not in .dtd file", 
		table->name, field->name);
	}
    }
hashAdd(table->fieldHash, name, field);
hashAdd(table->fieldMixedHash, mixedCaseName, field);
if (atTail)
    {
    slAddTail(&table->fieldList, field);
    }
else
    {
    slAddHead(&table->fieldList, field);
    }
table->fieldCount += 1;
return field;
}

