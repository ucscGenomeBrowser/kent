/* hgNear.h - interfaces to plug columns into hgNear.  The must important
 * thing here is the column structure. */

#ifndef HGNEAR_H
#define HGNEAR_H

#ifndef CART_H
#include "cart.h"
#endif 

#ifndef JKSQL_H
#include "jksql.h"
#endif

struct genePos
/* A gene and optionally a position. */
    {
    struct genePos *next;
    char *name;		/* Gene ID. */
    char *chrom;	/* Optional chromosome location. NULL ok. */
    int start;		/* Chromosome start. Disregarded if chrom == NULL. */
    int end;		/* End in chromosome. Disregarded if chrom == NULL. */
    };

int genePosCmpName(const void *va, const void *vb);
/* Sort function to compare two genePos by name. */

struct searchResult
/* A result from simple search - includes short and long names as well
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
   /* Data set during initializatio that is guaranteed to be in each track.  */
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

   int (*tableColumns)(struct column *col);
   /* How many html columns this uses. */

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
      /* Most columns that need any data at all use the next few fields. */
   char *table;			/* Name of associated table. */
   char *keyField;		/* GeneId field in associated table. */
   char *valField;		/* Value field in associated table. */
      /* The distance type columns like homology need this field too. */
   char *curGeneField;		/* curGeneId field in associated table.  Used by distance columns*/
      /* The expression ratio type columns use the next bunch of fields as well as
       * the table/key/val fields above. */
   char *experimentTable;	/* Experiment table in hgFixed if any. */
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
#define idVarName "near.id"         	
	/* Overrides searchVarName if it exists */
#define idPosVarName "near.idPos"      	
	/* chrN:X-Y position of id, may be empty. */
#define groupVarName "near.group"	/* Grouping scheme. */
#define getSeqVarName "near.getSeq"	/* Button to get sequence. */
#define getGenomicSeqVarName "near.getGenomicSeq"	
	/* Button to fetch genomic sequence. */
#define getSeqHowVarName "near.getSeqHow" /* How to get sequence. */
#define getSeqPageVarName "near.getSeqPage" /* Button go to getSequence page. */
#define proUpSizeVarName "near.proUpSize" /* Promoter upstream size. */
#define proDownSizeVarName "near.proDownSize" /* Promoter downstream size. */
#define proIncludeFiveOnly "near.proIncludeFiveOnly" 
	/* Include without 5' UTR? */
#define getTextVarName "near.getText"	/* Button to get as text. */
#define advSearchVarName "near.advSearch"      /* Advanced search */
#define advSearchClearVarName "near.advSearchClear" 
	/* Advanced search clear all button. */
#define advSearchBrowseVarName "near.advSearchBrowse" 
	/* Advanced search browse  button. */
#define advSearchListVarName "near.advSearchList" 
	/* Advanced search submit list. */
#define colOrderVar "near.colOrder"     /* Order of columns. */
#define defaultConfName "near.default"  /* Restore to default settings. */
#define hideAllConfName "near.hideAll"  /* Hide all columns. */
#define showAllConfName "near.showAll"  /* Show all columns. */
#define saveCurrentConfName "near.saveCurrent"  /* Save current columns. */
#define useSavedConfName "near.useSaved"  /* Use saved columns. */
#define savedColSettingsVarName "near.savedColSettings" 
	/* Variable that stores saved column settings. */
#define showAllSpliceVarName "near.showAllSplice" 
	/* Show all splice varients. */
#define expRatioColorVarName "near.expRatioColors" 
	/* Color scheme for expression ratios. */
#define resetConfName "near.reset"      /* Restore default column settings. */
#define colConfigPrefix "near.col."     
	/* Prefix for stuff set in configuration pages. */
#define advSearchPrefix "near.as."      
	/* Prefix for advanced search variables. */
#define advSearchPrefixI "near.asi."    
	/* Prefix for advanced search variables not forcing search. */
