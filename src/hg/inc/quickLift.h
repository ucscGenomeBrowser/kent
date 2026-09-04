/* quickLift genome annotations on the fly between assemblies using chain files */

/* Copyright (C) 2023 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef QUICKLIFT_H      
#define QUICKLIFT_H      

#define quickLiftCartName     "hubQuickLift"

#define quickLiftChainTableConfVariable      "quickLiftChainName"
#define defaultQuickLiftChainTableName       "quickLiftChain"

struct psl;
struct chain;
struct mafAli;

struct quickLiftRegions
// store highlight information
{
struct quickLiftRegions *next;
unsigned type;
char *chrom;
long chromStart;
long chromEnd;
char *bases;
unsigned baseCount;
char *oChrom;
long oChromStart;
long oChromEnd;
char *otherBases;
unsigned otherBaseCount;
char * id;
};

#define QUICKTYPE_INSERT     0
#define QUICKTYPE_DEL      1
#define QUICKTYPE_DOUBLE     2
#define QUICKTYPE_MISMATCH     3

extern char *quickTypeStrings[];

typedef struct slList *(*ItemLoader2)(char **row, int numFields);
/* Load a bed file from an SQL query result. */

struct bigBedInterval *quickLiftGetIntervals(char *instaPortFile, struct bbiFile *bbi,   char *chrom, int start, int end, struct hash **pChainHash);
/* Return intervals from "other" species that will map to the current window.
 * These intervals are NOT YET MAPPED to the current assembly.
 */

struct bed *quickLiftIntervalsToBed(struct bbiFile *bbi, struct hash *chainHash, struct bigBedInterval *bb);
/* Using chains stored in chainHash, port a bigBedInterval from another assembly to a bed
 * on the reference.
 */

struct bed *quickLiftIntervalsToBedClip(struct bbiFile *bbi, struct hash *chainHash, struct bigBedInterval *bb);
/* Like quickLiftIntervalsToBed, but an item too big for the chains we loaded is pulled in
 * to what they cover rather than dropped.  Callers that need the item's true extent (the
 * details page) should use quickLiftIntervalsToBed instead. */

struct slList *quickLiftSql(struct sqlConnection *conn, char *quickLiftFile, char *table, char *chromName, int winStart, int winEnd,  char *query, char *extraWhere, ItemLoader2 loader, int numFields, struct hash *chainHash);
/* Load a list of items (usually beds) from another database in a region that corresponds to chromName:winStart-winEnd in the reference database.
 * Fill a hash with the chains that were used to map the desired range.  These chains will be used to map the query side items back to the reference. */

struct genePred *quickLiftGenePreds(struct sqlConnection *conn, char *quickLiftFile, char *table, char *chromName, int winStart, int winEnd, char *extraWhere, struct hash *chainHash);
/* Like quickLiftSql, but load genePreds through a genePredReader so the actual set of
 * (extended) genePred columns in the table is honored rather than assuming 15 columns.
 * Fill a hash with the chains that were used to map the desired range. */

unsigned quickLiftGetChainId(struct cart *, char *fromDb, char *toDb);
/* Return the id from the quickLiftChain table for given assemblies. */

char *quickLiftGetChainPath(struct cart *, char *fromDb, char *toDb);
/* Return the path from the quickLiftChain table for given assemblies. */

struct bed *quickLiftBeds(struct bed *bedList, struct hash *chainHash, boolean blocked);
// Map a list of bedd in query coordinates to our current reference

struct quickLiftRange
// A range in the other assembly that some part of the reference window maps back to.
{
struct quickLiftRange *next;
char *chrom;            /* sequence name in the other assembly */
int start;
int end;
};

struct quickLiftRange *quickLiftSourceRanges(char *quickLiftFile, char *chrom, int start, int end,
    struct hash *chainHash);
// The ranges in the other assembly that map into chrom:start-end on the reference.  The
// chains that do the mapping are added to chainHash, which is the form the lift functions
// read.  Use this when the items cannot be had from a query quickLiftSql knows how to make.

