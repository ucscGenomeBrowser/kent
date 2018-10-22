/* cdwLib - routines shared by various cdw programs.    See also cdw
 * module for tables and routines to access structs built on tables. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef CDWLIB_H
#define CDWLIB_H

#ifndef DYSTRING_H
#include "dystring.h"
#endif 

#ifndef JKSQL_H
#include "jksql.h"
#endif

#ifndef BASICBED_H
#include "basicBed.h"
#endif

#ifndef CDW_H
#include "cdw.h"
#endif

#include "cart.h"

#define cdwRandomString "175d5bc99f7bb7312812c47d236791879BAEXzusIsdklnw86d73<*#$*(#)!DSFOUIHLjksdf"

extern char *cdwDatabase;   /* Name of database we connect to. */
extern char *cdwRootDir;    /* Name of root directory for our files, including trailing '/' */
extern char *eapRootDir;    /* Name of root directory for analysis pipeline */
extern char *cdwValDataDir; /* Data files we need for validation go here. */
extern char *cdwDaemonEmail; /* Email address of our automatic user. */

extern int cdwSingleFileTimeout;   // How many seconds we give ourselves to fetch a single file

#define cdwMinMapQual 3	//Above this -10log10 theshold we have >50% chance of being right

#define CDW_WEB_REFRESH_5_SEC 5000

struct sqlConnection *cdwConnect();
/* Returns a read only connection to database. */

struct sqlConnection *cdwConnectReadWrite();
/* Returns read/write connection to database. */

long long cdwGotFile(struct sqlConnection *conn, char *submitDir, char *submitFileName, 
    char *md5, long long size);
/* See if we already got file.  Return fileId if we do,  otherwise 0.  This returns
 * TRUE based mostly on the MD5sum.  For short files (less than 100k) then we also require
 * the submitDir and submitFileName to match.  This is to cover the case where you might
 * have legitimate empty files duplicated even though they were computed based on different
 * things. For instance coming up with no peaks is a legitimate result for many chip-seq
 * experiments. */

long long cdwGettingFile(struct sqlConnection *conn, char *submitDir, char *submitFileName);
/* See if we are in process of getting file.  Return file record id if it exists even if
 * it's not complete so long as it's not too old. Return -1 if record does not exist. */

char *cdwPathForFileId(struct sqlConnection *conn, long long fileId);
/* Return full path (which eventually should be freeMem'd) for fileId */

char *cdwTempDir();
/* Returns pointer to cdwTempDir.  This is shared, so please don't modify. */

char *cdwTempDirForToday(char dir[PATH_LEN]);
/* Fills in dir with temp dir of the day, and returns a pointer to it. */

long long cdwNow();
/* Return current time in seconds since Epoch. */

struct cdwUser *cdwUserFromUserName(struct sqlConnection *conn, char* userName);
/* Return user associated with that username or NULL if not found */

struct cdwUser *cdwUserFromEmail(struct sqlConnection *conn, char *email);
/* Return user associated with that email or NULL if not found */

struct cdwUser *cdwMustGetUserFromEmail(struct sqlConnection *conn, char *email);
/* Return user associated with email or put up error message. */

struct cdwUser *cdwUserFromId(struct sqlConnection *conn, int id);
/* Return user associated with that id or NULL if not found */

int cdwUserIdFromFileId(struct sqlConnection *conn, int fId);
/* Return user id who submit the file originally */

char *cdwUserNameFromFileId(struct sqlConnection *conn, int fId);
/* Return user who submit the file originally */

struct cdwUser *cdwFindUserFromFileId(struct sqlConnection *conn, int fId);
/* Return user who submit the file originally */

char *cdwFindOwnerNameFromFileId(struct sqlConnection *conn, int fId);
/* Return name of submitter. Return "an unknown user" if name is NULL */

int cdwFindUserIdFromEmail(struct sqlConnection *conn, char *userEmail);
/* Return true id of this user */

boolean cdwUserIsAdmin(struct sqlConnection *conn, char *userEmail);
/* Return true if the user is an admin */

void cdwWarnUnregisteredUser(char *email);
/* Put up warning message about unregistered user and tell them how to register. */