#define keyWordUploadPrefix "near.keyUp." /* Prefix for keyword uploads. */
#define keyWordPastePrefix "near.keyPaste." /* Prefix for keyword paste-ins. */
#define keyWordPastedPrefix "near.keyPasted." 
	/* Prefix for keyword paste-ins. */
#define keyWordClearPrefix "near.keyClear." /* Prefix for keyword paste-ins. */
#define keyWordClearedPrefix "near.keyCleared." 
	/* Prefix for keyword paste-ins. */

/* ---- General purpose helper routines. ---- */

int genePosCmpName(const void *va, const void *vb);
/* Sort function to compare two genePos by name. */

boolean wildMatchAny(char *word, struct slName *wildList);
/* Return TRUE if word matches any thing in wildList. */

boolean wildMatchAll(char *word, struct slName *wildList);
/* Return TRUE if word matches all things in wildList. */

boolean wildMatchList(char *word, struct slName *wildList, boolean orLogic);
/* Return TRUE if word matches things in wildList. */

/* ---- Some html helper routines. ---- */

void hvPrintf(char *format, va_list args);
/* Print out some html. */

void hPrintf(char *format, ...);
/* Print out some html. */

void makeTitle(char *title, char *helpName);
/* Make title bar. */

void selfAnchorId(struct genePos *gp);
/* Print self anchor to given id. */

void cellSelfLinkPrint(struct column *col, struct genePos *gp,
	struct sqlConnection *conn);
/* Print self and hyperlink to make this the search term. */

void selfAnchorSearch(struct genePos *gp);
/* Print self anchor to given search term. */

/* -- Other helper routines. -- */

int columnCmpPriority(const void *va, const void *vb);
/* Compare to sort columns based on priority. */

struct hash *hashColumns(struct column *colList);
/* Return a hash of columns keyed by name. */

struct column *findNamedColumn(struct column *colList, char *name);
/* Return column of given name from list or NULL if none. */

/* ---- Some helper routines for column methods. ---- */

char *columnSetting(struct column *col, char *name, char *defaultVal);
/* Return value of named setting in column, or default if it doesn't exist. */

int columnIntSetting(struct column *col, char *name, int defaultVal);
/* Return value of named integer setting or default if it doesn't exist. */

boolean columnSettingExists(struct column *col, char *name);
/* Return TRUE if setting exists in column. */

char *colVarName(struct column *col, char *prefix);
/* Return variable name prefix.col->name. This is just a static
 * variable, so don't nest these calls. */

void colButton(struct column *col, char *prefix, char *label);
/* Make a button named prefix.col->name with given label. */

struct column *colButtonPressed(struct column *colList, char *prefix);
/* See if a button named prefix.column is pressed for some
 * column, and if so return the column, else NULL. */

char *cellLookupVal(struct column *col, struct genePos *gp, struct sqlConnection *conn);
/* Get a field in a table defined by col->table, col->keyField, col->valField. */

void cellSimplePrint(struct column *col, struct genePos *gp, struct sqlConnection *conn);
/* This just prints cellSimpleVal. */

void labelSimplePrint(struct column *col);
/* This just prints cell->shortLabel. */

boolean simpleTableExists(struct column *col, struct sqlConnection *conn);
/* This returns true if col->table exists. */

void lookupSearchControls(struct column *col, struct sqlConnection *conn);
/* Print out controls for advanced search. */

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

char *advSearchNameI(struct column *col, char *varName);
/* Return name for advanced search that doesn't force search. */

void advSearchRemakeTextVar(struct column *col, char *varName, int size);
/* Make a text field of given name and size filling it in with
 * the existing value if any. */

void advSearchAnyAllMenu(struct column *col, char *varName, 
	boolean defaultAny);
/* Make a drop-down menu with value all/any. */

boolean advSearchOrLogic(struct column *col, char *varName, 
	boolean defaultOr);
/* Return TRUE if user has selected 'all' from any/all menu */

