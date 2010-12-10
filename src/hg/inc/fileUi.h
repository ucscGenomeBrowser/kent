/* hui - human genome browser user interface controls that are shared
 * between more than one CGI. */
#ifndef FILEUI_H
#define FILEUI_H

#include "cart.h"
#include "trackDb.h"

struct fileDb
// File in a list of downloadable files
    {
    struct fileDb *next;  // single link list
    char *fileName;       // File Name
    off_t fileSize;       // File size
    char *fileDate;       // File Modified? date and time
    struct mdbObj *mdb;   // The files are not trackDb entries but are found in the metaDb only
    char **sortFields;    // Array of strings to sort on in sort Order
    boolean *reverse;     // Direction of sort for array
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

#endif /* FILEUI_H */