struct cdwGroup *cdwGroupFromName(struct sqlConnection *conn, char *name);
/* Return cdwGroup of given name or NULL if not found. */

struct cdwGroup *cdwNeedGroupFromName(struct sqlConnection *conn, char *groupName);
/* Get named group or die trying */

boolean cdwFileInGroup(struct sqlConnection *conn, unsigned int fileId, unsigned int groupId);
/* Return TRUE if file is in group */

int cdwUserFileGroupsIntersect(struct sqlConnection *conn, long long fileId, int userId);
/* Return the number of groups file and user have in common,  zero for no match */

#define cdwAccessRead 1
#define cdwAccessWrite 2

boolean cdwCheckAccess(struct sqlConnection *conn, struct cdwFile *ef,
    struct cdwUser *user, int accessType);
/* See if user should be allowed this level of access.  The accessType is one of
 * cdwAccessRead or cdwAccessWrite.  Write access implies read access too. 
 * This can be called with user as NULL, in which case only access to shared-with-all
 * files is granted. This function takes almost a millisecond.  If you are doing it
 * to many files consider using cdwQuickCheckAccess instead. */

boolean cdwQuickCheckAccess(struct rbTree *groupedFiles, struct cdwFile *ef,
    struct cdwUser *user, int accessType);
/* See if user should be allowed this level of access.  The groupedFiles is
 * the result of a call to cdwFilesWithSharedGroup. The other parameters are as
 * cdwCheckAccess.  If you are querying thousands of files, this function is hundreds
 * of times faster though. */

struct rbTree *cdwFilesWithSharedGroup(struct sqlConnection *conn, int userId);
/* Make an intVal type tree where the keys are fileIds and the val is null 
 * This contains all files that are associated with any group that user is part of. 
 * Can be used to do quicker version of cdwCheckAccess. */

long long cdwCountAccessible(struct sqlConnection *conn, struct cdwUser *user);
/* Return total number of files associated user can access */

struct cdwFile *cdwAccessibleFileList(struct sqlConnection *conn, struct cdwUser *user);
/* Get list of all files user can access.  Null user means just publicly accessible.  */

struct rbTree *cdwAccessTreeForUser(struct sqlConnection *conn, struct cdwUser *user, 
    struct cdwFile *efList, struct rbTree *groupedFiles);
/* Construct intVal tree of files from efList that we have access to.  The
 * key is the fileId,  the value is the cdwFile object */

int cdwGetHost(struct sqlConnection *conn, char *hostName);
/* Look up host name in table and return associated ID.  If not found
 * make up new host table entry. */

int cdwGetSubmitDir(struct sqlConnection *conn, int hostId, char *submitDir);
/* Get submitDir from database, creating it if it doesn't already exist. */

#define cdwMaxPlateSize 16  /* Max size of license plate including prefix and trailing 0. */

void cdwMakeLicensePlate(char *prefix, int ix, char *out, int outSize);
/* Make a license-plate type string composed of prefix + funky coding of ix
 * and put result in out. */

void cdwMakeBabyName(unsigned long id, char *baseName, int baseNameSize);
/* Given a numerical ID, make an easy to pronouce file name */

void cdwDirForTime(time_t sinceEpoch, char dir[PATH_LEN]);
/* Return the output directory for a given time. */

char *cdwFindDoubleFileSuffix(char *path);
/* Return pointer to second from last '.' in part of path between last / and end.  
 * If there aren't two dots, just return pointer to normal single dot suffix. */

void cdwMakeFileNameAndPath(int cdwFileId, char *submitFileName, char cdwFile[PATH_LEN], char serverPath[PATH_LEN]);
/* Convert file id to local file name, and full file path. Make any directories needed
 * along serverPath. */

char *cdwSetting(struct sqlConnection *conn, char *name);
/* Return named settings value,  or NULL if setting doesn't exist. */

char *cdwRequiredSetting(struct sqlConnection *conn, char *name);
/* Returns setting, abort if it isn't found. */

char *cdwLicensePlateHead(struct sqlConnection *conn);
/* Return license plate prefix for current database - something like TST or DEV or ENCFF */

struct cdwFile *cdwGetLocalFile(struct sqlConnection *conn, char *localAbsolutePath, 
    char *symLinkMd5Sum);
