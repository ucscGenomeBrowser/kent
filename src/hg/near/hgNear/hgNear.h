/* hgNear.h - interfaces to plug columns into hgNear. */

#ifndef HGNEAR_H
#define HGNEAR_H

#ifndef CART_H
#include "cart.h"
#endif 

#ifndef JKSQL_H
#include "jksql.h"
#endif

struct genePos
/* A gene and optionally a position */
    {
    struct genePos *next;
    char *name;		/* Gene ID. */
    char *chrom;	/* Optional chromosome location. NULL ok. */
    int start;		/* Chromosome start. Disregarded if chrom == NULL. */
    int end;		/* End in chromosome. Disregarded if chrom == NULL. */
    };

struct searchResult
/* A result from search - includes short and long names as well
 * as genePos. */
    {
    struct searchResult *next;
    char *shortLabel;		/* Short name (gene name) */
    char *longLabel;		/* Long name (gene description) */
    struct genePos gp;		/* Gene id. */
    };

int searchResultCmpShortLabel(const void *va, const void *vb);
/* Compare to sort based on short label. */

struct column
/* A column in the big table. The central data structure for
 * hgNear. */
   {
   /* Data set during initialization and afterwards held constant 
    * that is guaranteed to be in each track.  */
   struct column *next;		/* Next column. */
   char *name;			/* Column name, not allocated here. */
   char *shortLabel;		/* Column label. */
   char *longLabel;		/* Column description. */
   boolean on;			/* True if turned on. */
   boolean defaultOn;		/* On by default? */
   float priority;		/* Order displayed. */
   char *type;			/* Type - encodes which methods to used etc. */
   struct hash *settings;	/* Settings from ra file. */

   /* -- Methods -- */
   boolean (*exists)(struct column *col, struct sqlConnection *conn);
   /* Return TRUE if column exists in database. */

   char *(*cellVal)(struct column *col, struct genePos *gp, 
   	struct sqlConnection *conn);
   /* Get value of one cell as string.  FreeMem this when done.  Note that
    * gp->chrom may be NULL legitimately. */

   void (*cellPrint)(struct column *col, struct genePos *gp, 
   	struct sqlConnection *conn);
   /* Print one cell of this column.  Note that gp->chrom may be 
    * NULL legitimately. */

   void (*labelPrint)(struct column *col);
   /* Print the label in the label row. */

   void (*configControls)(struct column *col);
   /* Print out configuration controls. */

   struct searchResult *(*simpleSearch)(struct column *col, 
   	struct sqlConnection *conn, char *search);
   /* Return list of genes with descriptions that match search. */

   void (*searchControls)(struct column *col, struct sqlConnection *conn);
   /* Print out controls for advanced search. */

   struct genePos *(*advancedSearch)(struct column *col, struct sqlConnection *conn,
   	struct genePos *inputList);
   /* Return list of positions for advanced search. */

   /* -- Data that may be track-specific. -- */
   char *table;			/* Name of associated table. */
   char *keyField;		/* GeneId field in associated table. */
   char *valField;		/* Value field in associated table. */
   char *curGeneField;		/* curGeneId field in associated table. */
   char *expTable;		/* Experiment table in hgFixed if any. */
   char *posTable;		/* Positional (bed12) for expression experiments. */
   double expScale;		/* What to scale by to get expression val from -1 to 1. */
   int representativeCount;	/* Count of representative experiments. */
   int *representatives;	/* Array (may be null) of representatives. */
   boolean expRatioUseBlue;	/* Use blue rather than red in expRatio. */
   };

/* ---- global variables ---- */

extern struct cart *cart; /* This holds variables between clicks. */
extern char *database;	  /* Name of genome database - hg15, or the like. */
extern char *organism;	  /* Name of organism - mouse, human, etc. */
extern char *groupOn;	  /* Current grouping strategy. */
extern int displayCount;  /* Number of items to display. */

extern struct genePos *curGeneId;	  /* Identity of current gene. */

/* ---- Cart Variables ---- */

