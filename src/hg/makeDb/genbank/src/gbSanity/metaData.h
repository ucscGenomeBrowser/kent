/* Object to store metadata from database for use in verification, also
 * a few shared functions. */
#ifndef METADATA_H
#define METADATA_H

#include "common.h"
#include "hgRelate.h"
#include "gbDefs.h"

struct metaData
/* Combined metadata for an accession.  Record is fixed-length.
 * to make it easy to store in DBM. */
{
    char acc[GB_ACC_BUFSZ];
    unsigned typeFlags; /* srcDb guessed from accession */

    /* Fields indicating the tables were we found the accession.  Data
     * below, booleans are together to pack. */
    boolean inGbCdnaInfo;
    boolean inSeq;
    boolean inExtFile;
    boolean inRefSeqStatus;
    boolean inRefLink;
    boolean inGbStatus;
    boolean inGbIndex;     /* genbank flat-file indices */
    boolean inGbAlign;     /* has aligned entry */

    /* refSeq protein-specific */
    boolean hasProt;
    boolean protInSeq;
    boolean protInExtFile;

    /* set from gbIndex */
    boolean isNative;

    /* Should this be excluded from the database?  RefSeq non-native are
     * not usually loaded into the DB. */
    boolean excluded;

    /* fields from the gbCdnaInfo table */
    HGID gbCdnaInfoId;
    unsigned gbCdnaInfoVersion;
    time_t gbCdnaInfoModdate;
    unsigned gbCdnaInfoType;
    boolean haveDesc;

    /* fields from the seq table */
    unsigned seqSize;

    /* fieldsfrom refLinkTable */
    char rlName[64];
    char rlProtAcc[GB_ACC_BUFSZ];

    /* fields from the gbStatus table */
    unsigned gbsVersion;
    time_t gbsModDate;
    unsigned gbsType;
    unsigned gbsSrcDb;
    unsigned gbsOrgCat;
    unsigned gbsNumAligns;

    /* Count of alignments found */
    unsigned numAligns;
};

struct metaDataTbls;
/* Object with metadata collect from various tables */

struct metaDataTbls* metaDataTblsNew();
/* create a metaData table object */

struct metaData* metaDataTblsFind(struct metaDataTbls* mdt,
                                  char* acc);
/* Find metadata by acc or NULL if not found. */

struct metaData* metaDataTblsGet(struct metaDataTbls* mdt,
                                 char* acc);
/* Get or create metadata table entry for an acc. */

void metaDataTblsAddProtAcc(struct metaDataTbls* mdt, struct metaData* md);
/* Add protein accession to protAccHash */

struct metaData* metaDataTblsGetByPep(struct metaDataTbls* mdt, char* pepAcc);
/* Get metadata table entry for an peptide acc, or NULL if not found */

void metaDataTblsFirst(struct metaDataTbls* mdt);
/* Set the pointer so the next call to metaDataTblsNext returns the first
 * entry */

struct metaData* metaDataTblsNext(struct metaDataTbls* mdt);
/* Get the next entry on serial reading of the table, */

void metaDataTblsFree(struct metaDataTbls** mdtPtr);
/* Free a metaDataTbls object */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

