/* tablesTables - this module deals with two types of tables: SQL tables in a database
 * and fieldedTable objects in memory. It has routines to do sortable, filterable web
 * displays on tables. */

#ifndef TABLESTABLES_H
#define TABLESTABLES_H

#include "fieldedTable.h"
#include "facetField.h"

void webTableBuildQuery(struct cart *cart, char *from, char *initialWhere, 
    char *varPrefix, char *fields, boolean withFilters, 
    struct dyString **retQuery, struct dyString **retWhere);
/* Construct select and where clauses in query, keeping an additional copy of where 
 * Returns the SQL query and the SQL where expression as two dyStrings (need to be freed)  */

struct fieldedTable *fieldedTableFromDbQuery(struct sqlConnection *conn, char *query);
/* Return fieldedTable from a database query */

typedef void webTableOutputWrapperType(struct fieldedTable *table, struct fieldedRow *row,
    char *field, char *val, char *shortVal, void *context);
/* If we want more than just text output we have to provide a function for a column
 * of this type.  This is responsible for rendering the tag as we want. */

void webSortableFieldedTable(struct cart *cart, struct fieldedTable *table, 
   char *returnUrl, char *varPrefix,
    int maxLenField, struct hash *tagOutputWrappers, void *wrapperContext);
/* Display all of table including a sortable label row.  The tagOutputWrappers
 * is an optional way to enrich output of specific columns of the table.  It is keyed
 * by column name and has for values functions of type webTableOutputWrapperType. */

struct fieldedTableSegment
/* Information on a segment we're processing out of something larger */
    {
    int tableSize;	// Size of larger structure
    int tableOffset;	// Where we are in larger structure
    };

void webFilteredFieldedTable(struct cart *cart, struct fieldedTable *table, 
    char *visibleFieldList, char *returnUrl, char *varPrefix,
    int maxLenField, struct hash *tagOutputWrappers, void *wrapperContext,
    boolean withFilters, char *pluralInstruction, 
    int pageSize, int facetUsualSize,
    struct fieldedTableSegment *largerContext, struct hash *suggestHash, 
    struct facetField **ffArray, char *visibleFacetList,
    void (*addFunc)(int), boolean facetMergeOk );
/* Show a fielded table that can be sorted by clicking on column labels and optionally
 * that includes a row of filter controls above the labels .
 * The maxLenField is maximum character length of field before truncation with ...
 * Pass in 0 for no max. */


void webFilteredSqlTable(struct cart *cart,    /* User set preferences here */
    struct sqlConnection *conn,		       /* Connection to database */
    char *fields, char *from, char *initialWhere,  /* Our query in three parts */
    char *returnUrl, char *varPrefix,	       /* Url to get back to us, and cart var prefix */
    int maxFieldWidth,			       /* How big do we let fields get in characters */
    struct hash *tagOutWrappers,	       /* A hash full of callbacks, one for each column */
    void *wrapperContext,		       /* Gets passed to callbacks in tagOutWrappers */
    boolean withFilters,	/* If TRUE put up filter controls under labels */
    char *pluralInstructions,   /* If non-NULL put up instructions and clear/search buttons */
    int pageSize,		/* How many items per page */
    int facetUsualSize,		/* How many items in a facet before opening */
    struct hash *suggestHash,	/* If using filter can put suggestions for categorical items here */
    char *visibleFacetList,     /* Comma separated list of fields to facet on */
    void (*addFunc)(int) );     /* Callback relevant with pluralInstructions only */
/* Turn sql query into a nice interactive table, possibly with facets.  It constructs
 * a query to the database in conn that is basically a select query broken into
 * separate clauses, construct and display an HTML table around results. Optionally table
 * may have a faceted search to the left or fields that can filter under the labels.  The table 
 * has column names that will sort the table, and optionally (if withFilters is set)
 * it will also allow field-by-field wildcard queries on a set of controls it draws above
 * the labels. 
 *    Much of the functionality rests on the call to webFilteredFieldedTable.  This function
 * does the work needed to bring in sections of potentially huge results sets into
 * the fieldedTable. */

#endif /* TABLESTABLES_H */

