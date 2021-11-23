/* hgText.h generic item to be shared between source files in this dir
 *
 *	$Id: hgText.h,v 1.6 2004/04/28 22:07:12 hiram Exp $
 */

/* Copyright (C) 2004 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef HGTEXT_H
#define HGTEXT_H

/* Values for outputTypePosMenu	*/
#define allFieldsPhase      "Tab-separated, All fields"
#define chooseFieldsPhase   "Tab-separated, Choose fields..."
#define seqOptionsPhase     "FASTA (DNA sequence)..."
#define gffPhase            "GTF"
#define bedOptionsPhase     "BED..."
#define ctOptionsPhase      "Custom Track..."
#define ctWigOptionsPhase   "Data custom track..."
#define linksPhase          "Hyperlinks to Genome Browser"
#define statsPhase          "Summary/Statistics"
#define wigOptionsPhase     "Get data points"

/* Other values that the "phase" var can take on: */
#define chooseTablePhase    "table"
#define pasteNamesPhase     "Paste in Names/Accessions"
#define uploadNamesPhase    "Upload Names/Accessions File"
#define outputOptionsPhase  "Advanced query..."
#define getOutputPhase      "Get results"
#define getSomeFieldsPhase  "Get these fields"
#define getSequencePhase    "Get sequence"
#define getBedPhase         "Get BED"
#define getCtPhase          "Get Custom Track"
#define getCtBedPhase       "Get Custom Track File"
#define getWigglePhase      "Get data"
#define getCtWiggleTrackPhase	"Get Custom Data Track"
#define getCtWiggleFilePhase	"Get Custom Data Track File"
#define intersectOptionsPhase "Intersect Results..."
#define histPhase           "Get histogram"
#define descTablePhase      "Describe table"
/* Old "phase" values handled for backwards compatibility: */
#define oldAllFieldsPhase   "Get all fields"
#define oldSeqOptionsPhase  "Get sequence..."

extern boolean typeWiggle;
extern boolean typeWiggle2;

/*	wiggle constraints, two for two possible tables */
extern char *wigConstraint[2];
extern struct bed *bedListWig[2];

/*	wiggle data value constraints, two possible tables, and two
 *	values for a possible range
 */
extern double wigDataConstraint[2][2];

/*	to select one of the two tables	*/
#define WIG_TABLE_1	0
#define WIG_TABLE_2	1
#define MAX_LINES_OUT	100000
#define DATA_AS_BED	TRUE
#define DATA_AS_POINTS	FALSE
#define DO_CT_DATA	TRUE
#define NOT_CT_DATA	FALSE

extern boolean (*wiggleCompare[2])(int tableId, double value,
    boolean summaryOnly, struct wiggle *wiggle);

extern char *database;
extern char *chrom;
extern int winStart;
extern int winEnd;
extern boolean tableIsSplit;
extern boolean allGenome;
extern char *getTableDb();
extern void getFullTableName(char *dest, char *newChrom, char *table);
extern char *getTableName();
extern char *getTable2Name();
extern char *getTrackName();
extern char *getTableVar();
extern void checkTableExists(char *table);
extern boolean existsAndEqual(char *var, char *value);
extern struct hTableInfo *getOutputHti();
extern struct customTrack *getCustomTracks();
extern struct customTrack *newCT(char *ctName, char *ctDesc, int visNum,
    char *ctUrl, int fields);
extern void saveOutputOptionsState();
extern void saveIntersectOptionsState();
extern char *constrainFields(char *tableId);
extern void preserveConstraints(char *fullTblName, char *db, char *tableId);
extern void preserveTable2();
extern void displayPosition();
extern char *customTrackPseudoDb;
extern struct slName *getOrderedChromList();
extern struct cart *cart;
extern char *freezeName;
extern char fullTableName[256];
extern char *httpFormMethod;

/*	in hgWigText.c	*/
extern void wigMakeBedList(char *db, char *table, char *chrom,
    char * constraints, int tableId);
extern void wigDoStats(char *database, char *table, struct slName *chromList,
    int tableId, char *constraints);
extern void wiggleConstraints(char *cmp, char *pat, int tableIndex);
extern void doGetWiggleData(boolean doCt);
extern void doWiggleCtOptions(boolean doCt);

#endif	/*	HGTEXT_H	*/
