#ifndef EXTFILETBL_H
#define EXTFILETBL_H

#include "common.h"
#include "hgRelate.h"
struct sqlConnection;
struct gbSelect;

struct extFile
/* This stores info on an external file. */
{
    HGID id;         /* id of file */
    char *path;      /* full path of file */
    off_t size;      /* size of file */
};

struct extFileRef
/* list of references to extFile objects */
{
    struct extFileRef *next;
    struct extFile *extFile;
};

struct extFileTbl
/* Object for managing and access the gbExtFile table.  Handles unique
 * id allocation, */
{
    struct hash* pathHash;    /* hash by path.  Hash localmem use for all
                               * allocations */
    struct hash* idHash;      /* hash by id */
};

/* Name of table */
extern char* EXT_FILE_TBL;

struct extFileTbl* extFileTblLoad(struct sqlConnection *conn);
/* Load the file table from the database, creating table if it doesn't exist */

HGID extFileTblGet(struct extFileTbl *eft, struct sqlConnection *conn,
                   char *path);
/* Lookup a file in the table.  If it doesn't exists, add it and update the
 * database. */

struct extFile* extFileTblFindById(struct extFileTbl *eft, HGID id);
/* Get the entry for an id, or null if not found */

struct extFileRef* extFileTblMatch(struct extFileTbl *eft, struct gbSelect *select);
/* get list of files with paths matching the specified list.  For refseqs,
 * protein files are returned too.  Free results with slFreeList. */

void extFileTblFree(struct extFileTbl **eftPtr);
/* Free a extFileTbl object */

void extFileTblClean(struct sqlConnection *conn, boolean verbEnabled);
/* Remove rows in the gbExtFile table that are not referenced in the gbSeq
 * table.  This is fast.  Also remove pep.fa file that are not associated with
 * any mrna.fa file.  This is a hack that gets around the refseq peptides not
 * being in gbSeq table. */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
