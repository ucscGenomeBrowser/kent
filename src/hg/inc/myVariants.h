#ifndef MYVARIANTS_H
#define MYVARIANTS_H

#include "jksql.h"
#include "asParse.h"
#include "common.h"
#include "cart.h"
#define MYVARIANTS_NUM_COLS 17 /* number of default columns before any user-added custom columns */

struct myVariants
/* An item in a myVariants type track. */
    {
    struct myVariants *next;  /* Next in singly linked list. */
    unsigned bin;    /* Bin for range index */
    char *chrom;     /* Reference sequence chromosome or scaffold */
    unsigned chromStart;    /* Start position in chromosome */
    unsigned chromEnd;      /* End position in chromosome */
    char *name;     /* Name of item - up to 16 chars */
    unsigned score; /* 0-1000.  Higher numbers are darker. */
    char strand[2]; /* + or - for strand */
    unsigned thickStart;    /* Start of thick part */
    unsigned thickEnd;      /* End position of thick part */
    unsigned itemRgb;   /* RGB 8 bits each as in bed */
    char *description;  /* Longer item description */
    char *db;           /* database name of this annotation */
    char *ref;          /* reference allele */
    char *alt;          /* alternate allele */
    char *project;      /* project name for grouping variants */
    char *mouseover;    /* short mouseover text for hover display */
    unsigned id;        /* Unique ID for item */
    struct slPair *customFields; /* user-defined custom field name/value pairs */
    };

void myVariantsStaticLoad(char **row, struct myVariants *ret);
/* Load a row from myVariants table into ret. The contents of ret will be replaced at the next call to this function. */

struct myVariants *myVariantsLoadByQuery(struct sqlConnection *conn, char *query);
/* Load all myVariants from table that satisfy the query given. Dispose of this with myVariantsFreeList(). */

void myVariantsSaveToDb(struct sqlConnection *conn, struct myVariants *el, char *tableName, int updateSize);
/* Save myVariants as a row to the table specified by tableName. */

struct myVariants *myVariantsLoad(char **row);
/* Load a myVariants from row fetched with select * from myVariants from database. Dispose of this with myVariantsFree(). */

struct myVariants *myVariantsLoadAll(char *fileName);
/* Load all myVariants from whitespace-separated file. Dispose of this with myVariantsFreeList(). */

struct myVariants *myVariantsLoadAllByChar(char *fileName, char chopper);
/* Load all myVariants from chopper separated file. Dispose of this with myVariantsFreeList(). */

#define myVariantsLoadAllByTab(a) myVariantsLoadAllByChar(a, '\t');
/* Load all myVariants from tab separated file. Dispose of this with myVariantsFreeList(). */

struct myVariants *myVariantsCommaIn(char **pS, struct myVariants *ret);
/* Create a myVariants out of a comma separated string. This will fill in ret if non-null, otherwise will return a new myVariants */

void myVariantsFree(struct myVariants **pEl);
/* Free a single dynamically allocated myVariants such as created with myVariantsLoad(). */

void myVariantsFreeList(struct myVariants **pList);
/* Free a list of dynamically allocated myVariants's */

void myVariantsOutput(struct myVariants *el, FILE *f, char sep, char lastSep);
/* Print out myVariants. Separate fields with sep. Follow last field with lastSep. */

#define myVariantsTabOut(el,f) myVariantsOutput(el,f,'\t','\n');
#define myVariantsCommaOut(el,f) myVariantsOutput(el,f,',',',');

struct asObject *myVariantsAsObj();
/* Return asObject describing fields of myVariants */

char *myVariantsGetDatabaseForUser(char *userName);
/* Hash the userName and map it to 1..31 inclusive for deciding what
 * database the table should be created in */

char *myVariantsGetTableName(char *userName);
/* Create the table for this users' myVariants */

char *myVariantsGetDbTable(char *userName);
/* Return the string db.tableName based on the userName for use in sql statements
 * without specifying the database */

char *myVariantsTableExists(char *userName);
/* See if we already have a table for this user. If so, return the name
 * of the table (in db.tableName format), else NULL */

char *myVariantsCreateTable(char *userName);
/* Return TRUE if the myVariants table for userName exists or we can create it. Return FALSE on an error */

char *myVariantsResolveDbTableForCustomTrack(char *trackName, struct cart *cart);
/* For a custom-track name of the form "myVariants_*", return the fully
 * qualified SQL table (db.tableName) holding the items.  Handles both own
 * tracks and shared tracks. Returns NULL on failure. */

void myVariantsDeleteForDb(char *userName, char *targetDb);
/* Delete the user's myVariants items for the given assembly.  No-op if the
 * table doesn't exist. */

char *myVariantsWriteCtFile(char *userName, char *targetDb, struct cart *cart);
/* Write a CT file to trash for user's myVariants in targetDb and any shared tracks
 * found in cart. Return filename or NULL if nothing to write. */

struct slName *myVariantsGetProjects(char *userName);
/* Return list of distinct non-empty project values for this user's myVariants table.
 * Caller must slFreeList the result. Returns NULL if no projects or table doesn't exist. */

struct slName *myVariantsGetCustomFields(char *userName);
/* Return list of user-added custom column names for this user's myVariants table.
 * Skips the first MYVARIANTS_NUM_COLS default columns and filters out _hidden_ prefixed columns.
 * Caller must slFreeList the result. Returns NULL if no custom fields or table doesn't exist. */

struct slName *myVariantsGetHiddenFields(char *userName);
/* Return list of hidden custom column names (with _hidden_ prefix stripped) for this user's
 * myVariants table. Caller must slFreeList the result. Returns NULL if none. */

#endif /* MYVARIANTS_H */
