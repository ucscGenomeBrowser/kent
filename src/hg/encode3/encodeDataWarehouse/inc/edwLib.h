/* edwLib - routines shared by various encodeDataWarehouse programs.    See also encodeDataWarehouse
 * module for tables and routines to access structs built on tables. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
extern char *eapRootDir;    /* Name of root directory for analysis pipeline */
extern char *edwValDataDir; /* Data files we need for validation go here. */
extern char *edwDaemonEmail; /* Email address of our automatic user. */

extern int edwSingleFileTimeout;   // How many seconds we give ourselves to fetch a single file

#define edwMinMapQual 3	//Above this -10log10 theshold we have >50% chance of being right

#define EDW_WEB_REFRESH_5_SEC 5000

struct sqlConnection *edwConnect();
/* Returns a read only connection to database. */

struct sqlConnection *edwConnectReadWrite();
/* Returns read/write connection to database. */

char *edwLicensePlatePrefix(struct sqlConnection *conn);
/* Return license plate prefix for current database - something like TST or DEV or ENCFF */

long long edwGotFile(struct sqlConnection *conn, char *submitDir, char *submitFileName, 
    char *md5, long long size);
/* See if we already got file.  Return fileId if we do,  otherwise 0.  This returns
 * TRUE based mostly on the MD5sum.  For short files (less than 100k) then we also require
 * the submitDir and submitFileName to match.  This is to cover the case where you might
 * have legitimate empty files duplicated even though they were computed based on different
 * things. For instance coming up with no peaks is a legitimate result for many chip-seq
 * experiments. */

long long edwGettingFile(struct sqlConnection *conn, char *submitDir, char *submitFileName);
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

struct edwUser *edwUserFromId(struct sqlConnection *conn, int id);
/* Return user associated with that id or NULL if not found */

int edwUserIdFromFileId(struct sqlConnection *conn, int fId);
/* Return user id who submit the file originally */

char *edwUserNameFromFileId(struct sqlConnection *conn, int fId);
/* Return user who submit the file originally */

struct edwUser *edwFindUserFromFileId(struct sqlConnection *conn, int fId);
/* Return user who submit the file originally */

char *edwFindOwnerNameFromFileId(struct sqlConnection *conn, int fId);
/* Return name of submitter. Return "an unknown user" if name is NULL */

int edwFindUserIdFromEmail(struct sqlConnection *conn, char *userEmail);
/* Return true id of this user */

boolean edwUserIsAdmin(struct sqlConnection *conn, char *userEmail);
/* Return true if the user is an admin */

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

char *edwSetting(struct sqlConnection *conn, char *name);
/* Return named settings value,  or NULL if setting doesn't exist. */

char *edwRequiredSetting(struct sqlConnection *conn, char *name);
/* Returns setting, abort if it isn't found. */

char *edwLicensePlateHead(struct sqlConnection *conn);
/* Return license plate prefix for current database - something like TST or DEV or ENCFF */

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

void edwValidFileUpdateDb(struct sqlConnection *conn, struct edwValidFile *el, long long id);
/* Save edwValidFile as a row to the table specified by tableName, replacing existing record at 
 * id. */

struct cgiParsedVars;   // Forward declare this so don't have to include cheapcgi
void edwValidFileFieldsFromTags(struct edwValidFile *vf, struct cgiParsedVars *tags);
/* Fill in many of vf's fields from tags. */

struct edwExperiment *edwExperimentFromAccession(struct sqlConnection *conn, char *acc); 
/* Given something like 'ENCSR123ABC' return associated experiment. */

struct edwFile *edwFileFromId(struct sqlConnection *conn, long long fileId);
/* Return edwFile given fileId - return NULL if not found. */

struct edwFile *edwFileFromIdOrDie(struct sqlConnection *conn, long long fileId);
/* Return edwFile given fileId - aborts if not found. */

struct genomeRangeTree *edwMakeGrtFromBed3List(struct bed3 *bedList);
/* Make up a genomeRangeTree around bed file. */

struct edwAssembly *edwAssemblyForUcscDb(struct sqlConnection *conn, char *ucscDb);
/* Get assembly for given UCSC ID or die trying */

struct edwAssembly *edwAssemblyForId(struct sqlConnection *conn, long long id);
/* Get assembly of given ID. */

char *edwSimpleAssemblyName(char *assembly);
/* Given compound name like male.hg19 return just hg19 */

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

struct edwSubmit *edwSubmitFromId(struct sqlConnection *conn, long long id);
/* Return submission with given ID or NULL if no such submission. */

struct edwSubmit *edwMostRecentSubmission(struct sqlConnection *conn, char *url);
/* Return most recent submission, possibly in progress, from this url */

long long edwSubmitMaxStartTime(struct edwSubmit *submit, struct sqlConnection *conn);
/* Figure out when we started most recent single file in the upload, or when
 * we started if not files started yet. */

int edwSubmitCountNewValid(struct edwSubmit *submit, struct sqlConnection *conn);
/* Count number of new files in submission that have been validated. */

boolean edwSubmitIsValidated(struct edwSubmit *submit, struct sqlConnection *conn);
/* Return TRUE if validation has run.  This does not mean that they all passed validation.
 * It just means the validator has run and has made a decision on each file in the submission. */

