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
    float distance;	/* Something to help sort on. */
    };
#define genePosTooFar 1000000000	/* Definitely not in neighborhood. */

int genePosCmpName(const void *va, const void *vb);
/* Sort function to compare two genePos by name. */

void genePosFillFrom4(struct genePos *gp, char **row);
/* Fill in genePos from row containing ascii version of
 * name/chrom/start/end. */

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

   void (*filterControls)(struct column *col, struct sqlConnection *conn);
   /* Print out controls for advanced filter. */

   struct genePos *(*advFilter)(struct column *col, struct sqlConnection *conn,
   	struct genePos *inputList);
   /* Return list of positions for advanced filter. */

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
#define colInfoVarName "near.colInfo"	/* Display column info. */
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
#define advFilterVarName "near.advFilter"      /* Aadvanced filter */
#define advFilterClearVarName "near.advFilterClear" 
	/* Aadvanced filter clear all button. */
#define advFilterBrowseVarName "near.advFilterBrowse" 
	/* Aadvanced filter browse  button. */
#define advFilterListVarName "near.advFilterList" 
	/* Aadvanced filter submit list. */
#define colOrderVar "near.colOrder"     /* Order of columns. */
#define defaultConfName "near.default"  /* Restore to default settings. */
#define hideAllConfName "near.hideAll"  /* Hide all columns. */
#define showAllConfName "near.showAll"  /* Show all columns. */

#define colSaveSettingsPrefix "near.colUserSet."  /* Prefix for column sets. */
#define saveCurrentConfName "near.do.colUserSet.save"   /* Save column set. */
#define savedCurrentConfName "near.do.colUserSet.saved" /* Saved column set. */
#define useSavedConfName "near.do.colUserSet.used"      /* Use column set. */

#define showAllSpliceVarName "near.showAllSplice" 
	/* Show all splice varients. */
#define expRatioColorVarName "near.expRatioColors" 
	/* Color scheme for expression ratios. */
#define resetConfName "near.reset"      /* Restore default column settings. */
#define colConfigPrefix "near.col."     
	/* Prefix for stuff set in configuration pages. */
#define advFilterPrefix "near.as."      
	/* Prefix for advanced filter variables. */
#define advFilterPrefixI "near.asi."    
	/* Prefix for advanced filter variables not forcing search. */
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

void colInfoAnchor(struct column *col);
/* Print anchor tag that leads to column info page. */

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

void lookupAdvFilterControls(struct column *col, struct sqlConnection *conn);
/* Print out controls for advanced filter. */

void lookupTypeMethods(struct column *col, char *table, char *key, char *val);
/* Set up the methods for a simple lookup column. */

struct searchResult *knownGeneSearchResult(struct sqlConnection *conn, 
	char *kgID, char *alias);
/* Return a searchResult for a known gene. */

struct genePos *knownPosAll(struct sqlConnection *conn);
/* Get all positions in knownGene table. */

#ifdef OLD 
struct hash *knownCannonicalHash(struct sqlConnection *conn);
/* Get all cannonical gene names in hash. */
#endif /* OLD */

void fillInKnownPos(struct genePos *gp, struct sqlConnection *conn);
/* If gp->chrom is not filled in go look it up. */

struct genePos *advFilterResults(struct column *colList, 
	struct sqlConnection *conn);
/* Get list of genes that pass all advanced filter filters.  
 * If no filters are on this returns all genes. */

char *advFilterName(struct column *col, char *varName);
/* Return variable name for advanced filter. */

char *advFilterVal(struct column *col, char *varName);
/* Return value for advanced filter variable.  Return NULL if it
 * doesn't exist or if it is "" */

char *advFilterNameI(struct column *col, char *varName);
/* Return name for advanced filter that doesn't force search. */

void advFilterRemakeTextVar(struct column *col, char *varName, int size);
/* Make a text field of given name and size filling it in with
 * the existing value if any. */

void advFilterAnyAllMenu(struct column *col, char *varName, 
	boolean defaultAny);
/* Make a drop-down menu with value all/any. */

boolean advFilterOrLogic(struct column *col, char *varName, 
	boolean defaultOr);
/* Return TRUE if user has selected 'all' from any/all menu */

void advFilterKeyUploadButton(struct column *col);
/* Make a button for uploading keywords. */

struct column *advFilterKeyUploadPressed(struct column *colList);
/* Return column where an key upload button was pressed, or
 * NULL if none. */

void doAdvFilterKeyUpload(struct sqlConnection *conn, struct column *colList, 
    struct column *col);
/* Handle upload keyword list button press in advanced filter form. */

void advFilterKeyPasteButton(struct column *col);
/* Make a button for uploading keywords. */

struct column *advFilterKeyPastePressed(struct column *colList);
/* Return column where an key upload button was pressed, or
 * NULL if none. */

void doAdvFilterKeyPaste(struct sqlConnection *conn, struct column *colList, 
    struct column *col);
/* Handle search keyword list button press in advanced filter form. */

struct column *advFilterKeyPastedPressed(struct column *colList);
/* Return column where an key upload button was pressed, or
 * NULL if none. */

void doAdvFilterKeyPasted(struct sqlConnection *conn, struct column *colList, 
    struct column *col);
/* Handle search keyword list button press in advanced filter form. */

void advFilterKeyClearButton(struct column *col);
/* Make a button for uploading keywords. */

