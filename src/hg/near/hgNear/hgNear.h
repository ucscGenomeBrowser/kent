/* hgNear.h - interfaces to plug columns into hgNear. */

#ifndef HGNEAR_H
#define HGNEAR_H

#ifndef CART_H
#include "cart.h"
#endif 

#ifndef JKSQL_H
#include "jksql.h"
#endif

struct column
/* A column in the big table. */
   {
   /* Data set during initialization and afterwards held constant 
    * that is guaranteed to be in each track.  */
   struct column *next;		/* Next column. */
   char *name;			/* Column name, not allocated here. */
   char *shortLabel;		/* Column label. */
   char *longLabel;		/* Column description. */
   boolean on;			/* True if turned on. */
   float priority;		/* Order displayed. */
   char *type;			/* Type - encodes which methods to used etc. */
   struct hash *settings;	/* Settings from ra file. */

   /* -- Methods -- */
   boolean (*exists)(struct column *col, struct sqlConnection *conn);
   /* Return TRUE if column exists in database. */

   char *(*cellVal)(struct column *col, char *geneId, struct sqlConnection *conn);
   /* Get value of one cell as string.  FreeMem this when done. */

   void (*cellPrint)(struct column *col, char *geneId, struct sqlConnection *conn);
   /* Print one cell of this column. */

   void (*labelPrint)(struct column *col);
   /* Print the label in the label row. */

   /* -- Data that may be track-specific. -- */
   char *table;			/* Name of associated table. */
   char *keyField;		/* GeneId field in associated table. */
   char *valField;		/* Value field in associated table. */
   char *curGeneField;		/* curGeneId field in associated table. */
   };

/* ---- global variables ---- */

extern struct cart *cart; /* This holds variables between clicks. */
extern char *database;	  /* Name of genome database - hg15, or the like. */
extern char *organism;	  /* Name of organism - mouse, human, etc. */
extern char *curGeneId;	  /* Identity of current gene. */
extern char *sortOn;	  /* Current sort strategy. */
extern int displayCount;  /* Number of items to display. */

/* ---- Cart Variables ---- */

#define confVarName "near.configure"	/* Configuration button */
#define countVarName "near.count"	/* How many items to display. */
#define searchVarName "near.search"	/* Search term. */
#define geneIdVarName "near.geneId"     /* Purely cart, not cgi. Last valid geneId */
#define groupVarName "near.group"	/* Grouping scheme. */
#define colOrderVar "near.colOrder"     /* Order of columns. */
#define defaultConfName "near.default"  /* Restore to default settings. */
#define resetConfName "near.reset"      /* Ignore setting changes. */

/* ---- Some html helper routines. ---- */

void hvPrintf(char *format, va_list args);
/* Print out some html. */

void hPrintf(char *format, ...);
/* Print out some html. */

void makeTitle(char *title, char *helpName);
/* Make title bar. */

/* -- Other helper routines. -- */

struct column *getColumns(struct sqlConnection *conn);
/* Return list of columns for big table. */

int columnCmpPriority(const void *va, const void *vb);
/* Compare to sort columns based on priority. */

struct hash *hashColumns(struct column *colList);
/* Return a hash of columns keyed by name. */

void doConfigure(char *bumpVar);
/* Configuration page. */

void doDefaultConfigure();
/* Do configuration starting with defaults. */


/* ---- Some helper routines for column methods. ---- */

char *cellLookupVal(struct column *col, char *geneId, struct sqlConnection *conn);
/* Get a field in a table defined by col->table, col->keyField, col->valField. */

void cellSimplePrint(struct column *col, char *geneId, struct sqlConnection *conn);
/* This just prints cellSimpleVal. */

void labelSimplePrint(struct column *col);
/* This just prints cell->shortLabel. */

static void cellSelfLinkPrint(struct column *col, char *geneId,
	struct sqlConnection *conn);
/* Print self and hyperlink to make this the search term. */

boolean simpleTableExists(struct column *col, struct sqlConnection *conn);
/* This returns true if col->table exists. */

void columnDefaultMethods(struct column *col);
/* Set up default methods. */

void lookupTypeMethods(struct column *col, char *table, char *key, char *val);
/* Set up the methods for a simple lookup column. */

char *knownPosVal(struct column *col, char *geneId, 
	struct sqlConnection *conn);
/* Get genome position of knownPos table.  Ok to have col NULL. */

/* ---- Column method setters. ---- */

void setupColumnNum(struct column *col, char *parameters);
/* Set up column that displays index in displayed list. */

void setupColumnLookup(struct column *col, char *parameters);
/* Set up column that just looks up one field in a table
 * keyed by the geneId. */

void setupColumnAcc(struct column *col, char *parameters);
/* Set up a column that displays the geneId (accession) */

void setupColumnDistance(struct column *col, char *parameters);
/* Set up a column that looks up a field in a distance matrix
 * type table such as the expression or homology tables. */

void setupColumnKnownPos(struct column *col, char *parameters);
/* Set up column that links to genome browser based on known gene
 * position. */

void setupColumnLookupKnown(struct column *col, char *parameters);
/* Set up a column that links to details page for known genes. */

#endif /* HGNEAR_H */

