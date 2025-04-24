/* hgc - Human Genome Click processor - gets called when user clicks
 * on something in human tracks display. This file contains stuff
 * shared with other modules in hgc,  but not in other programs. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
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

#ifndef WIKITRACK_H
#include "wikiTrack.h"
#endif

#ifndef VARIOME_H
#include "variome.h"
#endif

#ifndef BEDDETAIL_H
#include "bedDetail.h"
#endif

#include "hgdpGeo.h"
#include "dnaMotif.h"
#include "togaClick.h"

#include "featureBits.h"

#include "snp125.h"


extern struct cart *cart;	/* User's settings. */
extern char *seqName;		/* Name of sequence we're working on. */
extern int winStart, winEnd;    /* Bounds of sequence. */
extern char *database;		/* Name of mySQL database. */
extern char *organism;		/* Colloquial name of organism. */
extern char *genome;		/* common name, e.g. Mouse, Human */
extern char *scientificName;	/* Scientific name of organism. */
extern struct hash *trackHash;	/* A hash of all tracks - trackDb valued */

// A helper struct for allowing variable sized user defined tables. Each table
// is encoded in one field of the bigBed with '|' as column separators and ';' as
// field separators.
struct embeddedTbl
{
    struct embeddedTbl *next; // the next custom table
    char *field; // field name from bigBed, used as title when title is NULL
    char *title; // title of the table from trackDb, may be NULL
    char *encodedTbl; // contents of field in bigBed
};

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

struct trackDb *tdbForTableArg();
/* get trackDb for track passed in table arg */

void writeFramesetType();
/* Write document type that shows a frame set, rather than regular HTML. */

void htmlFramesetStart(char *title);
/* Write DOCTYPE HTML and HEAD sections for framesets. */

struct psl *getAlignments(struct sqlConnection *conn, char *table, char *acc);
/* get the list of alignments for the specified acc */

void printAlignmentsSimple(struct psl *pslList, int startFirst, char *hgcCommand,
                           char *typeName, char *itemIn);
/* Print list of mRNA alignments, don't add extra textual link when
 * doesn't honor hgcCommand. */

void printAlignments(struct psl *pslList,
		     int startFirst, char *hgcCommand, char *typeName, char *itemIn);
/* Print list of mRNA alignments. */

void printAlignmentsExtra(struct psl *pslList,
		     int startFirst, char *hgcCommand, char *hgcCommandInWindow, char *typeName, char *itemIn);
/* Print list of mRNA alignments with special "in window" alignment function. */

void showSomeAlignment(struct psl *psl, bioSeq *oSeq,
		       enum gfType qType, int qStart, int qEnd,
		       char *qName, int cdsS, int cdsE);
/* Display protein or DNA alignment in a frame. */

void linkToOtherBrowserTitle(char *otherDb, char *chrom, int start, int end, char *title);
/* Make anchor tag to open another browser window with a title. */

void linkToOtherBrowser(char *otherDb, char *chrom, int start, int end);
/* Make anchor tag to open another browser window. */

void printEntrezGeneUrl(FILE *f, int geneid);
/* Print URL for Entrez browser on a gene details page. */

    void printEntrezPubMedUidUrl(FILE *f, int pmid);
/* Print URL for Entrez browser on a PubMed search. */

void printSwissProtAccUrl(FILE *f, char *accession);
/* Print URL for Swiss-Prot protein accession. */

boolean clipToChrom(int *pStart, int *pEnd);
/* Clip start/end coordinates to fit in chromosome. */

void printCappedSequence(int start, int end, int extra);
/* Print DNA from start to end including extra at either end.
 * Capitalize bits from start to end. */

void printPos(char *chrom, int start, int end, char *strand, boolean featDna,
	      char *item);
/* Print position lines.  'strand' argument may be null. */

void bedPrintPos(struct bed *bed, int bedSize, struct trackDb *tdb);
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

void genericBigWigClick(struct sqlConnection *conn, struct trackDb *tdb,
	char *item, int start);
/* Display details for BigWig  tracks. */

void bigWigCustomClick(struct trackDb *tdb);
/* Display details for BigWig custom tracks. */

