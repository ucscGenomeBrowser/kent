/* hui - human genome browser user interface controls that are shared
 * between more than one CGI. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef FILEUI_H
#define FILEUI_H

#include "cart.h"
#include "trackDb.h"

#define DOWNLOADS_ONLY_TITLE "Downloadable Files"
#define FILE_SORT_ORDER      "fileSortOrder"

// Miscellaneous but useful defines
#define ENCODE_DCC_DOWNLOADS "encodeDCC"

struct fileDb
// File in a list of downloadable files
    {
    struct fileDb *next;     // single link list
    char *fileName;          // File Name
    char *fileType;          // File Type, taken directly from .* suffix, but with .gz removed
    unsigned long fileSize;  // File size
    char *fileDate;          // File Modified? date and time
    struct mdbObj *mdb;      // The files are not trackDb entries but are found in the metaDb only
    char **sortFields;       // Array of strings to sort on in sort Order
    boolean *reverse;        // Direction of sort for array
    };


struct fileDb *fileDbGet(char *db, char *dir, char *subDir, char *fileName);
// Returns NULL or if found a fileDb struct with name, size and date filled in.

void fileDbFree(struct fileDb **pFileList);
// free one or more fileDb objects

void filesDownloadUi(char *db, struct cart *cart, struct trackDb *tdb);
// UI for a "composite like" track: This will list downloadable files associated with
// a single trackDb entry (composite or of type "downloadsOnly". The list of files
// will have links to their download and have metadata information associated.
// The list will be a sortable table and there may be filtering controls.

int fileSearchResults(char *db, struct sqlConnection *conn, struct cart *cart,
                      struct slPair *varValPairs, char *fileType);
// Prints list of files in downloads directories matching mdb search terms. Returns count

#endif /* FILEUI_H */
