/* edwLib - routines shared by various encodeDataWarehouse programs.    See also encodeDataWarehouse
 * module for tables and routines to access structs built on tables. */

#ifndef EDWLIB_H
#define EDWLIB_H

#ifndef DYSTRING_H
#include "dystring.h"
#endif 

#ifndef JKSQL_H
#include "jksql.h"
#endif

#ifndef BASICBED_H
#include "basicBed.h"
#endif

long edwGotFile(struct sqlConnection *conn, char *submitDir, char *submitFileName, char *md5);
/* See if we already got file.  Return fileId if we do,  otherwise -1 */

long edwGettingFile(struct sqlConnection *conn, char *submitDir, char *submitFileName);
/* See if we are in process of getting file.  Return file record id if it exists even if
 * it's not complete so long as it's not too old. Return -1 if record does not exist. */

#define EDW_ACCESS_SIZE 65    /* Size of our access key - currently base64 encoded SHA384 with 
                               * NULL terminator */

extern char *edwDatabase;   /* Name of database we connect to. */
extern char *edwRootDir;    /* Name of root directory for our files, including trailing '/' */
extern char *edwLicensePlatePrefix; /* License plates start with this - thanks Mike Cherry. */
extern char *edwValDataDir; /* Data files we need for validation go here. */
extern int edwSingleFileTimeout;   // How many seconds we give ourselves to fetch a single file

char *edwPathForFileId(struct sqlConnection *conn, long long fileId);
/* Return full path (which eventually should be freeMem'd) for fileId */

char *edwTempDir();
/* Returns pointer to edwTempDir.  This is shared, so please don't modify. */

long long edwNow();
/* Return current time in seconds since Epoch. */

void edwMakeAccess(char *password, char access[EDW_ACCESS_SIZE]);
/* Convert password + salt to an access code */

int edwCheckAccess(struct sqlConnection *conn, char *user, char *password);
/* Make sure user exists and password checks out. Returns (non-zero) user ID on success*/

int edwMustHaveAccess(struct sqlConnection *conn, char *user, char *password);
/* Check user has access and abort with an error message if not. Returns user id. */

int edwCheckUserNameSize(char *user);
/* Make sure user name not too long. Returns size or aborts if too long. */

struct edwUser *edwUserFromEmail(struct sqlConnection *conn, char *email);
/* Return user associated with that email or NULL if not found */

struct edwUser *edwMustGetUserFromEmail(struct sqlConnection *conn, char *email);
/* Return user associated with email or put up error message. */

void edwMakeSid(char *user, char sid[EDW_ACCESS_SIZE]);
/* Convert users to sid */

int edwGetHost(struct sqlConnection *conn, char *hostName);
/* Look up host name in table and return associated ID.  If not found
 * make up new host table entry. */

int edwGetSubmitDir(struct sqlConnection *conn, int hostId, char *submitDir);
/* Get submitDir from database, creating it if it doesn't already exist. */

#define edwMaxPlateSize 16  /* Max size of license plate including prefix and trailing 0. */

void edwMakeLicensePlate(char *prefix, int ix, char *out, int outSize);
/* Make a license-plate type string composed of prefix + funky coding of ix
 * and put result in out. */

void edwDirForTime(time_t sinceEpoch, char dir[PATH_LEN]);
/* Return the output directory for a given time. */

char *edwFindDoubleFileSuffix(char *path);
/* Return pointer to second from last '.' in part of path between last / and end.  
 * If there aren't two dots, just return pointer to normal single dot suffix. */

void edwMakeFileNameAndPath(int edwFileId, char *submitFileName, char edwFile[PATH_LEN], char serverPath[PATH_LEN]);
/* Convert file id to local file name, and full file path. Make any directories needed
 * along serverPath. */

struct edwFile *edwGetLocalFile(struct sqlConnection *conn, char *localAbsolutePath, 
    char *symLinkMd5Sum);
/* Get record of local file from database, adding it if it doesn't already exist.
 * Can make it a symLink rather than a copy in which case pass in valid MD5 sum
 * for symLinkM5dSum. */

void edwUpdateFileTags(struct sqlConnection *conn, long long fileId, struct dyString *tags);
/* Update tags field in edwFile with given value */

struct edwFile *edwFileAllIntactBetween(struct sqlConnection *conn, int startId, int endId);
/* Return list of all files that are intact (finished uploading and MD5 checked) 
 * with file IDs between startId and endId - including endId*/

struct edwValidFile *edwValidFileFromFileId(struct sqlConnection *conn, long long fileId);
/* Return edwValidFile give fileId - returns NULL if not validated. */

struct edwFile *edwFileFromId(struct sqlConnection *conn, long long fileId);
/* Return edwValidFile given fileId - return NULL if not found. */

struct edwFile *edwFileFromIdOrDie(struct sqlConnection *conn, long long fileId);
/* Return edwValidFile given fileId - aborts if not found. */

struct genomeRangeTree *edwMakeGrtFromBed3List(struct bed3 *bedList);
/* Make up a genomeRangeTree around bed file. */

struct edwAssembly *edwAssemblyForUcscDb(struct sqlConnection *conn, char *ucscDb);
/* Get assembly for given UCSC ID or die trying */

struct genomeRangeTree *edwGrtFromBigBed(char *fileName);
/* Return genome range tree for simple (unblocked) bed */

boolean edwIsSupportedBigBedFormat(char *format);
/* Return TRUE if it's one of the bigBed formats we support. */

void edwWriteErrToTable(struct sqlConnection *conn, char *table, int id, char *err);
/* Write out error message to errorMessage field of table. */

void edwWriteErrToStderrAndTable(struct sqlConnection *conn, char *table, int id, char *err);
/* Write out error message to errorMessage field of table. */

void edwAddJob(struct sqlConnection *conn, char *command);
/* Add job to queue to run. */

void edwAddQaJob(struct sqlConnection *conn, long long fileId);
/* Create job to do QA on this and add to queue */

struct edwSubmit *edwMostRecentSubmission(struct sqlConnection *conn, char *url);
/* Return most recent submission, possibly in progress, from this url */

#endif /* EDWLIB_H */
