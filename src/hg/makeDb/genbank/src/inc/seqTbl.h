/* Object to manage the gbSeq table. */

#ifndef SEQTBL_H
#define SEQTBL_H
#include "common.h"
#include "hgRelate.h"
struct sqlConnection;
struct gbSelect;

struct seqTbl
/* object to manage the gbSeq table.  Since this table is large, and usually
 * randomly accessed, this doesn't support loading the entire table into
 * memory.  */
{
    HGID nextId;                 /* next available sequence id */
    char *tmpDir;                /* tmp directory to use, if not NULL */
    int verbose;                 /* verbose level to use */
    struct sqlUpdater* updater;  /* object that handles updating */
    struct sqlDeleter* deleter;  /* object that handles deleting */
    
};

/* name of gbSeqTbl */
extern char* SEQ_TBL;

/* Values for type field */
extern char *SEQ_MRNA;
extern char *SEQ_EST;
extern char *SEQ_PEP;
extern char *SEQ_DNA;

/* Values for srcDb field */
extern char *SEQ_GENBANK;
extern char *SEQ_REFSEQ;
extern char *SEQ_OTHER_DB;


struct seqTbl* seqTblNew(struct sqlConnection* conn, char* tmpDir,
                         boolean verbose);
/* Create a seqTbl object.  If the sql table doesn't exist, create it.
 * If tmpDir is not null, this is the directory where the tab file
 * is created adding entries.
 */

void seqTblFree(struct seqTbl** stPtr);
/* free a seqTbl object */

HGID seqTblAdd(struct seqTbl *st, char* acc, int version, char* type,
               char *srcDb, HGID extDileId, unsigned seqSize, off_t fileOff,
               unsigned recSize);
/* add a seq to the table, allocating an id */

void seqTblMod(struct seqTbl *st, HGID id, int version, HGID extFileId,
               unsigned seqSize, off_t fileOff, unsigned recSize);
/* Modify attributes of an existing sequence.  If fileId is not zero, set
 * extFile, size, file_off and file_size. If version >= 0, set version.*/

void seqTblDelete(struct seqTbl *st, char *acc);
/* delete a row from the seqTbl */

HGID seqTblGetId(struct seqTbl *st, struct sqlConnection *conn, char* acc);
/* Get the id for a sequence, or 0 of it's not it the table */

void seqTblCommit(struct seqTbl *st, struct sqlConnection *conn);
/* commit pending changes */

void seqTblCancel(struct seqTbl *st);
/* cancel pending changes */

struct hash* seqTblLoadAcc(struct sqlConnection *conn,
                           struct gbSelect* select);
/* build a hash table for the acc in the sequence table matching the 
 * select paramters.  No values are stored in the hash. */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

