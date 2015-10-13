/* fof.h - Manages a fof-type index file.  This index refers
 * to records in multiple external files. 
 *
 * This file is copyright 2000 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */


struct fofPos
/* This holds the result of a FOF search. */
    {
    FILE *f;              /* File. */
    bits32 offset;        /* Offset within file. */
    bits32 size;          /* Size within file. */
    int indexIx;          /* Position within index. */
    char *fileName;       /* File name. */
    };


struct fof *fofOpen(char *fofName, char *fofDir);
/* Open up the named fof. fofDir may be NULL.  It should include 
 * trailing '/' if non-null. */

void fofClose(struct fof **pFof);
/* Close down the named fof. */

int fofElementCount(struct fof *fof);
/* How many names are in fof file? */

void fofMake(char *inFiles[], int inCount, char *outName, 
    boolean (*readHeader)(FILE *inFile, void *data),
    boolean (*nextRecord)(FILE *inFile, void *data, char **rName, int *rNameLen), 
    void *data, boolean dupeOk);
/* Make an index file
 * Inputs:
 *     inFiles - List of files that you're indexing with header read and verified.
 *     inCount - Size of file list.
 *     outName - name of index file to create
 *     readHeader - function that sets up file to read first record.  May be NULL.
 *     nextRecord - function that reads next record in file you're indexing
 *                  and returns the name of that record.  Returns FALSE at
 *                  end of file.  Can set *rNameLen to zero you want indexer
 *                  to ignore the record. 
 *     data - void pointer passed through to nextRecord.
 *     dupeOk - set to TRUE if you want dupes to not cause squawking
 */

boolean fofFindFirst(struct fof *fof, char *prefix, 
    int prefixSize, struct fofPos *retPos);
/* Find first element with key starting with prefix. */

boolean fofFind(struct fof *fof, char *name, struct fofPos *retPos);
/* Find element corresponding with name.  Returns FALSE if no such name
 * in the index file. */

void *fofFetch(struct fof *fof, char *name, int *retSize);
/* Lookup element in index, allocate memory for it, and read
 * it.  Returns buffer with element in it, which should be
 * freeMem'd when done. Aborts if element isn't there. */

char *fofFetchString(struct fof *fof, char *name, int *retSize);
/* Lookup element in index, allocate memory for it, read it.
 * Returns zero terminated string with element in it, which 
 * should be freeMem'd when done. Aborts if element isn't there. */

struct fofBatch
/* A structure for doing batch FOF searches.  Client fills
 * in key and data members.  fofBatchSearch then fills in
 * file, offset, and size members.  The list on return is
 * sorted by file position for fast i/o. */
    {
    struct fofBatch *next;  /* Next in list. */
    char *key;              /* Lookup key - filled in by client. Not allocate here. */
    void *data;             /* Data associated with key - filled in by client. */
    FILE *f;                /* File - filled in by server. */
    bits32 offset;          /* Offset in file - filled in by server. */
    bits32 size;            /* Size in file - filled in by server. */
    };
 
struct fofBatch *fofBatchFind(struct fof *fof, struct fofBatch *list);
/* Look up all of members on list. */