void genericBigBedClick(struct sqlConnection *conn, struct trackDb *tdb,
		     char *item, int start, int end, int bedSize);

void doPubsDetails(struct trackDb *tdb, char *item);
/* Handle text2Genome track clicks in pubs.c */

/* Handle click in generic bigBed track. */

void bigBedCustomClick(struct trackDb *tdb);
/* Display details for BigWig custom tracks. */

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

void motifHitSection(struct dnaSeq *seq, struct dnaMotif *motif);
/* Print out section about motif. */

void motifMultipleHitsSection(struct dnaSeq **seqs, int count, struct dnaMotif *motif, char *title);
/* Print out section about motif, possibly with mutliple occurrences. */

void motifLogoAndMatrix(struct dnaSeq **seqs, int count, struct dnaMotif *motif);
/* Print out motif sequence logo and text (possibly with multiple occurences) */

struct dnaMotif *loadDnaMotif(char *motifName, char *motifTable);
/* Load dnaMotif from table. */

void doTriangle(struct trackDb *tdb, char *item, char *motifTable);
/* Display detailed info on a regulatory triangle item. */

void doTransRegCode(struct trackDb *tdb, char *item, char *motifTable);
/* Display detailed info on a transcriptional regulatory code item. */

void doTransRegCodeProbe(struct trackDb *tdb, char *item,
	char *codeTable, char *motifTable,
	char *tfToConditionTable, char *conditionTable);
/* Display detailed info on a CHIP/CHIP probe from transRegCode experiments. */

void doPeakClusters(struct trackDb *tdb, char *item);
/* Display detailed info about a cluster of peaks from other tracks. */

void doPeakClusterListItemsAssayed();
/* Put up a page that shows all experiments associated with a cluster track. */

void doFactorSource(struct sqlConnection *conn, struct trackDb *tdb, char *item, int start, int end);
/* Display detailed info about a cluster of peaks from other tracks. */

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

void offerLogin(int id, char *loginType, char *table);
/* display login prompts to the wiki when user isn't already logged in */

void outputJavaScript();
/* java script functions used in the create item form */

void doVariome (char *wikiItemId, char *chrom, int winStart, int winEnd);
/* handle item clicks on variome - may create new items */

void displayVariomeItem (struct variome *item, char *userName);
/* given an already fetched item, get the item description from
 *      the wiki.  Put up edit form(s) if userName is not NULL
 * separate from wikiTrack for form field differences and help
 */

void doCreateVariomeItem (char *itemName, char *chrom, int winStart, int winEnd);
/* handle create item clicks for variome */

void doAddVariomeComments(char *wikiItemId, char *chrom, int winStart, int winEnd);
/* handle add comment item clicks for Variome Track */

void doDeleteVariomeItem(char *wikiItemId, char *chrom, int winStart, int winEnd);
/* handle delete item clicks for Variome Track */

void printWikiVariomeForm (struct bedDetail *item);
/* print the wiki annotation form for the variome track */

void customMafClick(struct sqlConnection *conn,
	struct sqlConnection *conn2, struct trackDb *tdb);
/* handle clicks on a custom maf */

void doBigEncodePeak(struct trackDb *tdb, struct customTrack *cti, char *item);
/*  details for encodePeak type tracks.  */

void doEncodePeak(struct trackDb *tdb, struct customTrack *cti, char *item);
/*  details for encodePeak type tracks.  */

void doEncodeFiveC(struct sqlConnection *conn, struct trackDb *tdb);
/* Print details for 5C track */

void doPeptideMapping(struct sqlConnection *conn, struct trackDb *tdb, char *item);
/* Print details for a peptideMapping track.  */

void doHgdpGeo(struct trackDb *tdb, char *item);
/* Show details page for HGDP SNP with population allele frequencies
 * plotted on a world map. */

void hgdpGeoImg(struct hgdpGeo *geo);
/* Generate image as PNG, PDF, EPS: world map with pie charts for population allele frequencies. */

char *hgdpPngFilePath(char *rsId);
/* Return the stable PNG trash-cached image path for rsId. */

