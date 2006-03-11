/* Database table that tracks genbank/refseq version information for each
 * accession.  This is used only by the genbank/refseq update process. */
#ifndef GBSTATUSTBL_H
#define GBSTATUSTBL_H

#include "common.h"
#include "hgRelate.h"
#include "genbank.h"

/* Flags indicating used indiucate state changes */
#define GB_NOCHG     0x00    /* no change */
#define GB_NEW       0x01    /* new entry */
#define GB_DELETED   0x02    /* deleted entry */
#define GB_SEQ_CHG   0x04    /* sequence has changed */
#define GB_META_CHG  0x08    /* metadata has changed */
#define GB_EXT_CHG   0x10    /* extFile has changed */
#define GB_ORPHAN    0x20    /* orphaned (normally GB_NEW as well) */


struct gbStatus
/* genbank version information for an entry. There will be only one
 * entry per accession, indicating the version loaded into the database. */
{
    struct gbStatus* next;
    char* acc;               /* accession */
    int version;             /* version number */
    time_t modDate;          /* modification date */
    UBYTE type;              /* GB_MRNA or GB_EST */
    UBYTE srcDb;             /* GB_GENBANK or GB_REFSEQ */
    UBYTE orgCat;            /* GB_NATIVE or GB_XENO */
    HGID gbSeqId;            /* id in gbSeq table */
    unsigned numAligns;      /* number of alignments in table */
    char *seqRelease;        /* release version where sequence was obtained */
    char *seqUpdate;         /* update where sequence was obtained
                              * (date or 'full') */
    char *metaRelease;       /* release version where metadata was obtained */
    char *metaUpdate;        /* update where metadata was obtained
                              * (date or full) */
    char *extRelease;        /* release version containing external file */
    char *extUpdate;         /* update containing external file */
    time_t time;             /* time entry was inserted/updated */

    /* These fields are not loaded from the database, they are here for use
     * in the loading process */
    struct gbEntry* entry;              /* entry in genbank index */
    struct gbProcessed* selectProc;     /* selected processed obj */
    struct gbAligned* selectAlign;      /* selected aligned obj */
    UWORD stateChg;                     /* set of state change flags */
    boolean metaDone;                   /* add to metadata tables  */
    struct genbankCds cds;              /* CDS from genbank */    
    char *geneName;                     /* gene name for refSeq */
};

struct gbStatusTbl
/* Object to managing memory copy of genbank version table. */
{
    struct hash *accHash;   /* hash on accession, hash localmem is used
                             * for struct allocations as well */
    struct hash *strPool;   /* Table for allocating shared strings */
    char tmpDir[PATH_LEN];  /* tmp directory for db loads */
    boolean verbose;
    unsigned numEntries;    /* number of entries */
    boolean extFileUpdate;  /* should old extFiles references be updated?  */

    /* Lists that are built of entries in particular states.  Note that
     * more that one state maybe indciated in stateChg field, but it's
     * only on one of the Lists that are */
    struct gbStatus* deleteList;    /* delete seqs */
    unsigned numDelete;
    struct gbStatus* seqChgList;    /* changed seqs (and meta) */
    unsigned numSeqChg;
    struct gbStatus* metaChgList;   /* meta info change, but not seq */
    unsigned numMetaChg;
    struct gbStatus* extChgList;    /* external file release changed */
    unsigned numExtChg;
    struct gbStatus* newList;       /* new seqs (to be added to table) */
    unsigned numNew;
    struct gbStatus* orphanList;    /* new entries found in seq table */
    unsigned numOrphan;
    unsigned numNoChg;
};

/* name of status table */
extern char* GB_STATUS_TBL;

typedef void gbStatusLoadSelect(struct gbStatusTbl* statusTbl,
                                struct gbStatus* tmpStatus,
                                void* clientData);
/* Type of function that is called when selectively loading
 * entries from the table.  Selective loading is used to reduce
 * memory on incremental updates.  If the entry should be stored
 * in the table, this function should should call gbStatusStore.
 * NOTE: that the tmpStatus parameter is a temporary copy of the
 * status and should not be modified.  Use the entry returend by
 * gbStatusStore to set flags. */


struct gbStatusTbl* gbStatusTblSelectLoad(struct sqlConnection *conn,
                                          unsigned select, char* accPrefix,
                                          gbStatusLoadSelect* selectFunc,
                                          void* clientData, char* tmpDir,
                                          boolean extFileUpdate, boolean verbose);
/* Selectively load the status table file table from the database, creating
 * table if it doesn't exist.  This calls selectFunc on each entry that is
 * found.  See gbStatusLoadSelect for details.
 */

struct gbStatus* gbStatusStore(struct gbStatusTbl* statusTbl,
                               struct gbStatus* tmpStatus);
/* Function called to store status on selective load.  Returns the
 * entry that was added to the table. */

struct gbStatusTbl *gbStatusTblLoad(struct sqlConnection *conn,
                                    unsigned select, char* accPrefix,
                                    char* tmpDir, boolean extFileUpdate, 
                                    boolean verbose);
/* Load the file table from the database, creating table if it doesn't exist.
 * The select flags are some combination of GB_MRNA,GB_EST,GB_GENBANK,
 * GB_REFSEQ.  If accPrefix is not null, it is the first two characters of the
 * accessions to select. */

char* gbStatusTblGetStr(struct gbStatusTbl *statusTbl, char *str);
/* allocate a unique string for the table */

struct gbStatus *gbStatusTblAdd(struct gbStatusTbl *statusTbl, char* acc,
                                int version, time_t modDate,
                                unsigned type, unsigned srcDb, unsigned orgCat,
                                HGID gbSeqId, unsigned numAligns,
                                char *relVersion, char *update, time_t time);
/* create and add a new entry. */

void gbStatusTblRemoveDeleted(struct gbStatusTbl *statusTbl,
                              struct sqlConnection *conn);
/* Remove sequences marked as deleted in status table. They are not removed
 * from the in memory table, but will have their gbSeqId zeroed. */

struct gbStatus *gbStatusTblFind(struct gbStatusTbl *statusTbl, char *acc);
/* Lookup a entry in the table, return NULL if it doesn't exist */

struct sqlUpdater* gbStatusTblUpdate(struct gbStatusTbl *statusTbl,
                                     struct sqlConnection *conn,
                                     boolean commit);
/* Update the database with to reflect the changes in the status table entries
 * flaged as isNew, isSeqChanged, or isMetaChanged.  If commit is true, commit
 * the changes and return null.  If it is false, return the sqlUpdater with
 * the changes ready to for commit */

void gbStatusTblFree(struct gbStatusTbl** statusTbl);
/* Free a gbStatusTbl object */

struct slName* gbStatusTblLoadAcc(struct sqlConnection *conn, 
                                  unsigned select, char* accPrefix);
/* load a list of accession from the table matching the selection
 * criteria */
#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
