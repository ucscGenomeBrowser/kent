/* hgGenome.h - Include file used by all modules in hgGenome. 
 * hgGenome is a CGI script that produces a web page containing
 * a graphic with all chromosomes in genome, and a graph or two
 * on top of them. */

#ifndef HGGENOME_H
#define HGGENOME_H

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
#define hggLinesOfGraphs hggPrefix "linesOfGraphs"
#define hggDataSetName hggPrefix "dataSetName"
#define hggDataSetDescription hggPrefix "dataSetDescription"
#define hggLocType hggPrefix "locType"
#define hggFormatType hggPrefix "formatType"
#define hggColumnLabels hggPrefix "columnLabels"
#define hggMinVal hggPrefix "minVal"
#define hggMaxVal hggPrefix "maxVal"
#define hggLabelVals hggPrefix "labelVals"
#define hggMaxGapToFill hggPrefix "maxGapToFill"
#define hggUploadFile hggPrefix "uploadFile"
#define hggUploadRa hggPrefix "uploadRa"

/*** Command variables. ***/
#define hggConfigure hggDo "Configure"
#define hggBrowse hggDo "Browse"
#define hggSort hggDo "Sort"
#define hggCorrelate hggDo "Correlate"
#define hggUpload hggDo "Upload"
#define hggSubmitUpload hggDo "SubmitUpload"
#define hggClick hggDo "Click"
#define hggClickX hggClick ".x"
#define hggClickY hggClick ".y"
#define hggSubmitUpload2 hggDo "SubmitUpload2"

/*** External vars declared in hgGenome.c ***/
extern struct cart *cart;
extern struct hash *oldCart;
extern char *database;
extern char *genome;
extern struct genoGraph *ggUserList;	/* List of user graphs */
extern struct genoGraph *ggDbList;	/* List of graphs in database. */
extern struct slRef *ggList; /* List of active genome graphs */

/*** Name prefixes to separate user from db graphs. */
#define hggUserTag "user: "
#define hggDbTag "db: "

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
    };


/*** Routines from hgGenome.h ***/

void hPrintf(char *format, ...);
/* Print out some html.  Check for write error so we can
 * terminate if http connection breaks. */

int graphHeight();
/* Return height of graph. */

#define betweenRowPad 3

#define minGraphsPerLine 1
#define maxGraphsPerLine 4
#define defaultGraphsPerLine 2
int graphsPerLine();
/* Return number of graphs to draw per line. */

#define minLinesOfGraphs 1
#define maxLinesOfGraphs 6
#define defaultLinesOfGraphs 1
int linesOfGraphs();
/* Return number of lines of graphs */

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

void mainPage(struct sqlConnection *conn);
/* Do main page of application:  hotlinks bar, controls, graphic. */

/*** Functions imported from other modules. ***/

void uploadPage();
/* Put up initial upload page. */

void configurePage();
/* Put up configuration page. */

void correlatePage(struct sqlConnection *conn);
/* Put up correlation page. */

void submitUpload(struct sqlConnection *conn);
/* Called when they've submitted from uploads page */

void submitUpload2(struct sqlConnection *conn);
/* Called when they've submitted from uploads page */

void browseRegions(struct sqlConnection *conn);
/* Put up a frame with a list of links on the left and the
 * first link selected on the right. */

void sortGenes(struct sqlConnection *conn);
/* Put up sort gene page. */

void clickOnImage(struct sqlConnection *conn);
/* Handle click on image - calculate position in forward to genome browser. */

#endif /* HGGENOME_H */