#define confVarName "near.configure"	/* Configuration button */
#define countVarName "near.count"	/* How many items to display. */
#define searchVarName "near.search"	/* Search term. */
#define idVarName "near.id"         	/* Overrides searchVarName if it exists */
#define idPosVarName "near.idPos"      	/* chrN:X-Y position of id, may be empty. */
#define groupVarName "near.group"	/* Grouping scheme. */
#define getSeqVarName "near.getSeq"	/* Button to get sequence. */
#define getTextVarName "near.getText"	/* Button to get as text. */
#define advSearchVarName "near.advSearch"      /* Advanced search */
#define advSearchClearVarName "near.advSearchClear" /* Advanced search clear all button. */
#define advSearchSubmitVarName "near.advSearchSubmit" /* Advanced search submit button. */
#define colOrderVar "near.colOrder"     /* Order of columns. */
#define defaultConfName "near.default"  /* Restore to default settings. */
#define hideAllConfName "near.hideAll"  /* Hide all columns. */
#define resetConfName "near.reset"      /* Ignore setting changes. */
#define advSearchPrefix "near.as"       /* Prefix for advanced search variables. */

/* ---- Some html helper routines. ---- */

void hvPrintf(char *format, va_list args);
/* Print out some html. */

void hPrintf(char *format, ...);
/* Print out some html. */

void makeTitle(char *title, char *helpName);
/* Make title bar. */

void selfAnchorId(struct genePos *gp);
/* Print self anchor to given id. */

void selfAnchorSearch(struct genePos *gp);
/* Print self anchor to given search term. */

/* -- Other helper routines. -- */

int columnCmpPriority(const void *va, const void *vb);
/* Compare to sort columns based on priority. */

struct hash *hashColumns(struct column *colList);
/* Return a hash of columns keyed by name. */


/* ---- Some helper routines for column methods. ---- */

char *columnSetting(struct column *column, char *name, char *defaultVal);
/* Return value of named setting in column, or default if it doesn't exist. */

char *cellLookupVal(struct column *col, struct genePos *gp, struct sqlConnection *conn);
/* Get a field in a table defined by col->table, col->keyField, col->valField. */

void cellSimplePrint(struct column *col, struct genePos *gp, struct sqlConnection *conn);
/* This just prints cellSimpleVal. */

void labelSimplePrint(struct column *col);
/* This just prints cell->shortLabel. */

static void cellSelfLinkPrint(struct column *col, struct genePos *gp,
	struct sqlConnection *conn);
/* Print self and hyperlink to make this the search term. */

boolean simpleTableExists(struct column *col, struct sqlConnection *conn);
/* This returns true if col->table exists. */

void columnDefaultMethods(struct column *col);
/* Set up default methods. */

void lookupTypeMethods(struct column *col, char *table, char *key, char *val);
/* Set up the methods for a simple lookup column. */

struct searchResult *knownGeneSearchResult(struct sqlConnection *conn, 
	char *kgID, char *alias);
/* Return a searchResult for a known gene. */

struct genePos *knownPosAll(struct sqlConnection *conn);
/* Get all positions in knownGene table. */

void fillInKnownPos(struct genePos *gp, struct sqlConnection *conn);
/* If gp->chrom is not filled in go look it up. */

char *advSearchName(struct column *col, char *varName);
/* Return variable name for advanced search. */

char *advSearchVal(struct column *col, char *varName);
/* Return value for advanced search variable.  Return NULL if it
 * doesn't exist or if it is "" */

void advSearchRemakeTextVar(struct column *col, char *varName, int size);
/* Make a text field of given name and size filling it in with
 * the existing value if any. */

struct genePos *getSearchNeighbors(struct column *colList, 
	struct sqlConnection *conn);
/* Get neighbors by search. */

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

void setupColumnExpRatio(struct column *col, char *parameters);
/* Set up expression ration type column. */

boolean gotAdvSearch();
/* Return TRUE if advanced search variables are set. */

/* ---- Create high level pages. ---- */
void displayData(struct sqlConnection *conn, struct column *colList, struct genePos *gp);
/* Display data in neighborhood of gene. */

void doSearch(struct sqlConnection *conn, struct column *colList);
/* Search.  If result is unambiguous call doMain, otherwise
 * put up a page of choices. */

void doAdvancedSearch(struct sqlConnection *conn, struct column *colList);
/* Put up advanced search page. */

void doAdvancedSearchClear(struct sqlConnection *conn, struct column *colList);
/* Clear variables in advanced search page. */

void doAdvancedSearchSubmit(struct sqlConnection *conn, struct column *colList);
/* Handle submission in advanced search page. */

void doConfigure(struct sqlConnection *conn, struct column *colList, 
	char *bumpVar);
/* Configuration page. */

void doDefaultConfigure(struct sqlConnection *conn, struct column *colList );
/* Do configuration starting with defaults. */

void doConfigHideAll(struct sqlConnection *conn, struct column *colList);
/* Respond to hide all button in configuration page. */

#endif /* HGNEAR_H */

