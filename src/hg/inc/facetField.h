#ifndef FACETFIELD_H
#define FACETFIELD_H

struct facetVal
/* Keep track of number of uses of a field value */
    {
    struct facetVal *next;   /* Next in list */
    char *val;		    /* Value, not allocated here */
    int useCount;	    /* Number of times this value used */
    boolean selected;	    /* Selected value, true if selected. 
			     *  if no value has been selected = ALL are selected */
    int selectCount;	    /* Number of times this value used if selected. */
    };

struct facetField
/* Keeps track of number of uses and unique values of a field */
    {
    struct facetField *next;   /* Next in list */
    char *fieldName;	    /* Name of field */
    int useCount;	    /* Number of times field is used */
    struct hash *valHash;   /* Hash of tag values, facetVal valued */
    struct facetVal *valList; /* List of tag values sorted with most used first */
    struct facetVal *currentVal; /* Temporary value saves having to repeat hash lookup. */
    boolean allSelected;    /* When on no specific values selected, so all values are selected. default TRUE. */
    };

struct facetField *facetFieldsFromSqlTable(struct sqlConnection *conn, char *table, char *fields[], int fieldCount, 
    char *nullVal, char *where, char *selectedFields, int *pSelectedRowCount);
/* Return a list of facetField, one for each field of given table */

struct facetVal *facetValMajorPlusOther(struct facetVal *list, double minRatio);
/* Return a list of only the tags that are over minRatio of total tags.
 * If there are tags that have smaller amounts than this, lump them together
 * under "other".  Returns a new list. Use slFreeList() to free the result.  */

#endif /* FACETFIELD_H */
