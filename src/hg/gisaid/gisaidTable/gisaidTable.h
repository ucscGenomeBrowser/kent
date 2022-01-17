/* gisaidTable.h - interfaces to plug columns into gisaidTable.  The must important
 * thing here is the column structure. */

/* Copyright (C) 2009 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef gisaidTable_H
#define gisaidTable_H

struct subjInfo
/* subjInfo - subject info */
    {
    struct subjInfo *next;
    char **fields;       /* array of field values from gisaidSubjInfo. fields[0] is always subjId */
    char *sortString;    /* value to use if sorting as a string */
    int sortInteger;     /* value to use if sorting as an integer */
    int sortDouble;      /* value to use if sorting as an double */
    };


struct column
/* A column in the big table. The central data structure for
 * gisaidTable. */
    {
    /* Data set during initialization that is guaranteed to be in each track.  */
    struct column *next;		/* Next column. */
    char *name;			/* Column name, not allocated here. */
    char *shortLabel;		/* Column label. */
    char *longLabel;		/* Column description. */
    float priority;		/* Order displayed. */
    boolean on;			/* True if turned on. */
    boolean defaultOn;		/* On by default? */
    boolean filterOn;		/* True if its filter on. */
    char *type;                 /* type of column */
    struct hash* remap;         /* hash for substitutes if type="remap" */
    char *query;                /* query if not in gisaidSubjInfo main table */
    boolean filterDropDown;	/* True if filter uses dropdown list. */

    int colNo;                  /* index into subjInfo string array or -1 if not in gisaidSubjInfo table */

    struct hash *settings;	/* Settings from ra file. */

    /* -- Methods -- */

    boolean (*exists)(struct column *col, struct sqlConnection *conn);
    /* Return TRUE if column exists in database. */

    char *(*cellVal)(struct column *col, struct subjInfo *si,
	struct sqlConnection *conn);
    /* Get value of one cell as string.  FreeMem this when done.  Note that
    * gp->chrom may be NULL legitimately. */

    void (*cellPrint)(struct column *col, struct subjInfo *si,
	struct sqlConnection *conn);
    /* Print one cell of this column.  Note that gp->chrom may be
    * NULL legitimately. */

    void (*labelPrint)(struct column *col);
    /* Print the label in the label row. */

    int (*tableColumns)(struct column *col);
    /* How many html columns this uses. */

    struct subjInfo *(*advFilter)(struct column *col, struct sqlConnection *conn,
	struct subjInfo *inputList);
    /* Return list of positions for advanced filter. */

    int (*sortCmp)(const void *va, const void *vb);
    /* Compare to sort rows based on value. */

    void (*filterControls)(struct column *col, struct sqlConnection *conn);
    /* Print out controls for advanced filter. */



    };

/* ---- global variables ---- */

extern struct cart *cart; /* This holds variables between clicks. */
extern char *database;	  /* Name of genome database - hg15, or the like. */
extern char *genome;	  /* Name of organism - mouse, human, etc. */
extern char *sortOn;	  /* Current sorted by column. */
extern int displayCount;  /* Number of items to display. */

extern struct hash *columnHash;		  /* Hash of active columns keyed by name. */

/* ---- Cart Variables ---- */

#define dbVarName "db"                  /* Which assembly to use. */
#define orgVarName "org"                /* Which organism to use. */
#define orderVarName "gisaidTable.order"	/* Sorting column. */
#define confVarName "gisaidTable.do.configure"	/* Configuration button */
#define advFilterVarName "gisaidTable.do.advFilter"      /* Advanced filter */
#define countVarName "gisaidTable_count"	/* How many items to display. */
#define getSeqHowVarName "gisaidTable.getSeqHow" /* How to get sequence. */
#define getSeqPageVarName "gisaidTable.do.getSeqPage" /* Button go to getSequence page. */
#define getSeqVarName "gisaidTable.do.getSeq"  /* Button to get sequence. */
#define getTextVarName "gisaidTable.do.getText"	/* Button to get as text. */

#define advFilterPrefix "gisaidTable.as."      
	/* Prefix for advanced filter variables. */
#define advFilterPrefixI "gisaidTable.asi."
        /* Prefix for advanced filter variables not forcing search. */
#define advFilterVarName "gisaidTable.do.advFilter"      /* Advanced filter */
#define advFilterClearVarName "gisaidTable.do.advFilterClear"
        /* Advanced filter clear all button. */