void hgdpGeoFreqTable(struct hgdpGeo *geo);
/* Print an HTML table of populations and allele frequencies. */

void printOtherSnpMappings(char *table, char *name, int start,
			   struct sqlConnection *conn, int rowOffset, struct trackDb *tdb);
/* If this SNP (from any bed4+ table) is not uniquely mapped, print the other mappings. */

void printCustomUrl(struct trackDb *tdb, char *itemName, boolean encode);
/* Wrapper to call printCustomUrlWithLabel using the url setting in trackDb */

void printCustomUrlWithFields(struct trackDb *tdb, char *itemName, char *itemLabel, boolean encode,                                                      struct slPair *fields);
/* Wrapper to call printCustomUrlWithLabel with additional fields to substitute */

void printOtherCustomUrl(struct trackDb *tdb, char *itemName, char* urlSetting, boolean encode);
/* Wrapper to call printCustomUrlWithLabel to use another url setting other than url in trackDb e.g. url2, this allows the use of multiple urls for a track
 *  to be set in trackDb. */

void printIdOrLinks(struct asColumn *col, struct hash *fieldToUrl, struct trackDb *tdb, char *idList);
/* if trackDb does not contain a "urls" entry for current column name, just print idList as it is.
 *  * Otherwise treat idList as a comma-sep list of IDs and print one row per id, with a link to url,
 *   * ($$ in url is OK, wildcards like $P, $p, are also OK)
 *    * */

void printDbSnpRsUrl(char *rsId, char *labelFormat, ...)
/* Print a link to dbSNP's report page for an rs[0-9]+ ID. */
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
    ;

void doBamDetails(struct trackDb *tdb, char *item);
/* Show details of an alignment from a BAM file. */

void doVcfTabixDetails(struct trackDb *tdb, char *item);
/* Show details of an alignment from a VCF file compressed and indexed by tabix. */

void doVcfTabixDetailsExt(struct trackDb *tdb, char *item, struct featureBits **pFbList, int start, int end);
/* Show details of an alignment from a VCF file compressed and indexed by tabix. */

void doVcfDetails(struct trackDb *tdb, char *item);
/* Show details of an alignment from an uncompressed VCF file. */

void doVcfDetailsExt(struct trackDb *tdb, char *item, struct featureBits **pFbList, int start, int end);
/* Show details of an alignment from an uncompressed VCF file. */

void doBarChartDetails(struct trackDb *tdb, char *item);
/* Details of barChart item */

void doMakeItemsDetails(struct customTrack *ct, char *itemIdString);
/* Show details of a makeItems item. */

void doBedDetail(struct trackDb *tdb, struct customTrack *ct, char *itemName);
/* generate the detail page for a custom track of bedDetail type */

void doPgSnp(struct trackDb *tdb, char *itemName, struct customTrack *ct);
/* print detail page for personal genome track (pgSnp) */

void doGvf(struct trackDb *tdb, char *item);
/* Show details for variants represented as GVF, stored in a bed8Attrs table */

void doGeneReviews(struct trackDb *tdb, char *itemName);
/* generate the detail page for geneReviews */

void prGeneReviews(struct sqlConnection *conn, char *itemName);
/* print GeneReviews associated to this item */

void prGRShortRefGene(char *itemName);
/* print GeneReviews short label associated to this refGene item */

void doLrg(struct trackDb *tdb, char *item);
/* Locus Reference Genomic (LRG) info. */

void doLrgTranscriptPsl(struct trackDb *tdb, char *item);
/* Locus Reference Genomic (LRG) transcript mapping and sequences. */

void htcLrgCdna(char *item);
/* Serve up LRG transcript cdna seq */

void doPeptideAtlas(struct trackDb *tdb, char *item);
/* Details for PeptideAtlas peptide mapping */

void doGtexGeneExpr(struct trackDb *tdb, char *item);
/* Details of GTEX gene expression item */

void doGtexEqtlDetails(struct trackDb *tdb, char *item);
/* Details of GTEx eQTL item */

void printSnp125Function(struct trackDb *tdb, struct snp125 *snp);
/* If the user has selected a gene track for functional annotation,
 * report how this SNP relates to any nearby genes. */

