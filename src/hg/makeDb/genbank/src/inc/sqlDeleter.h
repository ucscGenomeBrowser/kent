/* Copyright (C) 2003 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef SQLDELETER_H
struct sqlConnection;

struct sqlDeleter
/* Object to delete a list of accessions from a table.  This was created to
 * deal with the problem of cleaning up orphans on a failed install.  Deleting
 * selected sequences was very slow in mysql (3.23.54). A case of deleting
 * ~800K accebsions from multiple tables took more than 23 hours.  This
 * appeared to be an partially an indexing issue, where indices were not used
 * when an OR expression was used in a delete.  So what this does is load the
 * accessions into a table, then does a join to create a new table, then
 * renames the new table to replace the old.  It also optimizes for deleting
 * by SQL commands when only a small number are deleted.  This object is 
 * initialized and can then be applied to many tables.
 */
{
    struct sqlDeleter* next;
    char tmpDir[PATH_LEN];
    struct lm* lm;                    /* local memory */
    int directMax;                    /* max number for direct deletes */
    int accCount;                     /* Count of accessions */
    struct slName* accs;              /* accessions for small deletes */
    struct sqlUpdater* accLoader;     /* Object to build accession table */
    boolean useDeleteJoin;            /* delete by joining into new table */
    boolean deletesDone;              /* have done a deletes, can't add */
    boolean verbose;                  /* verbose enabled */
};

struct sqlDeleter* sqlDeleterNew(char* tmpDir, boolean verbEnabled);
/* create a new deleter object.  If tmpdir is NULL, this will aways
 * do direct deletes, rather than falling back to the join/copy on a
 * large number of deleted. */

void sqlDeleterFree(struct sqlDeleter** sdPtr);
/* Free an object */

void sqlDeleterAddAcc(struct sqlDeleter* sd, char* acc);
/* Add an accession to list to to delete. */

void sqlDeleterDel(struct sqlDeleter* sd, struct sqlConnection* conn,
                   char* table, char* column);
/* Delete from where column matches list. */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

