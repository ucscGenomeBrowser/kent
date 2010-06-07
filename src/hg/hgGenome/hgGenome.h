/* hgGenome.h - Include file used by all modules in hgGenome.
 * hgGenome is a CGI script that produces a web page containing
 * a graphic with all chromosomes in genome, and a graph or two
 * on top of them. */

#ifndef HGGENOME_H
#define HGGENOME_H

#include "grp.h"
#include "hPrint.h"
#include "customTrack.h"

/*** Prefixes for variables in cart we don't share with other apps. ***/
#define hggPrefix "hgGenome_"
#define hggDo hggPrefix "do"

/*** Our various cart variables. ***/
#define hggGraphPrefix hggPrefix "graph"
#define hggGraphColorPrefix hggPrefix "graphColor"
#define hggGraphHeight hggPrefix "graphHeight"
#define hggLabels hggPrefix "labels"
#define hggThreshold hggPrefix "threshold"
#define defaultThreshold 3.5
#define hggGraphsPerLine hggPrefix "graphsPerLine"
#define hggChromLayout hggPrefix "chromLayout"
#define hggLinesOfGraphs hggPrefix "linesOfGraphs"
#define hggDataSetName hggPrefix "dataSetName"
#define hggDataSetDescription hggPrefix "dataSetDescription"
#define hggMarkerType hggPrefix "markerType"
#define hggFormatType hggPrefix "formatType"
#define hggColumnLabels hggPrefix "columnLabels"
#define hggMinVal hggPrefix "minVal"
#define hggMaxVal hggPrefix "maxVal"
#define hggLabelVals hggPrefix "labelVals"
#define hggMaxGapToFill hggPrefix "maxGapToFill"
#define hggUploadFile hggPrefix "uploadFile"
#define hggUploadUrl hggPrefix "uploadUrl"
#define hggUploadRa hggPrefix "uploadRa"
#define hggImageWidth hggPrefix "imageWidth"
#define hggRegionPad hggPrefix "regionPad"
#define hggYellowMissing hggPrefix "yellowMissing"

#define hggRegionPadDefault 25000

/*** Command variables. ***/
#define hggConfigure hggDo "Configure"
#define hggConfigureOne hggDo "ConfigureOne"
#define hggBrowse hggDo "Browse"
#define hggSort hggDo "Sort"
#define hggCorrelate hggDo "Correlate"
#define hggUpload hggDo "Upload"
#define hggSubmitUpload hggDo "SubmitUpload"
#define hggImport hggDo "Import"
#define hggSubmitImport hggDo "SubmitImport"
#define hggClick hggDo "Click"
#define hggPsOutput hggDo "PsOutput"
#define hggClickX hggClick ".x"
#define hggClickY hggClick ".y"



/* -- import-track related -- */

#define hggGroup hggPrefix "group"
#define hggTrack hggPrefix "track"
#define hggTable hggPrefix "table"


#define hggBedConvertType hggPrefix "bedConvertType"
#define hggBedConvertTypeJs "js_bedConvertType"
#define hggBedCoverage "coverage"
#define hggBedDepth "depth"


/* Global variables - generally set during initialization and then read-only. */
extern char *curTable;  /* Current selected table. */
extern struct trackDb *curTrack;        /* Currently selected track. */


struct trackDb *getFullTrackList();
/* Get all tracks including custom tracks if any. */

struct customTrack *getCustomTracks();
/* Get custom track list. */

struct trackDb *findSelectedTrack(struct trackDb *trackList,
        struct grp *group, char *varName);
/* Find selected track - from CGI variable if possible, else
 * via various defaults. */

#define lookupCt(name) ctFind(getCustomTracks(),name)

struct customTrack *newCt(char *ctName, char *ctDesc, int visNum, char *ctUrl,
                          int fields);
/* Make a new custom track record for the query results. */

void removeNamedCustom(struct customTrack **pList, char *name);
/* Remove named custom track from list if it's on there. */

struct hTableInfo *ctToHti(struct customTrack *ct);
/* Create an hTableInfo from a customTrack. */

struct hTableInfo *getHti(char *db, char *table);
/* Return primary table info. */

struct hTableInfo *maybeGetHti(char *db, char *table);
/* Return primary table info, but don't abort if table not there. */



#define hgtaCtName "hgta_ctName"
#define hgtaCtDesc "hgta_ctDesc"
#define hgtaCtVis "hgta_ctVis"
#define hgtaCtUrl "hgta_ctUrl"
#define hgtaCtWigOutType "hgta_ctWigOutType"

/* Prefix for temp name files created for custom tracks */
#define hgtaCtTempNamePrefix "hgtct"
/* Prefix for variables containing filters themselves. */
#define hgtaFilterVarPrefix hgtaFilterPrefix "v."
/* Prefix for variables managed by filter page. */
#define hgtaFilterPrefix "hgta_fil."

#define hgtaPrintCustomTrackHeaders "hgta_printCustomTrackHeaders"

#define outWigData "wigData"
#define hgtaIntersectTable "hgta_intersectTable"
#define hgtaIntersectTrack "hgta_intersectTrack"
#define hgtaInvertTable2 "hgta_invertTable2"
#define hgtaIntersectOp "hgta_intersectOp"

/*** External vars declared in hgGenome.c ***/
extern struct cart *cart;
extern struct hash *oldCart;
extern char *database;
extern char *genome;
extern struct genoGraph *ggUserList;	/* List of user graphs */
extern struct genoGraph *ggDbList;	/* List of graphs in database. */
extern struct trackLayout tl;  /* Dimensions of things, fonts, etc. */
extern struct slRef *ggList; /* List of active genome graphs */
extern struct hash *ggHash;	  /* Hash of active genome graphs */
extern boolean withLabels;	/* Draw labels? */

