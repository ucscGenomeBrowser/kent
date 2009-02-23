/* baseMaskCommon - functions to create genomeRangeTree baseMask files to/from database tracks. */
#ifndef BASEMASKCOMMON_H
#define BASEMASKCOMMON_H


char *chromTable(char *db, char *table, char *chromDb);
/* Get chr1_table if it exists, otherwise table.
 * You can freeMem this when done. */

time_t chromTableUpdateTime(char *db, char *table, char *chromDb);
/* Find the last update time for table in db.
 * If the table doesnt exist, find a chromosome or scaffold) name from chromDb and
 * check if the table is split by chromosome.
 * Errors out if time is not a positive integer.
 * Returns the table update time. */

void splitDbTable(char *chromDb, char *track, char **pDb, char **pTable);
/* split the track into db name and table name. 
 * If no db specified then this will be chromDb.
 * Cannabalizes track. 
 * *pDb points a table in *track. *pTable points at database in *track or *chromDb. */

void trackToBaseMask(char *db, char *track, char *chromDb, char *oBama, boolean quiet);
/* Create a baseMask file oBama representing the 'track' in database 'db'.
 * If quiet = false, outputs number of based on overlap of chromosome ranges. */

char *baseMaskCacheTrack(char *cacheDir, char *chromDb, char *db, char *table, boolean quiet, boolean logUpdateTimes);
/* Return the file name of the cached baseMask for the specified table and db.
 * The chromInfo table is read from chromDb database.
 * The cached file will be saved in the file cacheDir/db/table.bama 
 * If logUpdateTimes is true then the table and cache update times will be
 * appended to cacheDir/db/table.bama.log
 * The bama file is written as a temp file then moved to the final name as 
 * an atomic operation. 
 * Return value must be freed with freeMem(). */

#endif /* BASEMASKCOMMON_H */
