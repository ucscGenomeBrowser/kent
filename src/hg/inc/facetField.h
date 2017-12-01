#ifndef FACETFIELD_H
#define FACETFIELD_H

struct facetVal
/* Keep track of number of uses of a field value */
    {
    struct facetVal *next;   /* Next in list */
    char *val;		    /* Value, not allocated here */
    int useCount;	    /* Number of times this value used */
    };

struct facetField
/* Keeps track of number of uses and unique values of a field */
    {
    struct facetField *next;   /* Next in list */
    char *fieldName;	    /* Name of field */
    int useCount;	    /* Number of times field is used */
    struct hash *valHash;   /* Hash of tag values, facetVal valued */
    struct facetVal *valList; /* List of tag values sorted with most used first */
    };

struct facetField *facetFieldsFromSqlTable(struct sqlConnection *conn, char *table, char *fields[], int fieldCount, 
    char *nullVal, char *where);
/* Return a list of facetField, one for each field of given table */

struct facetVal *facetValMajorPlusOther(struct facetVal *list, double minRatio);
/* Return a list of only the tags that are over minRatio of total tags.
 * If there are tags that have smaller amounts than this, lump them together
 * under "other".  Returns a new list. Use slFreeList() to free the result.  */

#endif /* FACETFIELD_H */
