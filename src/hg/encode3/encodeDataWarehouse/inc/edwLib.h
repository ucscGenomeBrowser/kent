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

long edwGotFile(struct sqlConnection *conn, char *submitDir, char *submitFileName, char *md5);
/* See if we already got file.  Return fileId if we do,  otherwise -1 */

#define EDW_ACCESS_SIZE 65    /* Size of our access key - currently base64 encoded SHA384 with 
                               * NULL terminator */

extern char *edwDatabase;   /* Name of database we connect to. */
extern char *edwRootDir;    /* Name of root directory for our files, including trailing '/' */
extern char *edwLicensePlatePrefix; /* License plates start with this - thanks Mike Cherry. */
extern char *edwValDataDir; /* Data files we need for validation go here. */

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

#endif /* EDWLIB_H */