void printSnp153Function(struct trackDb *tdb, struct snp125 *snp);
/* If the user has selected a gene track for functional annotation,
 * report how this SNP relates to any nearby genes. */

void doBigDbSnp(struct trackDb *tdb, char *rsId);
/* Show details for bigDbSnp item. */

void printAddWbr(char *text, int distance);
/* a crazy hack for firefox/mozilla that is unable to break long words in tables
 * We need to add a <wbr> tag every x characters in the text to make text breakable.
 */

void printIframe(struct trackDb *tdb, char *itemName);
/* print an iframe with the URL specified in trackDb (iframeUrl), can have
 * the standard codes in it (like $$ for itemName, etc) */

char *getIdInUrl(struct trackDb *tdb, char *itemName);
/* If we have an idInUrlSql tag, look up itemName in that, else just
 * return itemName. */

void printFieldLabel(char *entry);
/* print the field label, the first column in the table, as a <td>. Allow a
 * longer description after a |-char, as some fields are not easy to
 * understand. */

struct slPair* getExtraFields(struct trackDb *tdb, char **fields, int fieldCount);
/* return the extra field names and their values as a list of slPairs */

struct slPair *getFields(struct trackDb *tdb, char **fields);
/* return field names and their values as a list of slPairs.  */

void printEmbeddedTable(struct trackDb *tdb, struct embeddedTbl *embeddedTblList,
                        struct dyString *tableLabelsDy);
/* Pretty print a '|' and ';' encoded table or a JSON encoded table from a bigBed field.
 * The JSON encoded tables get passed through as json so hgc.js can build them instead,
 * which preserves the table order */

void getExtraTableFields(struct trackDb *tdb, struct slName **retFieldNames, struct embeddedTbl **retEmbeddedTblList, struct hash *fieldsToEmbeddedTbl);
/* Parse the trackDb field "extraTableFields" into the field names and titles specified,
 * and fill out a hash keyed on the bigBed field name (which may be in an external file
 * and not in the bigBed itself) to a helper struct for storing user defined tables. */

int extraFieldsPrintAs(struct trackDb *tdb,struct sqlResult *sr,char **fields,int fieldCount, struct asObject *as);
// Any extra bed or bigBed fields (defined in as and occurring after N in bed N + types.
// sr may be null for bigBeds.
// Returns number of extra fields actually printed.

int extraFieldsPrint(struct trackDb *tdb,struct sqlResult *sr,char **fields,int fieldCount);
// Any extra bed or bigBed fields (defined in as and occurring after N in bed N + types.
// sr may be null for bigBeds.
// Returns number of extra fields actually printed.

struct slPair *parseDetailsTablUrls(struct trackDb *tdb);
/* Parse detailsUrls setting string into an slPair list of {offset column name, fileOrUrl} */

char *readOneLineMaybeBgzip(char *fileOrUrl, bits64 offset, bits64 len);
/* If fileOrUrl is bgzip-compressed and indexed, then use htslib's bgzf functions to
 * retrieve uncompressed data from offset; otherwise (plain text) use udc. If len is 0,
 * read up to next '\n' delimiter. */

#define NUCCORE_SEARCH "https://www.ncbi.nlm.nih.gov/sites/entrez?db=nuccore&cmd=search&term="

void doJRepeat (struct trackDb *tdb, char *repeat);
void doBigRmskRepeat (struct trackDb *tdb, char *repeat);
/* New RepeatMasker Visualization defined in joinedRmskClick.c */

INLINE char* strOrNbsp(char* val)
/* return val if not empty otherwise HTML entity &nbsp; */
{
return isEmpty(val) ? "&nbsp;" : val;
}

void doInteractDetails(struct trackDb *tdb, char *item);
/* Details of interaction item */

void doParDetails(struct trackDb *tdb, char *name);
/* show details of a PAR item. */


struct psl *getPslAndSeq(struct trackDb *tdb, char *chromName, struct bigBedInterval *bb, 
    unsigned seqTypeField, DNA **retSeq, char **retCdsStr);
/* Read in psl and query sequence out of a bbiInverval on a give chromosome */
#endif