struct column *advFilterKeyClearPressed(struct column *colList);
/* Return column where an key upload button was pressed, or
 * NULL if none. */

void doAdvFilterKeyClear(struct sqlConnection *conn, struct column *colList, 
    struct column *col);
/* Handle clear keyword list button press in advanced filter form. */

struct genePos *weedUnlessInHash(struct genePos *inList, struct hash *hash);
/* Return input list with stuff not in hash removed. */

#ifdef OLD
struct genePos *getSearchNeighbors(struct column *colList, 
	struct sqlConnection *conn, struct hash *goodHash, int maxCount);
/* Get neighbors by search. */
#endif /* OLD */

void gifLabelVerticalText(char *fileName, char **labels, int labelCount,
	int height);
/* Make a gif file with given labels. */

int gifLabelMaxWidth(char **labels, int labelCount);
/* Return maximum pixel width of labels.  It's ok to have
 * NULLs in labels array. */

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

void setupColumnSwissProt(struct column *col, char *parameters);
/* Set up a column that protein name and links to SwissProt. */

void setupColumnExpRatio(struct column *col, char *parameters);
/* Set up expression ration type column. */

boolean gotAdvFilter();
/* Return TRUE if advanced filter variables are set. */

boolean advFilterColAnySet(struct column *col);
/* Return TRUE if any of the advanced filter variables
 * for this col are set. */

/* ---- Get config options ---- */
boolean showOnlyCannonical();
/* Return TRUE if we only show cannonical splicing varients. */

struct hash *cannonicalHash();
/* Get cannonicalHash if necessary, otherwise return NULL. */

boolean expRatioUseBlue();
/* Return TRUE if should use blue instead of red
 * in the expression ratios. */

/* ---- Create high level pages. ---- */
void displayData(struct sqlConnection *conn, struct column *colList, struct genePos *gp);
/* Display data in neighborhood of gene. */

void doSearch(struct sqlConnection *conn, struct column *colList);
/* Search.  If result is unambiguous call doMain, otherwise
 * put up a page of choices. */

void doAdvFilter(struct sqlConnection *conn, struct column *colList);
/* Put up advanced filter page. */

void doAdvFilterClear(struct sqlConnection *conn, struct column *colList);
/* Clear variables in advanced filter page. */

void doAdvFilterBrowse(struct sqlConnection *conn, struct column *colList);
/* Put up family browser with advanced filter group by. */

void doAdvFilterList(struct sqlConnection *conn, struct column *colList);
/* List gene names matching advanced filter. */

void doConfigure(struct sqlConnection *conn, struct column *colList, 
	char *bumpVar);
/* Configuration page. */

void doDefaultConfigure(struct sqlConnection *conn, struct column *colList );
/* Do configuration starting with defaults. */

void doConfigHideAll(struct sqlConnection *conn, struct column *colList);
/* Respond to hide all button in configuration page. */

void doConfigShowAll(struct sqlConnection *conn, struct column *colList);
/* Respond to show all button in configuration page. */

void doNameCurrentColumns();
/* Put up page to save current column configuration. */

void doSaveCurrentColumns(struct sqlConnection *conn, struct column *colList);
/* Save the current columns, and then go on. */

#ifdef OLD
void doConfigSaveCurrent(struct sqlConnection *conn, struct column *colList);
/* Respond to Save Current Settings buttin in configuration page. */
#endif /* OLD */

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

/* ---- User settings stuff - soon to be moved to library I hope. */

struct userSettings 
/* This helps us the user a set of cart variables, defined
 * by settingsPrefix.  It allows the user to save different
 * sets of settings under different names. */
     {
     struct userSettings *next;	/* Next in list. */
     struct cart *cart;		/* Associated cart. */
     char *settingsPrefix;	/* Prefix (including .) of variables to save. */
     char *savePrefix;		/* Prefix for where we store settings.
                                 * We store at savePrefix.name where name
				 * is taken from listVar above. */
     char *nameVar;		/* Variable for name on give-it-a-name page. */
     char *formVar;		/* Submit button on give-it-a-name page. */
     char *listDisplayVar;	/* Variable name for list display control. */
     };

struct userSettings *userSettingsNew(
	struct cart *cart,
	char *settingsPrefix, 
	char *formVar,
	char *localVarPrefix);
/* Make new object to help manage sets of user settings. 
 * The settingsPrefix determines which user settings will
 * be saved.  The form variable is the name of the control
 * for the submit buttons generated by this system.  
 * The localVarPrefix defines a name space that the
 * form controls this object manages can use. When the
 * formVar is present in the cart then the application
 * needs to call userSettingsProcessForm.
 */

boolean userSettingsAnySaved(struct userSettings *us);
/* Return TRUE if any user settings are saved. */

void userSettingsUseNamed(struct userSettings *us, char *setName);
/* Use named collection of settings. */

void userSettingsUseSelected(struct userSettings *us);
/* Use currently selected user settings. */

void userSettingsSaveForm(struct userSettings *us, char *title, ...);
/* Put up controls that let user name and save the current
 * set. */

void userSettingsProcessForm(struct userSettings *us);
/* Handle button press in userSettings form. */

void userSettingsDropDown(struct userSettings *us);
/* Display list of available saved settings . */

#endif /* HGNEAR_H */

