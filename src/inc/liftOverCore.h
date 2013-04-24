/* the basic routine of liftover mapping separated from the other liftOver routines */
/* in src/hg that are more geared for the browser and need MySQL */

#ifndef LIFTOVERCORE_H
#define LIFTOVERCORE_H

#ifndef BASICBED_H
#include "basicBed.h"
#endif

#ifndef CHAIN_H 
#include "chain.h"
#endif

#define LIFTOVER_MINMATCH        0.95
#define LIFTOVER_MINBLOCKS       1.00
// The maximum number of words per line that can be lifted:
#define LIFTOVER_MAX_WORDS 64

struct liftOverChromMap
/* Remapping information for one (old) chromosome */
    {
    char *name;                 /* Chromosome name. */
    struct binKeeper *bk;       /* Keyed by old position, values are chains. */
    };

char *remapRangeCore(struct hash *chainHash, double minRatio, 
		     int minSizeT, int minSizeQ, 
		     int minChainSizeT, int minChainSizeQ, 
		     char *chrom, int s, int e, char qStrand,
		     int thickStart, int thickEnd, bool useThick,
		     double minMatch,
		     char *regionName, char *db, char *chainTableName,
		     struct chain *(*chainTableCallBack)(char *db, char *chainTableName, char *chrom, int start, int end, int id), 
		     struct bed **bedRet, struct bed **unmappedBedRet);
/* Remap a range through chain hash.  If all is well return NULL
 * and results in a BED (or a list of BED's, if regionName is set (-multiple).
 * Otherwise return a string describing the problem. 
 * note: minSizeT is currently NOT implemented. feel free to add it.
 *       (its not e-s, its the boundaries of what maps of chrom:s-e to Q)
 */

#endif /* LIFTOVERCORE_H */
