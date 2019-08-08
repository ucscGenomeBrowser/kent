/* fieldedTable - a table composed of untyped strings in memory.  Includes names for each
 * field. This is a handy way of storing small-to-medium tab-separated files that begin
 * with a "#list of fields" line among other things. */

#ifndef FIELDEDTABLE_H
#define FIELDEDTABLE_H

struct fieldedRow
/* An array of strings with a little extra info, can be hung on a list. */
    {
    struct fieldedRow *next;
    char **row; // Array of strings
    int id;	// In the file case this is the line of file row starts in
    };

struct fieldedTable
/* A table with a name for each field. */
    {
    struct fieldedTable *next;
    char *name;	    /* Often the file name */
    struct lm *lm;  /* All allocations done out of this memory pool. */
    int fieldCount; /* Number of fields. */
    char **fields;  /* Names of fields. */
    struct fieldedRow *rowList;  /* list of parsed out fields. */
    struct fieldedRow **cursor;  /* Pointer to where we add next item to list. */
    int rowCount;   /* Number of rows. */
    boolean startsSharp;  /* Whether first line starts with a # char */
    };

struct fieldedTable *fieldedTableNew(char *name, char **fields, int fieldCount);
/* Create a new empty fieldedTable with given name, often a file name. */

void fieldedTableFree(struct fieldedTable **pTable);
/* Free up memory resources associated with table. */

struct fieldedRow *fieldedTableAdd(struct fieldedTable *table,  char **row, int rowSize, int id);
/* Create a new row and add it to table.  Return row. */

struct fieldedRow *fieldedTableAddHead(struct fieldedTable *table, char **row, int rowSize, int id);
/* Create a new row and add it to start of table.  Return row. */

struct fieldedTable *fieldedTableFromTabFile(char *fileName, char *url, char *requiredFields[], int requiredCount);
/* Read table from tab-separated file with a #header line that defines the fields.  Ensures
 * all requiredFields (if any) are present.  The url is just used for error reporting and 
 * should be the same as fileName for most purposes.  This is used by edwSubmit though which
 * first copies to a local file, and we want to report errors from the url. */

void fieldedTableToTabFile(struct fieldedTable *table, char *fileName);
/* Write out a fielded table back to file */

boolean fieldedTableColumnIsNumeric(struct fieldedTable *table, int fieldIx);
/* Return TRUE if field has numeric values wherever non-null */

int fieldedTableMaxColChars(struct fieldedTable *table, int colIx);
/* Calculate the maximum number of characters in a cell for a column */

void fieldedTableSortOnField(struct fieldedTable *table, char *field, boolean doReverse);
/* Sort on field.  Distinguishes between numerical and text fields appropriately.  */

int fieldedTableMustFindFieldIx(struct fieldedTable *table, char *field);
/* Find index of field in table's row.  Abort if field not found. */

struct hash *fieldedTableIndex(struct fieldedTable *table, char *field);
/* Return hash of fieldedRows keyed by values of given field */

struct hash *fieldedTableUniqueIndex(struct fieldedTable *table, char *field);
/* Return hash of fieldedRows keyed by values of given field, which must be unique. */

#endif /* FIELDEDTABLE_H */