#define advFilterListVarName "gisaidTable.do.advFilterList"
        /* Advanced filter submit list. */

#define gisaidSubjList "gisaidTable.gisaidSubjList"
#define gisaidSeqList "gisaidTable.gisaidSeqList"
#define gisaidAaSeqList "gisaidTable.gisaidAaSeqList"

#define keyWordUploadPrefix "gisaidTable.do.keyUp." /* Prefix for keyword uploads. */
#define keyWordPastePrefix "gisaidTable.do.keyPaste." /* Prefix for keyword paste-ins. */
#define keyWordPastedPrefix "gisaidTable.do.keyPasted."
        /* Prefix for keyword paste-ins. */
#define keyWordClearPrefix "gisaidTable.do.keyClear." /* Prefix for keyword paste-ins. */




#define colConfigPrefix "gisaidTable.col."
        /* Prefix for stuff set in configuration pages. */
#define colOrderVar "gisaidTable.colOrder"     /* Order of columns. */
#define defaultConfName "gisaidTable.do.colDefault"  /* Restore to default settings. */
#define hideAllConfName "gisaidTable.do.colHideAll"  /* Hide all columns. */
#define showAllConfName "gisaidTable.do.colShowAll"  /* Show all columns. */

#define redirectName "fromProg"  /* cgi to redirect back to. */


/* ---- General purpose helper routines. ---- */

void saveSubjList(struct subjInfo *subjList);
/* save the filtered list of subject gisaids to a file for other applications to use */

boolean gotAdvFilter();
/* Return TRUE if advanced filter variables are set. */

void doAdvFilter(struct sqlConnection *conn, struct column *colList);
/* Put up advanced filter page. */

void doAdvFilterClear(struct sqlConnection *conn, struct column *colList);
/* Clear variables in advanced filter page. */

void doAdvFilterList(struct sqlConnection *conn, struct column *colList);
/* List gene names matching advanced filter. */

struct subjInfo *advFilterResults(struct column *colList,
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

void lookupAdvFilterControls(struct column *col, struct sqlConnection *conn);
/* Print out controls for advanced filter on lookup column. */

void minMaxAdvFilterControls(struct column *col, struct sqlConnection *conn);
/* Print out controls for min/max advanced filter. */

void dropDownAdvFilterControls(struct column *col, struct sqlConnection *conn);
/* Print out controls for dropdown list filter. */

struct subjInfo *readAllSubjInfo(struct sqlConnection *conn, struct column *columns);
/* Get all main table gisaidSubjInfo columns in use. */

boolean anyRealInCart(struct cart *cart, char *wild);
/* Return TRUE if variables are set matching wildcard. */

void controlPanelStart();
/* Put up start of tables around a control panel. */

void controlPanelEnd();
/* Put up end of tables around a control panel. */

void hotLinks();
/* Make top bar */

void makeTitle(char *title, char *helpName);
/* Make title bar. */

void doConfigure(struct sqlConnection *conn, struct column *colList);
	//, char *bumpVar);
/* Configuration page. */
void doDefaultConfigure(struct sqlConnection *conn, struct column *colList );
/* Do configuration starting with defaults. */

void doConfigHideAll(struct sqlConnection *conn, struct column *colList);
/* Respond to hide all button in configuration page. */

void doConfigShowAll(struct sqlConnection *conn, struct column *colList);
/* Respond to show all button in configuration page. */

void fixSafariSpaceInQuotes(char *s);
/* Safari on the Mac changes a space (ascii 32) to a
 * ascii 160 if it's inside of a single-quote in a
 * text input box!?  This tuns it back to a 32. */

char *colVarName(struct column *col, char *prefix);
/* Return variable name prefix.col->name. This is just a static
 * variable, so don't nest these calls. */


void colButton(struct column *col, char *prefix, char *label);
/* Make a button named prefix.col->name with given label. */

struct column *colButtonPressed(struct column *colList, char *prefix);
/* See if a button named prefix.column is pressed for some
 * column, and if so return the column, else NULL. */

void doGetSeqPage(struct sqlConnection *conn, struct column *colList);
/* Put up the get sequence page. */

void doGetSeq(struct sqlConnection *conn, struct subjInfo *siList, char *how);



#endif /* gisaidTable_H */

