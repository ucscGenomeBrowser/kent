/* hgMaf.h - Stuff to load up mafs from the browser database. 
 *           Also, items for maf track display */

/* Track settings and variables */
#define SPECIES_TREE_VAR        "speciesTree"
#define SPECIES_ORDER_VAR       "speciesOrder"
#define SPECIES_GROUP_VAR       "speciesGroups"
#define SPECIES_TARGET_VAR      "speciesTarget"
#define SPECIES_DEFAULT_OFF_VAR "speciesDefaultOff"
#define SPECIES_GROUP_PREFIX    "sGroup_"
#define SPECIES_HTML_TARGET	"sT"
#define SPECIES_CODON_DEFAULT	"speciesCodonDefault"
#define PAIRWISE_VAR            "pairwise"
#define PAIRWISE_HEIGHT         "pairwiseHeight"
#define SUMMARY_VAR             "summary"
#define BASE_COLORS_VAR         "baseColors"
#define BASE_COLORS_OFFSET_VAR  "baseColorsOffset"
#define CONS_WIGGLE             "wiggle"

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

int mafCmp(const void *va, const void *vb);
/* Compare to sort based on start of first component. */
