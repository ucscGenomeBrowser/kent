/* Code to create and access the imageClone table */

/* Copyright (C) 2003 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef IMAGECLONETBL_H
#define IMAGECLONETBL_H
#include "common.h"
struct sqlConnection;

struct imageCloneTbl
/* Object for load the image clone table. */
{
    struct sqlUpdater *updater;  /* table updating object */
};

/* name of the table */
extern char *IMAGE_CLONE_TBL;

unsigned imageCloneGBParse(char *cloneSpec);
/* Parse an image clone id from the string take from the genbank
 * /clone field.  There are many weird case, some are handled and some are
 * infrequent enough to skip.  Several of these weird case to seem to
 * have current IMAGE clones anyway.
 * Returns 0 if not a parsable IMAGE clone spec. */

struct imageCloneTbl *imageCloneTblNew(struct sqlConnection *conn,
                                       char *tmpDir, boolean verbEnabled);
/* Create an object for loading the clone table */

void imageCloneTblFree(struct imageCloneTbl **ictPtr);
/* Free object */

unsigned imageCloneTblGetId(struct sqlConnection *conn, char *acc);
/* get id for an acc, or 0 if none */

void imageCloneTblAdd(struct imageCloneTbl *ict, unsigned imageId,
                      char *acc, unsigned type, char direction);
/* Add an entry to the table */

void imageCloneTblMod(struct imageCloneTbl *ict, unsigned imageId,
                      char *acc, char direction);
/* update an entry in the table */

void imageCloneTblCommit(struct imageCloneTbl *ict,
                         struct sqlConnection *conn);
/* Commit pending changes */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
