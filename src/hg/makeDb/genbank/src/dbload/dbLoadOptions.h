/* option shared by all dbload modules */
#ifndef GBLOADOPTIONS_H
#define GBLOADOPTIONS_H

#include "gbDefs.h"
struct gbIndex;

#define DBLOAD_INITIAL           0x01   /* initial load of this databases */
#define DBLOAD_GO_FASTER         0x02   /* optimize speed over memory */
#define DBLOAD_LARGE_DELETES     0x04   /* don't enforce fraction deleted */
#define DBLOAD_DRY_RUN           0x08   /* Don't modify database */
#define DBLOAD_PER_CHROM_ALIGN   0x10   /* build per-chromosome alignment tables */
#define DBLOAD_EXT_FILE_UPDATE   0x20   /* update gbSeq to link to the latest
                                         * latest release's files. */
#define DBLOAD_BYPASS_GBLOADED   0x40   /* skip gbLoaded table optimizations,
                                         * needed if native or xeno partation is
                                         * added*/

struct dbLoadAttr
/* attributes associated with a srcDb+type+orgCat */
{
    boolean load;      /* should be loaded? */
    boolean loadDesc;  /* should descriptions be loaded? */
};

/* attribute array, indexed by [srcDbIdx][typeDbIdx][orgCatIdx] */
typedef struct dbLoadAttr dbLoadAttrArray[GB_NUM_SRC_DB][GB_NUM_TYPES][GB_NUM_ORG_CATS];

struct dbLoadOptions
/* Options for load. Option arrays are indexed by (srcDb*type*orgCat).
 * contents are take from both command line and options file */
{
    char* relRestrict;                         /* release restriction */
    char* accPrefixRestrict;                   /* acc prefix restriction */
    unsigned flags;                            /* above flags */
    unsigned maxExtFileUpdate;                 /* maximum number of entries
                                                * to update gbSeq/gbExtFile 
                                                * data for */
    dbLoadAttrArray loadAttr;                  /* should these be loaded? */
};

struct dbLoadOptions dbLoadOptionsParse(char* db);
/* parse many of the command line options and the options file. */

struct dbLoadAttr* dbLoadOptionsGetAttr(struct dbLoadOptions* options,
                                        unsigned srcDb, unsigned type,
                                        unsigned orgCat);
/* get a pointer the load attributes */
#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