/*** Name prefixes to separate user from db graphs. */
#define hggUserTag "user_"
#define hggDbTag ""

/*** Info on a single graph. ***/

struct genoGraph
/* A genomic graph */
    {
    struct genoGraph *next;	/* Next in list. */
    char *name;			/* Graph name. */
    char *shortLabel;		/* Short label. */
    char *longLabel;		/* Long label. */
    char *binFileName;		/* Binary file associated with graph. */
    struct chromGraphSettings *settings;  /* Display settings */
    boolean didRefine;		/* Set to true after refined */
    struct chromGraphBin *cgb;  /* Binary data handle. */
    boolean isSubGraph;         /* Set to TRUE if the graph is part of a composite. */
    boolean isComposite;        /* Set to TRUE if this just has composite settings. */
    };


/*** Routines from hgGenome.h ***/

int regionPad();
/* Number of bases to pad regions by. */

char *getThresholdName();
/* Return threshold name. */

double getThreshold();
/* Return user-set threshold */

boolean getYellowMissing();
/* Return draw background in yellow for missing data flag. */

struct bed3 *regionsOverThreshold();
/* Get list of regions over threshold */

int graphHeight();
/* Return height of graph. */

#define betweenRowPad 3

#define minGraphsPerLine 1
#define maxGraphsPerLine 4
#define defaultGraphsPerLine 2
int graphsPerLine();
/* Return number of graphs to draw per line. */

#define minLinesOfGraphs 1
#define maxLinesOfGraphs 10
#define defaultLinesOfGraphs 1
int linesOfGraphs();
/* Return number of lines of graphs */

#define layTwoPerLine "two per line"
#define layOnePerLine "one per line"
#define layAllOneLine "all in one line"
char *chromLayout();
/* Return one of above strings specifying layout. */

void getGenoGraphs(struct sqlConnection *conn);
/* Set up ggList and ggHash with all available genome graphs */

char *graphVarName(int row, int col);
/* Get graph data source variable for given row and column.  Returns
 * static buffer. */

char *graphColorVarName(int row, int col);
/* Get graph color variable for givein row and column. Returns
 * static buffer. */

char *graphSourceAt(int row, int col);
/* Return graph source at given row/column, NULL if none. */

char *graphColorAt(int row, int col);
/* Return graph color at given row/column, NULL if nonw. */

struct genoLay *ggLayout(struct sqlConnection *conn,
	int graphRows, int graphCols);
/* Figure out how to lay out image. */

struct genoGraph *ggFirstVisible();
/* Return first visible graph, or NULL if none. */

struct slRef *ggAllVisible();
/* Return list of references to all visible graphs */

void hggDoUsualHttp();
/* Wrap html page dispatcher with code that writes out
 * HTTP header and write cart back to database. */

/*** Functions imported from other modules. ***/

void handlePostscript(struct sqlConnection *conn);
/* Do graphic as eps/pdf. */

void mainPage(struct sqlConnection *conn);
/* Do main page of application:  hotlinks bar, controls, graphic. */

void printMainHelp();
/* Put up main page help info. */

void uploadPage();
/* Put up initial upload page. */

void importPage();
/* Put up initial import page. */

void configurePage();
/* Put up configuration page. */

void configureOnePage();
/* Put up configuration for one graph. */

void correlatePage(struct sqlConnection *conn);
/* Put up correlation page. */

void submitUpload(struct sqlConnection *conn);
/* Called when they've submitted from uploads page */

void submitUpload2(struct sqlConnection *conn);
/* Called when they've submitted from uploads page */

void submitImport();
/* Called when they've submitted from import page */

void browseRegions(struct sqlConnection *conn);
/* Put up a frame with a list of links on the left and the
 * first link selected on the right. */

void sortGenes(struct sqlConnection *conn);
/* Put up sort gene page. */

void clickOnImage(struct sqlConnection *conn);
/* Handle click on image - calculate position in forward to genome browser. */


/* ----------- Wiggle business in wiggle.c -------------------- */

#define	MAX_REGION_DISPLAY	1000

boolean isWiggle(char *db, char *table);
/* Return TRUE if db.table is a wiggle. */

boolean isBedGraph(char *table);
/* Return TRUE if table is specified as a bedGraph in the current database's
 * trackDb. */

char *getBedGraphType(char *table);
/* Return bedgraph track type if table is a bedGraph in the current database's
 * trackDb. */
 
int  getBedGraphColumnNum(char *table);
/* get the bedGraph dataValue column num from the track type */

void wiggleMinMax(struct trackDb *tdb, double *min, double *max);
/*	obtain wiggle data limits from trackDb or cart or settings */

struct wiggleDataStream *wigChromRawStats(char *chrom);
/* Fetch stats for wig data in chrom.
 * Returns a wiggleDataStream, free it with wiggleDataStreamFree() */



/* ------------ */

struct sqlResult *chromQuery(struct sqlConnection *conn, char *table,
        char *fields, char *chrom, boolean isPositional,
        char *extraWhere);
/* Construct and execute query for table on chrom. Returns NULL if
 * table doesn't exist (e.g. missing split table for chrom). */

struct bed *getBeds(char *chrom, struct lm *lm, int *retFieldCount);
/* Get list of beds on single chrom. */


struct bed *customTrackGetBedsForChrom(char *name, char *chrom,
	struct lm *lm,	int *retFieldCount);
/* Get list of beds from custom track of given name that are
 * in given chrom. You can bedFree this when done. */


boolean isMafTable(char *database, struct trackDb *track, char *table);
/* Return TRUE if table is maf. */


boolean isChromGraph(struct trackDb *track);
/* Return TRUE if it's a chromGraph track */

#endif /* HGGENOME_H */
