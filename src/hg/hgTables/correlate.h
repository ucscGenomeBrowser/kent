/* correlate.h - a help text string for correlate.c */
/*	and communication between correlate.c and correlatePlot.c	*/

/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef CORRELATE_H
#define CORRELATE_H

#if defined(INCL_HELP_TEXT)
static char *corrHelpText = "\
<HR>\
<H2>Methods</H2>\
<P>\
The data points from each table are projected down to the base level. \
At this level, the two data sets are intersected such that they each \
have data values at the same bases, resulting in data sets of equal length \
<em>n</em>. \
These two data sets (X,Y) are then used \
in a standard linear correlation function, computing the \
correlation coefficient: <BR> \
<BLOCKQUOTE> \
<em>r = s<sub>xy</sub> / (s<sub>x</sub> * s<sub>y</sub>)</em><BR> \
</BLOCKQUOTE> \
where <em>s<sub>x</sub></em> and <em>s<sub>y</sub></em> are the \
standard deviations of the data sets X and Y, and \
<em>s<sub>xy</sub></em> is the <em><b>covariance</b></em>, computed as: <BR>\
<BLOCKQUOTE> \
<em>s<sub>xy</sub> = ( sum(X<sub>i</sub>*Y<sub>i</sub>) - \
((sum(X<sub>i</sub>)*sum(Y<sub>i</sub>))/n) ) / (n - 1)</em> \
</BLOCKQUOTE> \
</P>\
<P>\
When available, the data values from a track are used in the calculations. \
For tracks that do not have data values, such as gene-structured tracks, \
the data value used in the calculation is 1.0 for bases that are covered \
by exons and 0.0 at all other positions in the region.  For simple tracks \
that are not gene structures, or data-valued tracks, the calculation \
uses the value 1.0 over the extent of the item and 0.0 for all other \
positions in the region.\
</P>\
<H2>Known Limitations</H2>\
<P>The number of bases that can be processed is limited, on the order of \
about 300,000,000, due to memory and processing time limits.  Therefore, \
large chromosomes, <em>e.g.</em> chr1 on human, cannot be processed entirely. \
Future improvements are expected to remove this processing limit.\
</P>\
<P>\
Intersection filters set on the main table browser page will function here, \
but they will apply to both tables when possible.  The various intersection \
options can produce unusual results.  It is probably best to avoid \
intersections or to use the results of an intersection that have been \
saved in a custom track.\
</P>";
#endif

#include "hgTables.h"

/*	Each track data's values will be copied into one of these structures,
 *	This is also the form that a result vector will take.
 *	There can be a linked list of these for multiple regions.
 *	The positions are in order, there are no gaps, count is the
 *	length of the position and value arrays.
 */
struct dataVector
    {
    struct dataVector *next;	/* linked list for multiple regions */
    char *chrom;		/* Chromosome. */
    int start;			/* Zero-based. */
    int end;			/* Non-inclusive. */
    char *name;		/*	potentially a region name (for encode) */
    int count;		/*	number of data values	*/
    int maxCount;	/*	no more than this number of data values	*/
    int *position;	/*	array of chrom positions, 0-relative	*/
    float *value;	/*	array of data values at these positions	*/
    double min;		/*	minimum data value in the set	*/
    double max;		/*	maximum data value in the set	*/
    double sumData;	/*	sum of all data here, sum(Xi)	*/
    double sumSquares;	/*	sum of squares of all data here, sum(Xi*Xi) */
    double sumProduct;	/*	accumulates sum(Xi * Yi) here */
    double r;		/*	correlation coefficient	*/
    double m;		/*	the m in: y = mx + b [linear regression line] */
    double b;		/*	the b in: y = mx + b [linear regression line] */
    long fetchTime;	/*	msec	*/
    long calcTime;	/*	msec	*/
    };

/*	there can be a list of these for N number of tables to work with */
/*	the first case will be two only, later improvements may do N tables */
struct trackTable
    {
    struct trackTable *next;
    struct trackDb *tdb;	/*	may be wigMaf primary	*/
    char *tableName;	/* the actual name, without composite confusion */
    char *dbTableName;	/* the actual database name, without ct confusion */
    char *shortLabel;
    char *longLabel;
    boolean isBedGraph;		/* type of data in table	*/
    boolean isWig;		/* type of data in table	*/
    boolean isBigWig;		/* type of data in table        */
    boolean isCustom;		/* might be a custom track	*/
    int bedGraphColumnNum;	/* the column where the dataValue is */
    char *bedGraphColumnName;	/* the name of the bedGraph column */
    int bedColumns;		/* the number of columns in the bed table */
				/*	as specified in the type line */
    struct trackDb *actualTdb;
			/* the actual tdb, without composite/wigMaf confusion */
    char *actualTable;	/* without composite/wigMaf confusion */
    struct dataVector *vSet;	/* the data for this table, all regions */
    int count;		/*	number of data values over all regions	*/
    double min;		/*	minimum data value over all regions	*/
    double max;		/*	maximum data value over all regions	*/
    double sumData;	/*	sum of all data here, all regions */
    double sumSquares;	/*	sum of squares of all data here, all regions */
    };

/*	functions in correlatePlot.c	*/

