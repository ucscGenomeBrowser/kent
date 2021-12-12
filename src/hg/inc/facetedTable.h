/* facetedTable - routines to help produce a sortable table with facet selection fields.
 * This builds on top of things in tablesTables and facetField. */

#ifndef FACETEDTABLE_H
#define FACETEDTABLE_H

/* Typical usage of this module would be as so for interactive updates of facets and selection:
 *
 * struct facetedTable *facTab = facetedTableFromTable(myTable, varPrefix, "organ,cell,stage");
 * facetedTableUpdateOnClick(facTab, cart); 
 * struct facetField *selectedFf;
 * struct fieldedTable *selected = facetedTableSelect(facTab, cart, &selectedFf);
 * facetedTableWriteHtml(facTab, cart, selected, selectedFf, "shortLabel,expressionValue",
 *                       "../cgi-bin/hgSomething?hgsid=123_ABC",  32,
 *                       NULL, NULL, 7);
 *
 * For just getting a list of selected rows (by row index)
 *
 * struct facetedTable *facTab = facetedTableFromTable(myTable, varPrefix, "organ,cell,stage");
 * struct slInt *selList = facetedTableSelectOffsets(facTab, cart);
 *
*/

struct facetedTable
/* Help manage a faceted table */
    {
    struct facetedTable *next;
    char *name;		/* Name of file or database table */
    char *varPrefix;	/* Prefix used on CGI variables */
    char *facets;   /* Comma separated list of facets */
    struct fieldedTable *table;	/* Associated table-in-memory */
    struct facetField **ffArray; /* Additional info on each field of table for faceter */
    boolean mergeFacetsOk;	/* If set can merge facets */
    };

void facetedTableFree(struct facetedTable **pFt);
/* Free up resources associated with faceted table */

struct facetedTable *facetedTableFromTable(struct fieldedTable *table, 
    char *varPrefix, char *facets);
/* Construct a facetedTable around a fieldedTable */

boolean facetedTableUpdateOnClick(struct facetedTable *facTab, struct cart *cart);
/* If we got called by a click on a facet deal with that and return TRUE, else do
 * nothing and return false */

struct fieldedTable *facetedTableSelect(struct facetedTable *facTab, struct cart *cart,
    struct facetField ***retFfArray);
/* Return table containing rows of facTab->table that have passed facet selection */

void facetedTableWriteHtml(struct facetedTable *facTab, 
    struct cart *cart,		    /* User settingss and stuff */
    struct fieldedTable *selected,  /* Table that just contains rows that survive selection */
    struct facetField **selectedFf, /* Fields that survive selection */
    char *displayList,		    /* Comma separated list of fields to display in table */
    char *returnUrl,		    /* Url that takes us back to page we are writing next click */
    int maxFieldWidth,		    /* How big do we let fields get in characters */
    struct hash *tagOutWrappers,    /* A hash full of callbacks, keyed by field name */
    void *wrapperContext,	    /* Gets passed to callbacks in tagOutWrappers */
    int facetUsualSize);	    /* Ho many items in a facet before opening */
/* Write out the main HTML associated with facet selection and table. */

struct slInt *facetedTableSelectOffsets(struct facetedTable *facTab, struct cart *cart);
/* Return a list of row positions that pass faceting */

#endif /* FACETEDTABLE_H */

