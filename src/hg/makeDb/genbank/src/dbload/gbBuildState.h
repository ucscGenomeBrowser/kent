/* Load the status table and determine the state of each entry.  Also,
 * various operators base on state. */
#ifndef GBBUILDSTATE_H

struct sqlConnection;
struct gbRelease;
struct gbGenome;
struct gbSelect;
struct gbStatus;
struct dbLoadOptions;

/* Special values to store in gbEntry.selectVer */
#define GB_UNCHG_SELECT_VER -2

struct gbStatusTbl* gbBuildState(struct sqlConnection *conn,
                                 struct gbSelect* select,
                                 struct dbLoadOptions* options,
                                 float maxShrinkage,
                                 char* tmpDir, int verboseLevel,
                                 boolean* maxShrinkageExceeded);
/* Load status table and find of state of all genbank entries in the release
 * compared to the database.  If maxShrinkage is exeeeded, the list of deleted
 * accessions is printed and maxShrinkageExceeded is set to true. */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
