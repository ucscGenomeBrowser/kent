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
#define hggThreshold hggPrefix "threshold"
#define hggGraphsPerLine hggPrefix "graphsPerLine"
#define hggLinesOfGraphs hggPrefix "linesOfGraphs"

/*** Command variables. ***/
#define hggConfigure hggDo "Configure"
#define hggBrowse hggDo "Browse"
#define hggSort hggDo "Sort"
#define hggCorrelate hggDo "Correlate"
#define hggUpload hggDo "Upload"

/*** External vars declared in hgGenome.h ***/
extern struct cart *cart;
extern struct hash *oldCart;
extern char *database;
extern char *genome;

/*** Routines from hgGenome.h ***/

void hPrintf(char *format, ...);
/* Print out some html.  Check for write error so we can
 * terminate if http connection breaks. */

int graphHeight();
/* Return height of graph. */

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

/*** Functions imported from other modules. ***/

void configurePage();
/* Put up configuration page. */

#endif /* HGGENOME_H */
