/* hgHeatmap.h - Include file used by all modules in hgHeatmap. 
 * hgHeatmap is a CGI script that produces a web page containing
 * a graphic with all chromosomes in genome, and a graph or two
 * on top of them. */

#include "cart.h"

#ifndef HGHEATMAP_H
#define HGHEATMAP_H

/*** Prefixes for variables in cart we don't share with other apps. ***/
#define hghPrefix "hgHeatmap_"
#define hghDo hghPrefix "do"

/*** Our various cart variables. ***/
#define hghHeatmap hghPrefix "heatmap"
#define hghExperimentHeight hghPrefix "experimentHeight"
#define hghChromLayout hghPrefix "chromLayout"
#define hghDataSetName hghPrefix "dataSetName"
#define hghDataSetDescription hghPrefix "dataSetDescription"
#define hghMarkerType hghPrefix "markerType"
#define hghFormatType hghPrefix "formatType"
#define hghLabelVals hghPrefix "labelVals"
#define hghMaxGapToFill hghPrefix "maxGapToFill"
#define hghImageWidth hghPrefix "imageWidth"

/*** Command variables. ***/
#define hghConfigure hghDo "Configure"
#define hghConfigureOne hghDo "ConfigureOne"
#define hghBrowse hghDo "Browse"
#define hghUpload hghDo "Upload"
#define hghSubmitUpload hghDo "SubmitUpload"
#define hghClick hghDo "Click"
#define hghPsOutput hghDo "PsOutput"
#define hghClickX hghClick ".x"
#define hghClickY hghClick ".y"

/*** customTrack***/
#define CUSTOMDB "CUSTOMDB"

/*** External vars declared in hgHeatmap.c ***/
extern struct cart *cart;
extern struct hash *oldCart;
extern char *database;
extern char *genome;
extern struct bed *ggUserList;	/* List of user graphs */
extern struct bed *ggDbList;	/* List of graphs in database. */
extern struct trackLayout tl;	/* Dimensions of things, fonts, etc. */
extern struct slRef *ghList;	/* List of active genome graphs */
extern struct hash *ghHash;	/* Hash of active genome graphs */
extern struct hash *ghOrder;	/* Hash of orders for data */

/*** Name prefixes to separate user from db graphs. */
#define hghUserTag "user: "
#define hghDbTag "db: "

/*** Info on a single heatmap. ***/

struct genoHeatmap
/* A genomic heatmap */
    {
    void *next;			/* Next in list. */
    char *name;                 /* Graph name. */
    char *shortLabel;           /* Short label. */
    char *longLabel;            /* Long label. */
    int expCount;		/* number of experiments */
    char *database;             /* database */
    struct trackDb *tDb;	/* the track database */
    };

/*** Routines from hgHeatmap.h ***/

int selectedHeatmapHeight();
/* Return height of user selected heatmaps from the web interface. */

int heatmapHeight(char* heatmap);
/* Return height of heatmap. */

char *heatmapName();
/* Return name of heatmap */

struct slName* heatmapNames();
/* Return names of all heatmaps */

int experimentHeight();
/* Return height of an experiment */

int experimentCount(char* heatmap);
/* Return the number of experiments */

#define betweenRowPad 3

#define layTwoPerLine "two per line"
#define layOnePerLine "one per line"
#define layAllOneLine "all in one line"
char *chromLayout();
/* Return one of above strings specifying layout. */

void getGenoHeatmaps(struct sqlConnection *conn);
/* Set up ggList and ggHash with all available genome heatmaps */

struct genoLay *ggLayout(struct sqlConnection *conn);
/* Figure out how to lay out image. */

struct genoHeatmap *getUserHeatmaps();
/* Get list of all user graphs */

int* addChromOrder(char* heatmap, char* chromName);
/* Add the ChromOrder of a specific heatmap and chromosome combo to 
   the ghOrder hash */

int* getChromOrder(char* heatmap, char* chromName);
/* Return the ChromOrder of a specific heatmap and chromosome combo to 
   the ghOrder hash 
   return an array for reordering the experiments in a chromosome 
*/

void hghDoUsualHttp();
/* Wrap html page dispatcher with code that writes out
 * HTTP header and write cart back to database. */

/*** Functions imported from other modules. ***/

void handlePostscript(struct sqlConnection *conn);
/* Do graphic as eps/pdf. */

void mainPage(struct sqlConnection *conn);
/* Do main page of application:  hotlinks bar, controls, graphic. */

void printMainHelp();
/* Put up main page help info. */

void configurePage();
/* Put up configuration page. */

void submitUpload2(struct sqlConnection *conn);
/* Called when they've submitted from uploads page */

void clickOnImage(struct sqlConnection *conn);
/* Handle click on image - calculate position in forward to genome browser. */

/* Convert a row of strings to a bed.
   Only load chromStart, chromEnd, expScores fields */
struct bed *bedLoad15Simple(char *row[]);

#endif /* HGHEATMAP_H */