void advSearchKeyUploadButton(struct column *col);
/* Make a button for uploading keywords. */

struct column *advSearchKeyUploadPressed(struct column *colList);
/* Return column where an key upload button was pressed, or
 * NULL if none. */

void doAdvSearchKeyUpload(struct sqlConnection *conn, struct column *colList, 
    struct column *col);
/* Handle upload keyword list button press in advanced search form. */

void advSearchKeyPasteButton(struct column *col);
/* Make a button for uploading keywords. */

struct column *advSearchKeyPastePressed(struct column *colList);
/* Return column where an key upload button was pressed, or
 * NULL if none. */

void doAdvSearchKeyPaste(struct sqlConnection *conn, struct column *colList, 
    struct column *col);
/* Handle search keyword list button press in advanced search form. */

struct column *advSearchKeyPastedPressed(struct column *colList);
/* Return column where an key upload button was pressed, or
 * NULL if none. */

void doAdvSearchKeyPasted(struct sqlConnection *conn, struct column *colList, 
    struct column *col);
/* Handle search keyword list button press in advanced search form. */

void advSearchKeyClearButton(struct column *col);
/* Make a button for uploading keywords. */

struct column *advSearchKeyClearPressed(struct column *colList);
/* Return column where an key upload button was pressed, or
 * NULL if none. */

void doAdvSearchKeyClear(struct sqlConnection *conn, struct column *colList, 
    struct column *col);
/* Handle clear keyword list button press in advanced search form. */

struct genePos *weedUnlessInHash(struct genePos *inList, struct hash *hash);
/* Return input list with stuff not in hash removed. */

struct genePos *getSearchNeighbors(struct column *colList, 
	struct sqlConnection *conn);
/* Get neighbors by search. */

/* ---- Column method setters. ---- */

void columnDefaultMethods(struct column *col);
/* Set up default methods. */

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

void setupColumnKnownDetails(struct column *col, char *parameters);
/* Set up a column that links to details page for known genes. */

void setupColumnKnownName(struct column *col, char *parameters);
/* Set up a column that looks up name to display, and selects on a click. */

void setupColumnExpRatio(struct column *col, char *parameters);
/* Set up expression ration type column. */

boolean expRatioUseBlue();
/* Return TRUE if should use blue instead of red
 * in the expression ratios. */

boolean gotAdvSearch();
/* Return TRUE if advanced search variables are set. */

boolean advSearchColAnySet(struct column *col);
/* Return TRUE if any of the advanced search variables
 * for this col are set. */

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

void doAdvancedSearchBrowse(struct sqlConnection *conn, struct column *colList);
/* Put up family browser with advanced search group by. */

void doAdvancedSearchList(struct sqlConnection *conn, struct column *colList);
/* List gene names matching advanced search. */

void doConfigure(struct sqlConnection *conn, struct column *colList, 
	char *bumpVar);
/* Configuration page. */

void doDefaultConfigure(struct sqlConnection *conn, struct column *colList );
/* Do configuration starting with defaults. */

void doConfigHideAll(struct sqlConnection *conn, struct column *colList);
/* Respond to hide all button in configuration page. */

void doConfigShowAll(struct sqlConnection *conn, struct column *colList);
/* Respond to show all button in configuration page. */

void doConfigSaveCurrent(struct sqlConnection *conn, struct column *colList);
/* Respond to Save Current Settings buttin in configuration page. */

void doConfigUseSaved(struct sqlConnection *conn, struct column *colList);
/* Respond to Use Saved Settings buttin in configuration page. */

void doGetSeqPage(struct sqlConnection *conn, struct column *colList);
/* Put up the get sequence page. */

void doGetSeq(struct sqlConnection *conn, struct column *colList, 
	struct genePos *geneList, char *how);
/* Show sequence. */

void doGetGenomicSeq(struct sqlConnection *conn, struct column *colList,
	struct genePos *geneList);
/* Retrieve genomic sequence sequence according to options. */

#endif /* HGNEAR_H */

