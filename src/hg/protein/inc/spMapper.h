/* spMapper - map swissprot accessions to KnownGene ids.  Intended to use in
 * building gene sorter tables.  Can optionally map uniref entry ids.  Used
 * to collect results so that duplication of entries can be avoided.  */
#ifndef SPMAPPER_H
#define SPMAPPER_H

struct sqlConnection *conn;

struct spMapper
/* mapper object */
{
    struct spKgMap *spKgMap;      /* hash of swissprot id to kg id. */
    struct unirefTbl *unirefTbl;  /* table of uniref entries. */
    struct hash *noMapTbl;        /* table of ids that could not be mapped,
                                   * value is code indicating the reasons */

    /* counts of ids that count not be found, only counted once */
    int noSpIdMapCnt;       /* couldn't map a sp id */
    int noUnirefMapCnt;     /* couldn't map a uniref entry id */

    /* counts of swissprot query/target pair mappings */
    int qtMapCnt;           /* pairs successfuly mapped */
    int qtNoSpIdMapCnt;     /* pairs not mapped due to no sp mapping */
    int qtNoUnirefMapCnt;   /* pairs not mapped due to no uniref entry mapping */

    int scoreDir;           /* 1 if higher score is better, -1 if lower score
                             * is better */

    /* hash table of best score for query and target ids.  A naive
     * implementation would have the pair as a single string, but this greatly
     * increase the memory usage.  Instead, both are entered separately, point
     * to the same struct. */
    struct hash *kgPairMap;      /* hash to kgEntry objects, which are chained
                                  * together for a given id */
    struct kgPair *kgPairList;   /* list of all spPair objects. */
};

struct kgEntry
/* struct used to store a known gene id with a link to the next entry
 * and the associated spPairScore struct. */
{
    struct kgEntry *next;
    char *id;                /* kd id, memory not owned by this struct */
    struct kgPair *kgPair;   /* containing shared score */
};

struct kgPair
/* structure used to keep the best e-value of a pair of kg ids. */
{
    struct kgPair *next;
    struct kgEntry *kg1Entry;   /* pointers back to each entry */
    struct kgEntry *kg2Entry;
    float score;                 /* best score for the pair */
};

  
struct spMapper *spMapperNew(struct sqlConnection *conn, int scoreDir,
                             char *unirefFile, char *orgFilter);
/* construct a new spMapper object. scoreDir is 1 if higher score is better,
 * -1 if lower score is better If unirefFile is not null, this is a tab file
 * reformatting of the uniref xml file with member data extracted (see
 * uniref.h).  If orgFilter is not null, then only uniref members from this
 * organism are loaded.
 */

void spMapperFree(struct spMapper **smPtr);
/* free smMapper object */

void spMapperMapPair(struct spMapper *sm, char *qSpId, char *tSpId, float score);
/* map a pair of swissprot/uniprot ids to one or more pairs of known gene
 * ids and add them to the table */

void spMapperPrintNoMapInfo(struct spMapper *sm, char *outFile);
/* Print missed id table. First column is id, second column is R if it wasn't
 * found in uniref, or K if it couldn't be associated with a known gene */


#endif