/* Get record of local file from database, adding it if it doesn't already exist.
 * Can make it a symLink rather than a copy in which case pass in valid MD5 sum
 * for symLinkM5dSum. */

void cdwUpdateFileTags(struct sqlConnection *conn, long long fileId, struct dyString *tags);
/* Update tags field in cdwFile with given value */

struct cdwFile *cdwFileLoadAllValid(struct sqlConnection *conn);
/* Get list of cdwFiles that have been validated with no error */

struct cdwFile *cdwFileAllIntactBetween(struct sqlConnection *conn, int startId, int endId);
/* Return list of all files that are intact (finished uploading and MD5 checked) 
 * with file IDs between startId and endId - including endId*/

long long cdwFindInSameSubmitDir(struct sqlConnection *conn, 
    struct cdwFile *ef, char *submitFileName);
/* Return fileId of most recent file of given submitFileName from submitDir
 * associated with file */

struct cdwValidFile *cdwValidFileFromFileId(struct sqlConnection *conn, long long fileId);
/* Return cdwValidFile give fileId - returns NULL if not validated. */

struct cdwValidFile *cdwValidFileFromLicensePlate(struct sqlConnection *conn, char *licensePlate);
/* Return cdwValidFile from license plate - returns NULL if not found. */

void cdwValidFileUpdateDb(struct sqlConnection *conn, struct cdwValidFile *el, long long id);
/* Save cdwValidFile as a row to the table specified by tableName, replacing existing record at 
 * id. */

struct cgiParsedVars;   // Forward declare this so don't have to include cheapcgi
void cdwValidFileFieldsFromTags(struct cdwValidFile *vf, struct cgiParsedVars *tags);
/* Fill in many of vf's fields from tags. */

struct cdwExperiment *cdwExperimentFromAccession(struct sqlConnection *conn, char *acc); 
/* Given something like 'ENCSR123ABC' return associated experiment. */

struct cdwFile *cdwFileFromId(struct sqlConnection *conn, long long fileId);
/* Return cdwFile given fileId - return NULL if not found. */

struct cdwFile *cdwFileFromIdOrDie(struct sqlConnection *conn, long long fileId);
/* Return cdwFile given fileId - aborts if not found. */

int cdwFileIdFromPathSuffix(struct sqlConnection *conn, char *suf);
/* return most recent fileId for file where submitDir.url+submitFname ends with suf. 0 if not found. */

struct genomeRangeTree *cdwMakeGrtFromBed3List(struct bed3 *bedList);
/* Make up a genomeRangeTree around bed file. */

struct cdwAssembly *cdwAssemblyForUcscDb(struct sqlConnection *conn, char *ucscDb);
/* Get assembly for given UCSC ID or die trying */

struct cdwAssembly *cdwAssemblyForId(struct sqlConnection *conn, long long id);
/* Get assembly of given ID. */

char *cdwSimpleAssemblyName(char *assembly);
/* Given compound name like male.hg19 return just hg19 */

struct genomeRangeTree *cdwGrtFromBigBed(char *fileName);
/* Return genome range tree for simple (unblocked) bed */

boolean cdwIsSupportedBigBedFormat(char *format);
/* Return TRUE if it's one of the bigBed formats we support. */

void cdwWriteErrToTable(struct sqlConnection *conn, char *table, int id, char *err);
/* Write out error message to errorMessage field of table. */

void cdwWriteErrToStderrAndTable(struct sqlConnection *conn, char *table, int id, char *err);
/* Write out error message to errorMessage field of table. */

void cdwAddJob(struct sqlConnection *conn, char *command, int submitId);
/* Add job to queue to run. */

void cdwAddQaJob(struct sqlConnection *conn, long long fileId, int submitId);
/* Create job to do QA on this and add to queue */

struct cdwSubmitDir *cdwSubmitDirFromId(struct sqlConnection *conn, long long id);
/* Return submissionDir with given ID or NULL if no such submission. */

struct cdwSubmit *cdwSubmitFromId(struct sqlConnection *conn, long long id);
/* Return submission with given ID or NULL if no such submission. */

