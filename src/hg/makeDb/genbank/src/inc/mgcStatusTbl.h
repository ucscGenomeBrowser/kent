/* Code to create and access the mgcStatus table */
#ifndef MGCSTATUSTBL_H
#define MGCSTATUSTBL_H
#include "common.h"
struct sqlConnection;

struct mgcStatusType
/* Structure used to represent a status.  There is a table of the valid
 * values, with the pointer to the status being using as the value. */
{
    char *dbValue;    /* value used for database field (which is an enum) */
    int dbIndex;      /* database enum index for value */
    char *desc;       /* natural language description */
    boolean isError;  /* does this represent an error? */
};

/* 
 * Clone detailed status values.
 *
 * IMPORTANT: changes here must be reflected in the parser in mgcImport.c
 * and in browser hgc.c mgcStatusDesc table.
 */
/** has not been picked status */
extern struct mgcStatusType MGC_UNPICKED;

/*** these are in-progress status ***/
extern struct mgcStatusType MGC_PICKED;
extern struct mgcStatusType MGC_NOT_BACK;
extern struct mgcStatusType MGC_NO_DECISION;

/*** these are full-length status ***/
extern struct mgcStatusType MGC_FULL_LENGTH;
extern struct mgcStatusType MGC_FULL_LENGTH_SHORT;

/*** these are error status ***/
extern struct mgcStatusType MGC_INCOMPLETE;
extern struct mgcStatusType MGC_CHIMERIC;
extern struct mgcStatusType MGC_FRAME_SHIFTED;
extern struct mgcStatusType MGC_CONTAMINATED;
extern struct mgcStatusType MGC_RETAINED_INTRON;
extern struct mgcStatusType MGC_MIXED_WELLS;
extern struct mgcStatusType MGC_NO_GROWTH;
extern struct mgcStatusType MGC_NO_INSERT;
extern struct mgcStatusType MGC_NO_5_EST_MATCH;
extern struct mgcStatusType MGC_MICRODELETION;
extern struct mgcStatusType MGC_LIBRARY_ARTIFACTS;
extern struct mgcStatusType MGC_NO_POLYA_TAIL;
extern struct mgcStatusType MGC_CANT_SEQUENCE;

struct mgcStatus
/* A row in the mgcStatus table.  None of the fields are unique, am image id
 * may occur multiple times.  However since we don't track row, plate, this is
 * a simplified view.  A image id will occur multiple times only if it has
 * been successfuly sequences multiple times (has multiple gb_acc in the
 * fullength table).
 */
{
    struct mgcStatus *next;
    unsigned imageId;                 /* image clone id number */
    struct mgcStatusType *status;     /* current status information */
    char *acc;                        /* accession, or null if not available */
    char organism[3];                 /* two-letter organism code */
};

struct mgcStatusTbl
/* Object for creating and mgc table. */
{
    struct hash *imageIdHash;    /* hash of id to mgcStatus.  Status objects
                                  * also stored in localmem */
};

/* SQL to create status table. Should have table name sprinted into it. */
extern char *mgcStatusCreateSql;

char *mgcOrganismNameToCode(char *organism, char *whereFound);
/* convert a MGC organism name to a two-letter code.  An error with
 * whereFound is generated if it can't be mapped */

struct mgcStatusTbl *mgcStatusTblNew();
/* Create an mgcStatusTbl object */

struct mgcStatusTbl *mgcStatusTblLoad(char *mgcStatusTab);
/* Load a mgcStatusTbl object from a tab file */

void mgcStatusTblFree(struct mgcStatusTbl **mstPtr);
/* Free object */

void mgcStatusTblAdd(struct mgcStatusTbl *mst, unsigned imageId,
                     struct mgcStatusType *status, char *acc,
                     char *organism);
/* Add an entry to the table. acc maybe NULL */

void mgcStatusSetAcc(struct mgcStatusTbl *mst, struct mgcStatus *ms, char *acc);
/* Change the accession on entry to the table. acc maybe NULL */

struct mgcStatus* mgcStatusTblFind(struct mgcStatusTbl *mst, unsigned imageId);
/* Find an entry in the table by imageId, or null if not foudn.  If there
 * are multiple occuranges, they will be linked by the next field. */

void mgcStatusTblTabOut(struct mgcStatusTbl *mst, FILE *f);
/* Write the table to a tab file */

void mgcStatusTblFullTabOut(struct mgcStatusTbl *mst, FILE *f);
/* Write the full-length clones in the table to a tab file */


#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
