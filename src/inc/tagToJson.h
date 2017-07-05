/* tagToJson - stuff to help convert between tagStorm and json */

#ifndef TAGTOJSON_H
#define TAGTOJSON_H

struct ttjSubObj
/* A subobject - may have children or not */
    {
    struct ttjSubObj *next;      // Points to next at same level
    struct ttjSubObj *children;  // Points to lower level, may be NULL.
    char *name;		      // Field name 
    char *fullName;	      // Field name with parent fields too.
    };

struct slName *ttjUniqToDotList(struct slName *fieldList, char *prefix, int prefixLen);
/* Return list of all unique prefixes in field, that is parts up to first dot */
/* Return list of all words in fields that start with optional prefix, up to next dot */

struct ttjSubObj *ttjMakeSubObj(struct slName *fieldList, char *objName, char *prefix);
/* Make a ttjSubObj - a somewhat cryptic recursive routine.  Please see hcaTagStormToBundles.c
 * for a use case. The parameters are:
 *   fieldList - a list of all the fields including their full names with dots
 *   objName - the name of the current object working on, no dots
 *   prefix - any higher level prefix.  If you are processing the rna in assay.rna.seq
 *       the prefix would be "assay" */

#endif /* TAGTOJSON_H */
