/* hgMaf.c - Stuff to load up mafs from the browser database. */

struct mafAli *mafLoadInRegion(struct sqlConnection *conn, char *table,
	char *chrom, int start, int end);
/* Return list of alignments in region. */

struct mafAli *axtLoadAsMafInRegion(struct sqlConnection *conn, char *table,
	char *chrom, int start, int end, 
	char *tPrefix, char *qPrefix, int tSize,  struct hash *qSizeHash);
/* Return list of alignments in region from axt external file as a maf. */

struct mafAli *hgMafFrag(
	char *database,     /* Database, must already have hSetDb to this */
	char *track, 	    /* Name of MAF track */
	char *chrom, 	    /* Chromosome (in database genome) */
	int start, int end, /* start/end in chromosome */
	char strand, 	    /* Chromosome strand. */
	char *outName, 	    /* Optional name to use in first component */
	struct slName *orderList /* Optional order of organisms. */
	);
/* mafFrag- Extract maf sequences for a region from database.  
 * This creates a somewhat unusual MAF that extends from start
 * to end whether or not there are actually alignments.  Where
 * there are no alignments (or alignments missing a species)
 * a . character fills in.   The score is always zero, and
 * the sources just indicate the species.  You can mafFree this
 * as normal. */

