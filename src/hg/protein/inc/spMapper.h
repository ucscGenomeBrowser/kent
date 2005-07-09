/* spMapper - map swissprot accessions to KnownGene ids.  Intended to use in
 * building gene sorter tables.  Can optionally map uniref entry ids. */
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
};
  
struct spMapperId
/* a reference to id */
{
    struct spMapperId *next;
    char *id;                   /* id (memory not owned) */
};

struct spMapperPairs
/* query and target id pairs; used to avoid duplication of entries. */
{
    struct spMapperPairs *next;
    char *qId;                  /* query id (memory not owned) */
    struct spMapperId *tIds;   /* target ids paired with this query */
};

struct spMapper *spMapperNew(struct sqlConnection *conn,
                             char *unirefFile, char *orgFilter);
/* construct a new spMapper object. If unirefFile is not null, this is a tab
 * file reformatting of the uniref xml file with member data extracted (see
 * uniref.h).  If orgFilter is not null, then only uniref members from
 * this organism are loaded.
 */

void spMapperFree(struct spMapper **smPtr);
/* free smMapper object */

struct spMapperPairs *spMapperMapPair(struct spMapper *sm, char *qSpId, char *tSpId);
/* map a pair of swissprot/uniprot ids to one or more pairs of known gene
 * ids. */


void spMapperPairsFree(struct spMapperPairs **head);
/* free list of spMapperPairs objects */

void spMapperPrintNoMapInfo(struct spMapper *sm, char *outFile);
/* Print missed id table. First column is id, second column is R if it wasn't
 * found in uniref, or K if it couldn't be associated with a known gene */


#endif