struct cdwSubmit *cdwMostRecentSubmission(struct sqlConnection *conn, char *url);
/* Return most recent submission, possibly in progress, from this url */

long long cdwSubmitMaxStartTime(struct cdwSubmit *submit, struct sqlConnection *conn);
/* Figure out when we started most recent single file in the upload, or when
 * we started if not files started yet. */

int cdwSubmitCountNewValid(struct cdwSubmit *submit, struct sqlConnection *conn);
/* Count number of new files in submission that have been validated. */

boolean cdwSubmitIsValidated(struct cdwSubmit *submit, struct sqlConnection *conn);
/* Return TRUE if validation has run.  This does not mean that they all passed validation.
 * It just means the validator has run and has made a decision on each file in the submission. */

void cdwAddSubmitJob(struct sqlConnection *conn, char *userEmail, char *url, boolean update);
/* Add submission job to table and wake up daemon.  If update is set allow submission to
 * include new metadata on old files. */

int cdwSubmitPositionInQueue(struct sqlConnection *conn, char *url, unsigned *retJobId);
/* Return position of our URL in submission queue.  Optionally return id in cdwSubmitJob
 * table of job. */

struct cdwValidFile *cdwFindElderReplicates(struct sqlConnection *conn, struct cdwValidFile *vf);
/* Find all replicates of same output and format type for experiment that are elder
 * (fileId less than your file Id).  Younger replicates are responsible for taking care 
 * of correlations with older ones.  Sorry younguns, it's like social security. */

void cdwWebHeaderWithPersona(char *title);
/* Print out HTTP and HTML header through <BODY> tag with persona info */

void cdwWebFooterWithPersona();
/* Print out end tags and persona script stuff */

char *cdwGetEmailAndVerify();
/* Get email from persona-managed cookies and validate them.
 * Return email address if all is good and user is logged in.
 * If user not logged in return NULL.  If user logged in but
 * otherwise things are wrong abort. */

/* This is size of base64 encoded hash plus 1 for the terminating zero. */
#define CDW_SID_SIZE 65   

void cdwMakeSid(char *user, char sid[CDW_SID_SIZE]);
/* Convert users to sid */

void cdwCreateNewUser(char *email);
/* Create new user, checking that user does not already exist. */

void cdwPrintLogOutButton();
/* Print log out button */

struct dyString *cdwFormatDuration(long long seconds);
/* Convert seconds to days/hours/minutes. Return result in a dyString you can free */

struct cdwFile *cdwFileInProgress(struct sqlConnection *conn, int submitId);
/* Return file in submission in process of being uploaded if any. */

struct cdwScriptRegistry *cdwScriptRegistryFromCgi();
/* Get script registery from cgi variables.  Does authentication too. */

void cdwFileResetTags(struct sqlConnection *conn, struct cdwFile *ef, char *newTags, 
    boolean revalidate, int submitId);
/* Reset tags on file, strip out old validation and QA,  optionally schedule new validation 
 * and QA. */

#define cdwSampleTargetSize 250000  /* We target this many samples */

void cdwReserveTempFile(char *path);
/* Call mkstemp on path.  This will fill in terminal XXXXXX in path with file name
 * and create an empty file of that name.  Generally that empty file doesn't stay empty for long. */

void cdwIndexPath(struct cdwAssembly *assembly, char indexPath[PATH_LEN]);
/* Fill in path to a bowtie index (originally bwa index). */

void cdwAsPath(char *format, char path[PATH_LEN]);
/* Convert something like "narrowPeak" in format to full path involving
 * encValDir/as/narrowPeak.as */

void cdwAlignFastqMakeBed(struct cdwFile *ef, struct cdwAssembly *assembly,
    char *fastqPath, struct cdwValidFile *vf, FILE *bedF,
    double *retMapRatio,  double *retDepth,  double *retSampleCoverage,
    double *retUniqueMapRatio, char *assay);
/* Take a sample fastq and run bwa on it, and then convert that file to a bed. */

void cdwMakeTempFastqSample(char *source, int size, char dest[PATH_LEN]);
/* Copy size records from source into a new temporary dest.  Fills in dest */

void cdwCleanupTrimReads(char *fastqPath, char trimmedPath[PATH_LEN]);
/* Remove trimmed sample file.  Does nothing if fastqPath and trimmedPath the same. */

