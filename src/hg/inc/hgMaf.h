/* hgMaf.h - Stuff to load up mafs from the browser database. 
 *           Also, items for maf track display */

#include "trackDb.h"
#include "cart.h"

/* Track settings and variables */
#define SPECIES_TREE_VAR        "speciesTree"
#define SPECIES_ORDER_VAR       "speciesOrder"
#define SPECIES_GROUP_VAR       "speciesGroups"
#define SPECIES_TARGET_VAR      "speciesTarget"
#define SPECIES_DEFAULT_OFF_VAR "speciesDefaultOff"
#define SPECIES_GROUP_PREFIX    "sGroup_"
#define SPECIES_HTML_TARGET	"sT"
#define SPECIES_CODON_DEFAULT	"speciesCodonDefault"
#define SPECIES_USE_FILE        "speciesUseFile"
#define PAIRWISE_VAR            "pairwise"
#define PAIRWISE_HEIGHT         "pairwiseHeight"
#define SUMMARY_VAR             "summary"
#define BASE_COLORS_VAR         "baseColors"
#define BASE_COLORS_OFFSET_VAR  "baseColorsOffset"
#define CONS_WIGGLE             "wiggle"
#define ITEM_FIRST_CHAR_CASE	"itemFirstCharCase"
#define DEFAULT_CONS_LABEL      "Conservation"

#define gsidSubjList "gsidTable.gsidSubjList"
#define gsidSeqList "gsidTable.gsidSeqList"

struct mafAli *mafLoadInRegion2(struct sqlConnection *conn, 
    struct sqlConnection *conn2, char *table, char *chrom, 
    int start, int end, char *file);
/* Return list of alignments in region. */

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

struct consWiggle {
    struct consWiggle *next;    /* Next in list */
    char *table;                /* phastCons table */
    char *leftLabel;            /* Left label for hgTracks */
    char *uiLabel;              /* Label to print on trackUi */
};

struct consWiggle *wigMafWiggles(char *db, struct trackDb *tdb);
/* get conservation wiggle table names and labels from trackDb setting,
   ignoring those where table doesn't exist */

char *wigMafWiggleVar(struct trackDb *tdb, struct consWiggle *wig);
/* Return name of cart variable for this cons wiggle */

struct wigMafSpecies 
    {
    struct wigMafSpecies *next;
    char *name;
    int group;
    boolean on;
    };

struct wigMafSpecies * wigMafSpeciesTable(struct cart *cart, 
    struct trackDb *tdb, char *name, char *db) ;
char **wigMafGetSpecies(struct cart *cart, struct trackDb *tdb, char *db, struct wigMafSpecies **list, int *groupCt);