struct hash *quickLiftChainHash(char *quickLiftFile, char *chrom, int start, int end);
// Load the quickLift chains covering chrom:start-end on the reference and return them in a
// hash keyed on the other assembly's sequence names, which is the shape the lift functions
// want.  Use this when the items were fetched some other way, so quickLiftSql was not the
// thing that collected the chains.

struct psl *quickLiftPsl(struct hash *chainHash, struct hash **pMapPsls, struct psl *psl);
// Map the target side of an alignment from the other assembly onto our current reference.
// The query side (the mRNA, EST or protein the alignment is to) is left alone.  Returns
// NULL if the alignment doesn't map.  pMapPsls points at a hash of mapping alignments the
// caller keeps across a run of items; point it at a NULL hash to start.

struct mafAli *quickLiftMafs(struct hash *chainHash, struct mafAli *mafList,
    char *sourceDb, char *refSrc, int refSrcSize);
// Map MAF blocks from the other assembly onto our current reference.  A block is cut at
// every chain block boundary, since a MAF block has to be one contiguous run on its first
// row and the lift does not keep the reference contiguous.  refSrc is the name the browser
// expects on the reference row, "<db>.<chrom>", with no hub prefix.

boolean quickLiftIsOwnChainTrack(struct trackDb *tdb);
// TRUE when this is the chain track quickLift builds to show the lift itself.  That stanza
// carries quickLiftUrl and quickLiftDb like any lifted track, but its data is already in
// reference coordinates and must not be lifted a second time.

struct chain *quickLiftChain(struct hash *chainHash, struct hash **pMapPsls,
    struct chain *chain);
// Map a chain's target side from the other assembly onto our current reference.  A chain is
// an alignment between that assembly and some other species, so this composes the two and
// leaves a chain between the reference and that species.  The query side is left alone.
// Returns NULL if the chain doesn't map.  The chain handed in is not modified.

struct psl *quickLiftPsls(struct hash *chainHash, struct psl *pslList);
// Map a list of alignments in the other assembly's coordinates onto our current reference.
// Alignments that don't map are dropped.

struct encodePeak *quickLiftPeaks(struct encodePeak *peakList, struct hash *chainHash);
// Map a list of encodePeaks in query coordinates to our current reference.  These can't go
// through quickLiftBeds:  the thickStart and thickEnd it assigns overlay signalValue and
// pValue in struct encodePeak.

boolean quickLiftEnabled(struct cart *cart);
/* Return TRUE if feature is available */

struct quickLiftRegions *quickLiftGetRegions(char *ourDb, char *liftDb, char *quickLiftFile, char *chrom, int seqStart, int seqEnd);
/* Figure out the highlight regions and cache them. */

char *quickLiftChainTable();
/* Return the name of the quickLiftChain table. */

void quickLiftResolveTable(struct trackDb *tdb, char *trackTable, char **retTable, char **retLiftDb);
/* Resolve the table name and liftDb for a quickLift track.  For custom tracks,
 * sets *retLiftDb to CUSTOM_TRASH and *retTable to the dbTableName setting;
 * otherwise sets *retTable to trackTable. Caller should have already set
 * *retLiftDb to trackDbSetting(tdb, "quickLiftDb"). */

struct bed *quickLiftSqlLoadBeds(struct trackDb *tdb, char *trackTable, char *liftDb,
    char *chrom, int start, int end, char *extraWhere,
    ItemLoader2 loader, int numFields, boolean blocked);
/* Load items from another assembly via quickLift SQL, map them back to the reference,
 * and return the lifted beds.  Handles custom track table resolution internally.
 * Caller provides liftDb from trackDbSetting(tdb, "quickLiftDb"). */

boolean quickLiftLiftPos(char *sourceDb, char *destDb,
    char *chrom, int start, int end,
    char **retChrom, int *retStart, int *retEnd);
/* Map a position from source (sourceDb) coords to destination (destDb) coords
 * using the liftOver chain for sourceDb -> destDb.  Used to remap hgFind
 * results from quickLifted bigBed tracks back to the destination assembly. */

boolean quickLiftHubRemoveTrack(struct cart *cart, char *sourceDb, char *trackName);
/* Remove a track stanza from the quickLift hub file for sourceDb.  Returns
 * TRUE if a stanza matching trackName was found and removed. */
#endif