boolean cdwTrimReadsForAssay(char *fastqPath, char trimmedPath[PATH_LEN], char *assay);
/* Look at assay and see if it's one that needs trimming.  If so make a new trimmed
 * file and put file name in trimmedPath.  Otherwise just copy fastqPath to trimmed
 * path and return FALSE. */

void cdwMakeFastqStatsAndSample(struct sqlConnection *conn, long long fileId);
/* Run fastqStatsAndSubsample, and put results into cdwFastqFile table. */

struct cdwFastqFile *cdwFastqFileFromFileId(struct sqlConnection *conn, long long fileId);
/* Get cdwFastqFile with given fileId or NULL if none such */

struct cdwBamFile * cdwMakeBamStatsAndSample(struct sqlConnection *conn, long long fileId, 
    char sampleBed[PATH_LEN]);
/* Run cdwBamStats and put results into cdwBamFile table, and also a sample bed.
 * The sampleBed will be filled in by this routine. */

struct cdwBamFile *cdwBamFileFromFileId(struct sqlConnection *conn, long long fileId);
/* Get cdwBamFile with given fileId or NULL if none such */

struct cdwQaWigSpot *cdwMakeWigSpot(struct sqlConnection *conn, long long wigId, long long spotId);
/* Create a new cdwQaWigSpot record in database based on comparing wig file to spot file
 * (specified by id's in cdwFile table). */

struct cdwQaWigSpot *cdwQaWigSpotFor(struct sqlConnection *conn, 
    long long wigFileId, long long spotFileId);
/* Return wigSpot relationship if any we have in database for these two files. */

struct cdwVcfFile * cdwMakeVcfStatsAndSample(struct sqlConnection *conn, long long fileId, 
    char sampleBed[PATH_LEN]);
/* Run cdwVcfStats and put results into cdwVcfFile table, and also a sample bed.
 * The sampleBed will be filled in by this routine. */

struct cdwVcfFile *cdwVcfFileFromFileId(struct sqlConnection *conn, long long fileId);
/* Get cdwVcfFile with given fileId or NULL if none such */

char *cdwOppositePairedEndString(char *end);
/* Return "1" for "2" and vice versa */

struct cdwValidFile *cdwOppositePairedEnd(struct sqlConnection *conn, struct cdwFile *ef, struct cdwValidFile *vf);
/* Given one file of a paired end set of fastqs, find the file with opposite ends. */

struct cdwQaPairedEndFastq *cdwQaPairedEndFastqFromVfs(struct sqlConnection *conn,
    struct cdwValidFile *vfA, struct cdwValidFile *vfB,
    struct cdwValidFile **retVf1,  struct cdwValidFile **retVf2);
/* Return pair record if any for the two fastq files. */

void cdwMd5File(char *fileName, char md5Hex[33]);
/* call md5sum utility to calculate md5 for file and put result in hex format md5Hex 
 * This ends up being about 30% faster than library routine md5HexForFile,
 * however since there's popen() weird interactions with  stdin involved
 * it's not suitable for a general purpose library.  Environment inside cdw
 * is controlled enough it should be ok. */

void cdwPathForCommand(char *command, char path[PATH_LEN]);
/* Figure out path associated with command */

void cdwPokeFifo(char *fifoName);
/* Send '\n' to fifo to wake up associated daemon */

FILE *cdwPopen(char *command, char *mode);
/* do popen or die trying */

void cdwOneLineSystemResult(char *command, char *line, int maxLineSize);
/* Execute system command and return one line result from it in line */

boolean cdwOneLineSystemAttempt(char *command, char *line, int maxLineSize);
/* Execute system command and return one line result from it in line */

/***/
/* Shared functions for CDW web CGI's.
   Mostly wrappers for javascript tweaks */

void cdwWebAutoRefresh(int msec);
/* Refresh page after msec.  Use 0 to cancel autorefresh */

/***/
/* Navigation bar */

void cdwWebNavBarStart();
/* Layout navigation bar */

void cdwWebNavBarEnd();
/* Close layout after navigation bar */

