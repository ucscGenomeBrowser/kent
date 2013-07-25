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

#define edwRandomString "175d5bc99f7bb7312812c47d236791879BAEXzusIsdklnw86d73<*#$*(#)!DSFOUIHLjksdf"

extern char *edwDatabase;   /* Name of database we connect to. */
extern char *edwRootDir;    /* Name of root directory for our files, including trailing '/' */
extern char *edwLicensePlatePrefix; /* License plates start with this - thanks Mike Cherry. */
extern char *edwValDataDir; /* Data files we need for validation go here. */
extern int edwSingleFileTimeout;   // How many seconds we give ourselves to fetch a single file

struct sqlConnection *edwConnect();
/* Returns a read only connection to database. */

struct sqlConnection *edwConnectReadWrite();
/* Returns read/write connection to database. */

long edwGotFile(struct sqlConnection *conn, char *submitDir, char *submitFileName, char *md5);
/* See if we already got file.  Return fileId if we do,  otherwise -1 */

long edwGettingFile(struct sqlConnection *conn, char *submitDir, char *submitFileName);
/* See if we are in process of getting file.  Return file record id if it exists even if
 * it's not complete so long as it's not too old. Return -1 if record does not exist. */

char *edwPathForFileId(struct sqlConnection *conn, long long fileId);
/* Return full path (which eventually should be freeMem'd) for fileId */

char *edwTempDir();
/* Returns pointer to edwTempDir.  This is shared, so please don't modify. */

char *edwTempDirForToday(char dir[PATH_LEN]);
/* Fills in dir with temp dir of the day, and returns a pointer to it. */

long long edwNow();
/* Return current time in seconds since Epoch. */

struct edwUser *edwUserFromEmail(struct sqlConnection *conn, char *email);
/* Return user associated with that email or NULL if not found */

struct edwUser *edwMustGetUserFromEmail(struct sqlConnection *conn, char *email);
/* Return user associated with email or put up error message. */

struct edwUser *edwUserFromEmail(struct sqlConnection *conn, char *email);
/* Return user associated with that email or NULL if not found */

void edwWarnUnregisteredUser(char *email);
/* Put up warning message about unregistered user and tell them how to register. */

int edwGetHost(struct sqlConnection *conn, char *hostName);
/* Look up host name in table and return associated ID.  If not found
 * make up new host table entry. */

int edwGetSubmitDir(struct sqlConnection *conn, int hostId, char *submitDir);
/* Get submitDir from database, creating it if it doesn't already exist. */

#define edwMaxPlateSize 16  /* Max size of license plate including prefix and trailing 0. */

void edwMakeLicensePlate(char *prefix, int ix, char *out, int outSize);
/* Make a license-plate type string composed of prefix + funky coding of ix
 * and put result in out. */

void edwMakeBabyName(unsigned long id, char *baseName, int baseNameSize);
/* Given a numerical ID, make an easy to pronouce file name */

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
/* Return edwFile given fileId - return NULL if not found. */

struct edwFile *edwFileFromIdOrDie(struct sqlConnection *conn, long long fileId);
/* Return edwFile given fileId - aborts if not found. */

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

long long edwSubmitMaxStartTime(struct edwSubmit *submit, struct sqlConnection *conn);
/* Figure out when we started most recent single file in the upload, or when
 * we started if not files started yet. */

int edwSubmitCountNewValid(struct edwSubmit *submit, struct sqlConnection *conn);
/* Count number of new files in submission that have been validated. */

void edwAddSubmitJob(struct sqlConnection *conn, char *userEmail, char *url);
/* Add submission job to table and wake up daemon. */

int edwSubmitPositionInQueue(struct sqlConnection *conn, char *url);
/* Return position of our URL in submission queue */

struct edwValidFile *edwFindElderReplicates(struct sqlConnection *conn, struct edwValidFile *vf);
/* Find all replicates of same output and format type for experiment that are elder
 * (fileId less than your file Id).  Younger replicates are responsible for taking care 
 * of correlations with older ones.  Sorry younguns, it's like social security. */

void edwWebHeaderWithPersona(char *title);
/* Print out HTTP and HTML header through <BODY> tag with persona info */

void edwWebFooterWithPersona();
/* Print out end tags and persona script stuff */

char *edwGetEmailAndVerify();
/* Get email from persona-managed cookies and validate them.
 * Return email address if all is good and user is logged in.
 * If user not logged in return NULL.  If user logged in but
 * otherwise things are wrong abort. */

/* This is size of base64 encoded hash plus 1 for the terminating zero. */
#define EDW_SID_SIZE 65   

void edwMakeSid(char *user, char sid[EDW_SID_SIZE]);
/* Convert users to sid */

void edwCreateNewUser(char *email);
/* Create new user, checking that user does not already exist. */

void edwPrintLogOutButton();
/* Print log out button */

struct dyString *edwFormatDuration(long long seconds);
/* Convert seconds to days/hours/minutes. Return result in a dyString you can free */

struct edwFile *edwFileInProgress(struct sqlConnection *conn, int submitId);
/* Return file in submission in process of being uploaded if any. */

struct edwScriptRegistry *edwScriptRegistryFromCgi();
/* Get script registery from cgi variables.  Does authentication too. */

void edwFileResetTags(struct sqlConnection *conn, struct edwFile *ef, char *newTags);
/* Reset tags on file, strip out old validation and QA,  schedule new validation and QA. */

void edwAlignFastqMakeBed(struct edwFile *ef, struct edwAssembly *assembly,
    char *fastqPath, struct edwValidFile *vf, FILE *bedF,
    double *retMapRatio,  double *retDepth,  double *retSampleCoverage);
/* Take a sample fastq and run bwa on it, and then convert that file to a bed. */

#endif /* EDWLIB_H */
