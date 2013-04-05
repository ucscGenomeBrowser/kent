/* edwLib - routines shared by various encodeDataWarehouse programs.    See also encodeDataWarehouse
 * module for tables and routines to access structs built on tables. */

#ifndef EDWLIB_H
#define EDWLIB_H

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

#define edwMaxPlateSize 16  /* Max size of license plate including prefix. */

void edwMakeLicensePlate(char *prefix, int ix, char *out, int outSize);
/* Make a license-plate type string composed of prefix + funky coding of ix
 * and put result in out. */

void edwDirForTime(time_t sinceEpoch, char dir[PATH_LEN]);
/* Return the output directory for a given time. */

void edwMakePlateFileNameAndPath(int edwFileId, char licensePlate[edwMaxPlateSize],
    char edwFile[PATH_LEN], char serverPath[PATH_LEN]);
/* Convert file id to local file name, and full file path. Make any directories needed
 * along serverPath. */

long long edwGetLocalFile(struct sqlConnection *conn, char *localAbsolutePath, boolean symLink);
/* Get the id of a local file from the database, adding it if it doesn't already exist.
 * Add a local file to the database.  Go ahead and give it a license plate and an ID. 
 * optionally can make it a symbolic link instead of a copy. */

#endif /* EDWLIB_H */