void edwAddSubmitJob(struct sqlConnection *conn, char *userEmail, char *url, boolean update);
/* Add submission job to table and wake up daemon.  If update is set allow submission to
 * include new metadata on old files. */

int edwSubmitPositionInQueue(struct sqlConnection *conn, char *url, unsigned *retJobId);
/* Return position of our URL in submission queue.  Optionally return id in edwSubmitJob
 * table of job. */

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

void edwFileResetTags(struct sqlConnection *conn, struct edwFile *ef, char *newTags,
    boolean revalidate);
/* Reset tags on file, strip out old validation and QA,  optionally schedule new validation 
 * and QA. */

#define edwSampleTargetSize 250000  /* We target this many samples */

void edwReserveTempFile(char *path);
/* Call mkstemp on path.  This will fill in terminal XXXXXX in path with file name
 * and create an empty file of that name.  Generally that empty file doesn't stay empty for long. */

void edwBwaIndexPath(struct edwAssembly *assembly, char indexPath[PATH_LEN]);
/* Fill in path to BWA index. */

void edwAsPath(char *format, char path[PATH_LEN]);
/* Convert something like "narrowPeak" in format to full path involving
 * encValDir/as/narrowPeak.as */

void edwAlignFastqMakeBed(struct edwFile *ef, struct edwAssembly *assembly,
    char *fastqPath, struct edwValidFile *vf, FILE *bedF,
    double *retMapRatio,  double *retDepth,  double *retSampleCoverage,
    double *retUniqueMapRatio);
/* Take a sample fastq and run bwa on it, and then convert that file to a bed. */

void edwMakeTempFastqSample(char *source, int size, char dest[PATH_LEN]);
/* Copy size records from source into a new temporary dest.  Fills in dest */

void edwMakeFastqStatsAndSample(struct sqlConnection *conn, long long fileId);
/* Run fastqStatsAndSubsample, and put results into edwFastqFile table. */

struct edwFastqFile *edwFastqFileFromFileId(struct sqlConnection *conn, long long fileId);
/* Get edwFastqFile with given fileId or NULL if none such */

struct edwBamFile * edwMakeBamStatsAndSample(struct sqlConnection *conn, long long fileId, 
    char sampleBed[PATH_LEN]);
/* Run edwBamStats and put results into edwBamFile table, and also a sample bed.
 * The sampleBed will be filled in by this routine. */

struct edwBamFile *edwBamFileFromFileId(struct sqlConnection *conn, long long fileId);
/* Get edwBamFile with given fileId or NULL if none such */

struct edwQaWigSpot *edwMakeWigSpot(struct sqlConnection *conn, long long wigId, long long spotId);
/* Create a new edwQaWigSpot record in database based on comparing wig file to spot file
 * (specified by id's in edwFile table). */

struct edwQaWigSpot *edwQaWigSpotFor(struct sqlConnection *conn, 
    long long wigFileId, long long spotFileId);
/* Return wigSpot relationship if any we have in database for these two files. */

char *edwOppositePairedEndString(char *end);
/* Return "1" for "2" and vice versa */

struct edwValidFile *edwOppositePairedEnd(struct sqlConnection *conn, struct edwValidFile *vf);
/* Given one file of a paired end set of fastqs, find the file with opposite ends. */

struct edwQaPairedEndFastq *edwQaPairedEndFastqFromVfs(struct sqlConnection *conn,
    struct edwValidFile *vfA, struct edwValidFile *vfB,
    struct edwValidFile **retVf1,  struct edwValidFile **retVf2);
/* Return pair record if any for the two fastq files. */

void edwMd5File(char *fileName, char md5Hex[33]);
/* call md5sum utility to calculate md5 for file and put result in hex format md5Hex 
 * This ends up being about 30% faster than library routine md5HexForFile,
 * however since there's popen() weird interactions with  stdin involved
 * it's not suitable for a general purpose library.  Environment inside edw
 * is controlled enough it should be ok. */

void edwPathForCommand(char *command, char path[PATH_LEN]);
/* Figure out path associated with command */

void edwPokeFifo(char *fifoName);
/* Send '\n' to fifo to wake up associated daemon */

FILE *edwPopen(char *command, char *mode);
/* do popen or die trying */

void edwOneLineSystemResult(char *command, char *line, int maxLineSize);
/* Execute system command and return one line result from it in line */

boolean edwOneLineSystemAttempt(char *command, char *line, int maxLineSize);
/* Execute system command and return one line result from it in line */

/***/
/* Shared functions for EDW web CGI's.
   Mostly wrappers for javascript tweaks */

void edwWebAutoRefresh(int msec);
/* Refresh page after msec.  Use 0 to cancel autorefresh */

/***/
/* Navigation bar */

void edwWebNavBarStart();
/* Layout navigation bar */

void edwWebNavBarEnd();
/* Close layout after navigation bar */

void edwWebBrowseMenuItem(boolean on);
/* Toggle visibility of 'Browse submissions' link on navigation menu */

void edwWebSubmitMenuItem(boolean on);
/* Toggle visibility of 'Submit data' link on navigation menu */

#endif /* EDWLIB_H */
