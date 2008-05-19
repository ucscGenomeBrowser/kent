/* hgc - Human Genome Click processor - gets called when user clicks
 * on something in human tracks display. This file contains stuff
 * shared with other modules in hgc,  but not in other programs. */
#ifndef HGC_H
#define HGC_H

#ifndef CART_H
#include "cart.h"
#endif

#ifndef TRACKDB_H
#include "trackDb.h"
#endif 

#ifndef BED_H
#include "bed.h"
#endif

#ifndef HDB_H
#include "hdb.h"
#endif 

#ifndef HPRINT_H
#include "hPrint.h"
#endif

#ifndef CUSTOMTRACK_H
#include "customTrack.h"
#endif

extern struct cart *cart;	/* User's settings. */
extern char *seqName;		/* Name of sequence we're working on. */
extern int winStart, winEnd;    /* Bounds of sequence. */
extern char *database;		/* Name of mySQL database. */
extern char *organism;		/* Colloquial name of organism. */
extern char *genome;		/* common name, e.g. Mouse, Human */
extern char *scientificName;	/* Scientific name of organism. */
extern struct hash *trackHash;	/* A hash of all tracks - trackDb valued */


void hgcStart(char *title);
/* Print out header of web page with title.  Set
 * error handler to normal html error handler. */

char *hgcPath();
/* Return path of this CGI script. */

char *hgcPathAndSettings();
/* Return path with this CGI script and session state variable. */

void hgcAnchorSomewhere(char *group, char *item, char *other, char *chrom);
/* Generate an anchor that calls click processing program with item 
 * and other parameters. */

void hgcAnchorWindow(char *group, char *item, int thisWinStart, 
        int thisWinEnd, char *other, char *chrom);
/* Generate an anchor that calls click processing program with item
 * and other parameters, INCLUDING the ability to specify left and
 * right window positions different from the current window*/

void hgcAnchor(char *group, char *item, char *other);
/* Generate an anchor that calls click processing program with item 
 * and other parameters. */

void writeFramesetType();
/* Write document type that shows a frame set, rather than regular HTML. */

struct psl *getAlignments(struct sqlConnection *conn, char *table, char *acc);
/* get the list of alignments for the specified acc */

void printAlignmentsSimple(struct psl *pslList, int startFirst, char *hgcCommand,
                           char *typeName, char *itemIn);
/* Print list of mRNA alignments, don't add extra textual link when 
 * doesn't honor hgcCommand. */

void printAlignments(struct psl *pslList, 
		     int startFirst, char *hgcCommand, char *typeName, char *itemIn);
/* Print list of mRNA alignments. */

void showSomeAlignment(struct psl *psl, bioSeq *oSeq, 
		       enum gfType qType, int qStart, int qEnd, 
		       char *qName, int cdsS, int cdsE);
/* Display protein or DNA alignment in a frame. */

void linkToOtherBrowserTitle(char *otherDb, char *chrom, int start, int end, char *title);
/* Make anchor tag to open another browser window with a title. */

void linkToOtherBrowser(char *otherDb, char *chrom, int start, int end);
/* Make anchor tag to open another browser window. */

void printEntrezPubMedUidUrl(FILE *f, int pmid);
/* Print URL for Entrez browser on a PubMed search. */

boolean clipToChrom(int *pStart, int *pEnd);
/* Clip start/end coordinates to fit in chromosome. */

void printCappedSequence(int start, int end, int extra);
/* Print DNA from start to end including extra at either end.
 * Capitalize bits from start to end. */

void printPos(char *chrom, int start, int end, char *strand, boolean featDna,
	      char *item);
/* Print position lines.  'strand' argument may be null. */

void bedPrintPos(struct bed *bed, int bedSize);
/* Print first three fields of a bed type structure in
 * standard format. */

void savePosInTextBox(char *chrom, int start, int end);
/* Save basic position/database info in text box and hidden var. 
   Positions becomes chrom:start-end*/

void genericHeader(struct trackDb *tdb, char *item);
/* Put up generic track info. */

void genericBedClick(struct sqlConnection *conn, struct trackDb *tdb, 
		     char *item, int start, int bedSize);
/* Handle click in generic BED track. */

void printPosOnChrom(char *chrom, int start, int end, char *strand,
		     boolean featDna, char *item);
/* Print position lines referenced to chromosome. Strand argument may be NULL */

void printTrackHtml(struct trackDb *tdb);
/* If there's some html associated with track print it out. */

void abbr(char *s, char *fluff);
/* Cut out fluff from s. */

void printTableHeaderName(char *name, char *clickName, char *url);
/* creates a table to display a name vertically,
 * basically creates a column of letters */

void printBacStsXRef(char *clone);
/* Print out associated STS XRef information for BAC clone on BAC ends */
/* tracks details pages. */

/* ----Routines in other modules in the same directory---- */
void genericWiggleClick(struct sqlConnection *conn, struct trackDb *tdb, 
	char *item, int start);
/* Display details for WIGGLE tracks. */

void genericMafClick(struct sqlConnection *conn, struct trackDb *tdb, 
	char *item, int start);
/* Display details for MAF tracks. */

void genericAxtClick(struct sqlConnection *conn, struct trackDb *tdb, 
	char *item, int start, char *otherDb);
/* Display details for AXT tracks. */

void genericExpRatio(struct sqlConnection *conn, struct trackDb *tdb, 
	char *item, int start);
/* Display details for expRatio tracks. */

void rosettaDetails(struct trackDb *tdb, char *item);
/* Set up details for rosetta. */