#define GRAPH_WIDTH	300	/*	actual graphing area size	*/
#define GRAPH_HEIGHT	300	/*	left and bottom text will add to this */
#define PLOT_MARGIN	2	/*	around graph and around everything */
#define DOT_SIZE	2	/*	size of plotted point	*/

struct tempName *scatterPlot(struct trackTable *table1,
    struct trackTable *table2, struct dataVector *result,
	int *width, int *height);
/*	create scatter plot gif file in trash, return path name */

struct tempName *residualPlot(struct trackTable *table1,
    struct trackTable *table2, struct dataVector *result, double *F_statistic,
	double *fitMin, double *fitMax, int *width, int *height);
/*	create residual plot gif file in trash, return path name */

struct tempName *histogramPlot(struct trackTable *table,
    int *width, int *height);
/*	create histogram plot gif file in trash, return path name */

struct trackTable *trackTableNew(struct trackDb *tdb, char *tableName,
				 struct sqlConnection *conn);
/* Allocate, fill in and return a trackTable. */

struct dataVector *dataVectorFetchOneRegion(struct trackTable *table,
	struct region *region, struct sqlConnection *conn);
/*	Return a dataVector containing all the data for this track and table 
 *	in the given region */

struct dataVector *dataVectorNew(char *chrom, int size);
/* allocate a dataVector for 'size' number of data points on specified chrom */

void freeDataVector(struct dataVector **v);
/*	free up space belonging to a dataVector	*/
#define dataVectorFree(x) freeDataVector(x)

enum dataVectorBinaryOpType
    {
    dataVectorBinaryOpSum,
    dataVectorBinaryOpProduct,
    dataVectorBinaryOpMin,
    dataVectorBinaryOpMax,
    };

void dataVectorBinaryOp(struct dataVector *dv1, struct dataVector *dv2,
			enum dataVectorBinaryOpType op, boolean requireBoth);
/* Store op(dv1, dv2) in dv1.  
 * If requireBoth is TRUE, remove a value from dv1 if dv2 has no value at the 
 * same position; if FALSE, leave dv1 value unchanged if dv2 has no value at 
 * same position. */

#define dataVectorSum(dv1,dv2,r) (dataVectorBinaryOp(dv1, dv2, dataVectorBinaryOpSum, r))
#define dataVectorProduct(dv1,dv2,r) (dataVectorBinaryOp(dv1, dv2, dataVectorBinaryOpProduct, r))
#define dataVectorMin(dv1,dv2,r) (dataVectorBinaryOp(dv1, dv2, dataVectorBinaryOpMin, r))
#define dataVectorMax(dv1,dv2,r) (dataVectorBinaryOp(dv1, dv2, dataVectorBinaryOpMax, r))

void dataVectorNormalize(struct dataVector *dv, float denominator);
/* Divide each value in dv by denominator. */

void dataVectorFilterMin(struct dataVector *dv, float minScore);
/* Remove values less than minScore from dv. */

void dataVectorIntersect(struct dataVector *dv1, struct dataVector *dv2,
			 boolean dv2IsWiggle, boolean complementDv2);
/* Remove a value from dv1 if dv2 has no value (or if complementDv2, any value)
 * at the same position.  Note: this will change dv2 too, unless complementDv2 
 * is set! */

int dataVectorWriteWigAscii(struct dataVector *dataVectorList,
			    char *filename, int maxOut, char *description);
/* Write wigAscii for all dataVectors in dataVectorList to filename. 
 * Return the number of datapoints written. */

struct bed *dataVectorToBedList(struct dataVector *dvList);
/* Allocate and return a bedList of ranges where dataVectors in dvList 
 * contain values. */

int dataVectorWriteBed(struct dataVector *dvList, char *filename, int maxOut,
		       char *description);
/* Print out bed of ranges where dataVectors in dvList contain values. 
 * Return the number of datapoints distilled into bed (the number of bed 
 * items will usually be smaller than the number of datapoints). */

struct dataVector *bedGraphDataVector(char *table,
	struct sqlConnection *conn, struct region *region);
/* Read in bedGraph as dataVector and return it.  Filtering, subtrack merge 
 * and intersection are handled. */

struct dataVector *wiggleDataVector(struct trackDb *tdb, char *table,
	struct sqlConnection *conn, struct region *region);
/* Read in wiggle as dataVector and return it.  Filtering, subtrack merge 
 * and intersection are handled. */

struct dataVector *bigWigDataVector(char *table,
	struct sqlConnection *conn, struct region *region);
/* Read in bigWig as dataVector and return it.  Filtering, subtrack merge 
 * and intersection are handled. */

void bigWigFillDataVector(char *table, struct region *region, 
	struct sqlConnection *conn, struct dataVector *vector);
/* Fill in data vector with bigWig info on region.  Handles filters and 
 * intersections, but not merging. */

struct dataVector *mergedWigDataVector(char *table,
	struct sqlConnection *conn, struct region *region);
/* Perform the specified subtrack merge wiggle-operation on table and 
 * all other selected subtracks and intersect if necessary. */

int wigPrintDataVectorOut(struct dataVector *dataVectorList,
			      enum wigOutputType wigOutType, int maxOut,
			      char *description);
/* Print out bed or data points from list of dataVectors. */

#endif /* CORRELATE_H */
