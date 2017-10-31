/* tagToJson - stuff to help convert between tagStorm and json */

#include "common.h"
#include "tagToJson.h"

struct slName *ttjUniqToDotList(struct slName *fieldList, char *prefix, int prefixLen)
/* Return list of all unique prefixes in field, that is parts up to first dot */
/* Return list of all words in fields that start with optional prefix, up to next dot */
{
struct slName *subList = NULL, *fieldEl;
for (fieldEl = fieldList; fieldEl != NULL; fieldEl = fieldEl->next)
     {
     char *field = fieldEl->name;
     if (prefixLen > 0)
         {
	 if (!startsWith(prefix, field))
	     continue;
	 field += prefixLen;
	 }
     char *dotPos = strchr(field, '.');
     if (dotPos == NULL)
	dotPos = field + strlen(field);
     int prefixSize = dotPos - field;
     char prefix[prefixSize+1];
     memcpy(prefix, field, prefixSize);
     prefix[prefixSize] = 0;
     slNameStore(&subList, prefix);
     }
return subList;
}

struct ttjSubObj *ttjMakeSubObj(struct slName *fieldList, char *objName, char *prefix)
/* Make a ttjSubObj - a somewhat cryptic recursive routine.  Please see hcaTagStormToBundles.c
 * for a use case. The parameters are:
 *   fieldList - a list of all the fields including their full names with dots
 *   objName - the name of the current object working on, no dots
 *   prefix - any higher level prefix.  If you are processing the rna in assay.rna.seq
 *       the prefix would be "assay" */
{
struct ttjSubObj *obj;
AllocVar(obj);
obj->name = cloneString(emptyForNull(objName));
obj->fullName = cloneString(prefix);
verbose(3, "Making ttjSubObj %s %s\n", obj->name, obj->fullName);

/* Make a string that is prefix plus a dot */
char prefixDot[512];
if (isEmpty(prefix))
    prefixDot[0] = 0;
else
    safef(prefixDot, sizeof(prefixDot), "%s.", prefix);
int prefixDotLen = strlen(prefixDot);

struct slName *subList = ttjUniqToDotList(fieldList, prefixDot, prefixDotLen);
if (subList != NULL)
     {
     struct slName *subName;
     for (subName = subList; subName != NULL; subName = subName->next)
         {
	 char newPrefix[512];
	 char *name = subName->name;
	 safef(newPrefix, sizeof(newPrefix), "%s%s", prefixDot, name);
	 struct ttjSubObj *subObj = ttjMakeSubObj(fieldList, name, newPrefix); 
	 slAddHead(&obj->children, subObj);
	 }
     slFreeList(&subList);
     }

return obj;
}

