/* quickLift genome annotations on the fly between assemblies using chain files */

/* Copyright (C) 2023 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef QUICKLIFT_H      
#define QUICKLIFT_H      

#define quickLiftCartName     "hubQuickLift"

struct quickLiftRegions
// store highlight information
{
struct quickLiftRegions *next;
unsigned type;
long chromStart;
long chromEnd;
long oChromStart;
long oChromEnd;
char *otherBases;
unsigned otherCount;
char * id;
};

#define QUICKTYPE_INSERT     0
#define QUICKTYPE_DEL      1
#define QUICKTYPE_DOUBLE     2
#define QUICKTYPE_MISMATCH     3
#define QUICKTYPE_NOTHING     4

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

struct slList *quickLiftSql(struct sqlConnection *conn, char *quickLiftFile, char *table, char *chromName, int winStart, int winEnd,  char *query, char *extraWhere, ItemLoader2 loader, int numFields, struct hash *chainHash);
/* Load a list of items (usually beds) from another database in a region that corresponds to chromName:winStart-winEnd in the reference database.
 * Fill a hash with the chains that were used to map the desired range.  These chains will be used to map the query side items back to the reference. */

unsigned quickLiftGetChain(char *fromDb, char *toDb);
/* Return the id from the quickLiftChain table for given assemblies. */

struct bed *quickLiftBeds(struct bed *bedList, struct hash *chainHash, boolean blocked);
// Map a list of bedd in query coordinates to our current reference

boolean quickLiftEnabled();
/* Return TRUE if feature is available */

struct quickLiftRegions *quickLiftGetRegions(char *ourDb, char *liftDb, char *quickLiftFile, char *chrom, int seqStart, int seqEnd);
/* Figure out the highlight regions and cache them. */

#endif
