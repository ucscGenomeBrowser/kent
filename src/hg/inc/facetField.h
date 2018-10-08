#ifndef FACETFIELD_H
#define FACETFIELD_H

#define FacetFieldLimit 20   // maximum facet values to show before displaying the See More link 

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
    boolean showAllValues;  /* When true, show all values, otherwise only show first 100 values plus See More link, defaults to false */
    };

int facetValCmpSelectCountDesc(const void *va, const void *vb);
/* Compare two facetVal so as to sort them based on selectCount with most used first.
 * If two have same count, sort alphabetically on facet val.  */

int facetValCmp(const void *va, const void *vb);
/* Compare two facetVal alphabetically by val */

void selectedListFacetValUpdate(struct facetField **pSelectedList, char *facetName, char *facetValue, char *op);
/* Add or remove by changing selected boolean */

char *linearizeFacetVals(struct facetField *selectedList);
/* Linearize selected fields vals into a string */

struct facetField *deLinearizeFacetValString(char *selectedFields);
/* Turn linearized selected fields string back into facet structures */

boolean perRowFacetFields(int fieldCount, char **row, char *nullVal, struct facetField *ffArray[]);
/* Process each row of a resultset updating use and select counts. 
 * Returns TRUE if row passes selected facet values filter and should be included in the final result set. */

struct facetField *facetFieldsFromSqlTableInit(char *fields[], int fieldCount, char *selectedFields, struct facetField *ffArray[]);
/* Initialize ffList and ffArray and selected facet values */

void facetFieldsFromSqlTableFinish(struct facetField *ffList, int (*compare )(const void *elem1,  const void *elem2));
/* Do final cleanup after passing over rows */

struct facetField *facetFieldsFromSqlTable(struct sqlConnection *conn, char *table, char *fields[], int fieldCount, 
    char *nullVal, char *where, char *selectedFields, int *pSelectedRowCount);
/* Return a list of facetField, one for each field of given table */

struct facetVal *facetValMajorPlusOther(struct facetVal *list, double minRatio);
/* Return a list of only the tags that are over minRatio of total tags.
 * If there are tags that have smaller amounts than this, lump them together
 * under "other".  Returns a new list. Use slFreeList() to free the result.  */

#endif /* FACETFIELD_H */