void affyDetails(struct trackDb *tdb, char *item);
/* Set up details for affy. */

void gnfExpRatioDetails(struct trackDb *tdb, char *item);
/* Put up details on some gnf track. */

void loweExpRatioDetails(struct trackDb *tdb, char *item);
/* Put up details on some lowe track. */

void affyUclaDetails(struct trackDb *tdb, char *item);
/* Set up details for affyUcla. */

void cghNci60Details(struct trackDb *tdb, char *item);
/* Set up details for cghNci60. */

void nci60Details(struct trackDb *tdb, char *item);
/* Set up details for nci60. */

void doAffyHumanExon(struct trackDb *tdb, char *item);
/* Details for affyHumanExon all exon arrays. */

void doExpRatio(struct trackDb *tdb, char *item, struct customTrack *ct);
/* Generic expression ratio deatils using microarrayGroups.ra file */
/* and not the expRecord tables. */

void getMsBedExpDetails(struct trackDb *tdb, char *expName, boolean all);
/* Create tab-delimited output to download */
 
void printPslFormat(struct sqlConnection *conn, struct trackDb *tdb, char *item, int start, char *subType);
/*Handles click in Affy tracks and prints out alignment details with link*/
/* to sequences if available in the database */
 
void doAffy(struct trackDb *tdb, char *item, char *itemForUrl);
/* Display alignment information for Affy tracks */

void doScaffoldEcores(struct trackDb *tdb, char *item);
/* Creates details page and gets the scaffold co-ordinates for unmapped */
/* genomes for display and to use to create the correct outside link URL */

struct customTrack *lookupCt(char *name);
/* Return custom track for name, or NULL. */

void doRnaSecStr(struct trackDb *tdb, char *itemName);
/* Handle click on rnaSecStr type elements. */

void doTriangle(struct trackDb *tdb, char *item, char *motifTable);
/* Display detailed info on a regulatory triangle item. */

void doTransRegCode(struct trackDb *tdb, char *item, char *motifTable);
/* Display detailed info on a transcriptional regulatory code item. */

void doTransRegCodeProbe(struct trackDb *tdb, char *item, 
	char *codeTable, char *motifTable, 
	char *tfToConditionTable, char *conditionTable);
/* Display detailed info on a CHIP/CHIP probe from transRegCode experiments. */

void doFlyreg(struct trackDb *tdb, char *item);
/* flyreg.org: Drosophila DNase I Footprint db. */

void dbRIP(struct trackDb *tdb, char *item, char *itemForUrl);
/* Put up dbRIP track info */

void doCgapSage(struct trackDb *tdb, char *itemName);
/* CGAP SAGE details. */

char* getEntrezNucleotideUrl(char *accession);
/* get URL for Entrez browser on a nucleotide. free resulting string */

void printEntrezNucleotideUrl(FILE *f, char *accession);
/* Print URL for Entrez browser on a nucleotide. */

void printEntrezProteinUrl(FILE *f, char *accession);
/* Print URL for Entrez browser on a protein. */

void printCcdsForMappedGene(struct sqlConnection *conn, char *acc,
                            char *mapTable);
/* Print out CCDS links a gene mapped via a cddsGeneMap table  */

int getImageId(struct sqlConnection *conn, char *acc);
/* get the image id for a clone, or 0 if none */

char *getRefSeqSummary(struct sqlConnection *conn, char *acc);
/* RefSeq summary or NULL if not available; free result */

char *getRefSeqCdsCompleteness(struct sqlConnection *conn, char *acc);
/* get description of RefSeq CDS completeness or NULL if not available */

char *kgIdToSpId(struct sqlConnection *conn, char* kgId);
/* get the swissprot id for a known genes id; resulting string should be
 * freed */

char *hgTracksPathAndSettings();
/* Return path with hgTracks CGI path and session state variable. */

void medlineLinkedTermLine(char *title, char *text, char *search, char *keyword);
/* Produce something that shows up on the browser as
 *     TITLE: value
 * with the value hyperlinked to medline using a specified search term. */

void medlineLinkedLine(char *title, char *text, char *search);
/* Produce something that shows up on the browser as
 *     TITLE: value
 * with the value hyperlinked to medline. */

void hgcAnchorPosition(char *group, char *item); 
/* Generate an anchor that calls click processing program with item 
 * and group parameters. */

void geneShowPosAndLinks(char *geneName, char *pepName, struct trackDb *tdb, 
			 char *pepTable, char *pepClick, 
			 char *mrnaClick, char *genomicClick, char *mrnaDescription);
/* Show parts of gene common to everything. If pepTable is not null,
 * it's the old table name, but will check gbSeq first. */

bool loweLabClick(char *track, char *item, struct trackDb *tdb);
/* check if we have one of the lowelab tracks */

void showTxInfo(char *geneName, struct trackDb *tdb, char *txInfoTable);
/* Print out stuff from txInfo table. */

void showCdsEvidence(char *geneName, struct trackDb *tdb, char *evTable);
/* Print out stuff from cdsEvidence table. */

void doWikiTrack(char *itemName, char *chrom, int winStart, int winEnd);
/* handle item clicks on wikiTrack - may create new items */

void doCreateWikiItem(char *itemName, char *chrom, int winStart, int winEnd);
/* handle create item clicks for wikiTrack */

void doAddWikiComments(char *itemName, char *chrom, int winStart, int winEnd);
/* handle add comment item clicks for wikiTrack */

void doDeleteWikiItem(char *itemName, char *chrom, int winStart, int winEnd);
/* handle delete item clicks for wikiTrack */

#endif