void cdwWebBrowseMenuItem(boolean on);
/* Toggle visibility of 'Browse submissions' link on navigation menu */

void cdwWebSubmitMenuItem(boolean on);
/* Toggle visibility of 'Submit data' link on navigation menu */

/***/
/* Metadata queries */

/* Declarations of some structures so don't need all the include files */
struct rqlStatement;
struct tagStorm;
struct tagStanza;

struct tagStorm *cdwTagStorm(struct sqlConnection *conn);
/* Load  cdwMetaTags.tags, cdwFile.tags, and select other fields into a tag
 * storm for searching */

char *cdwLookupTag(struct cgiParsedVars *list, char *tag); 
/* Return first occurence of tag on list, or empty string if not found */

struct tagStorm *cdwUserTagStorm(struct sqlConnection *conn, struct cdwUser *user);
/* Return tag storm just for files user has access to. */

struct tagStorm *cdwUserTagStormFromList(struct sqlConnection *conn, 
    struct cdwUser *user, struct cdwFile *validList ,struct rbTree *groupedFiles);

void cdwCheckRqlFields(struct rqlStatement *rql, struct slName *tagFieldList);
/* Make sure that rql query only includes fields that exist in tags */

char *cdwRqlLookupField(void *record, char *key);
/* Lookup a field in a tagStanza. */

boolean cdwRqlStatementMatch(struct rqlStatement *rql, struct tagStanza *stanza,
	struct lm *lm);
/* Return TRUE if where clause and tableList in statement evaluates true for stanza. */

struct slRef *tagStanzasMatchingQuery(struct tagStorm *tags, char *query);
/* Return list of references to stanzas that match RQL query */

void cdwPrintMatchingStanzas(char *rqlQuery, int limit, struct tagStorm *tags, char *format);
/* Show stanzas that match query */

void cdwPrintSlRefList(struct slRef *results, struct slName *fieldNames, char *format, int limit);
/* Print a linked list of results in ra, tsv, or csv format.  Each result should be a list of
 * slPair key/values. */

struct cgiParsedVars *cdwMetaVarsList(struct sqlConnection *conn, struct cdwFile *ef);
/* Return list of cgiParsedVars dictionaries for metadata for file.  Free this up 
 * with cgiParsedVarsFreeList() */

char *testOriginalSymlink(char *submitFileName, char *submitDir);
/* Follows submitted symlinks to real file.
 * Aborts if real file path starts with cdwRootDir
 * since it should not point to a file already under cdwRoot. */

void replaceOriginalWithSymlink(char *submitFileName, char *submitDir, char *cdwPath);
/* For a file that was just copied, remove original and symlink to new one instead
 * to save space. Follows symlinks if any to the real file and replaces it with a symlink */

int findSubmitSymlinkExt(char *submitFileName, char *submitDir, char **pPath, char **pLastPath, int *pSymlinkLevels);
/* Find the last symlink and real file in the chain from submitDir/submitFileName.
 * This is useful for when target of symlink in cdw/ gets renamed 
 * (e.g. license plate after passes validation), or removed (e.g. cdwReallyRemove* commands). 
 * Returns 0 for success. /
 * Returns -1 if path does not exist. */

char *findSubmitSymlink(char *submitFileName, char *submitDir, char *oldPath);
/* Find the last symlink in the chain from submitDir/submitFileName.
 * This is useful for when target of symlink in cdw/ gets renamed 
 * (e.g. license plate after passes validation), or removed (e.g. cdwReallyRemove* commands). */

void cdwReallyRemoveFile(struct sqlConnection *conn, char *submitDir, long long fileId, boolean unSymlinkOnly, boolean really);
/* Remove all records of file from database and from Unix file system if 
 * the really flag is set.  Otherwise just print some info on the file. */

char *cdwLocalMenuBar(struct cart *cart, boolean makeAbsolute);
/* Return menu bar string. Optionally make links in menubar to point to absolute URLs, not relative. */

char *fileExtFromFormat(char *format);
/* return file extension given the cdwFile format as defined in cdwValid.c. Result has to be freed */

void printMatchingStanzas(char *rqlQuery, int limit, struct tagStorm *tags, char *format);
/* Show stanzas that match query */

#endif /* CDWLIB_H */
