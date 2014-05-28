/* eapLib - library shared by analysis pipeline modules */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "portable.h"
#include "intValTree.h"
#include "../../../../parasol/inc/jobResult.h"
#include "../../../../parasol/inc/paraMessage.h"
#include "../../encodeDataWarehouse/inc/encodeDataWarehouse.h"
#include "../../encodeDataWarehouse/inc/edwLib.h"
#include "eapLib.h"
#include "eapDb.h"

char *eapEdwCacheDir = "/hive/groups/encode/3/eap/cach/";
/* Where data warehouse files are cached in a place that the cluster can access. */

char *eapValDataDir = "/hive/groups/encode/3/encValData/";
/* Where information sufficient to validate a file lives.  This includes genomes of
 * several species indexed for alignment. */

char *eapTempDir = "/hive/groups/encode/3/encodeAnalysisPipeline/tmp/";
/* This temp dir will contain a subdir for each job.  The edwFinish program will
 * remove these if the job went well.  If the job didn't go well they'll probably
 * be empty.  There's some in-between cases though. */

char *eapJobTable = "eapJob";
/* Main analysis job table in encodeDataWarehouse. */

char *eapParaHost = "ku";
/* Parasol host name. A machine running paraHub */

char *eapSshArgs = "-o StrictHostKeyChecking=no -o BatchMode=yes";
/* Arguments to pass to ssh or scp for good performance and security */

struct sqlConnection *eapConnect()
/* Return read-only connection to eap database (which may be same as edw database) */
{
return edwConnect();
}

struct sqlConnection *eapConnectReadWrite()
/* Return read/write connection to eap database, which may be same as edw database) */
{
return edwConnectReadWrite();
}

struct edwUser *eapUserForPipeline(struct sqlConnection *conn)
/* Get user associated with automatic processes and pipeline submissions. */
{
return edwUserFromEmail(conn, edwDaemonEmail);
}

char *eapParaDirs(struct sqlConnection *conn)
/* Root directory to parasol job results queues, where parasol (eventually) stores
 * results of jobs that successfully complete or crash. */
{
static char buf[PATH_LEN];
if (buf[0] == 0)
    {
    safef(buf, sizeof(buf), "%s/%s", "/hive/groups/encode/encode3/eap/queues", 
	edwLicensePlateHead(conn));
    };
return buf;
}


struct paraPstat2Job *eapParasolRunningList(char *paraHost)
/* Return list of running jobs  in paraPstat2Job format. */
{
char cmd[512];
safef(cmd, sizeof(cmd), "pstat2 %s", getUser());
struct paraPstat2Job *jobList = NULL;
struct slName *lineEl, *lineList = pmHubMultilineQuery(cmd, paraHost);
for (lineEl = lineList; lineEl != NULL; lineEl = lineEl->next)
    {
    char *line = lineEl->name;
    char *row[PARAPSTAT2JOB_NUM_COLS];
    int wordCount = chopByWhite(line, row, ArraySize(row));
    if (wordCount == PARAPSTAT2JOB_NUM_COLS && sameString(row[0], "r"))
	{
	struct paraPstat2Job *job = paraPstat2JobLoad(row);
	slAddHead(&jobList, job);
	}
    }
slFreeList(&lineList);
slReverse(&jobList);
return jobList;
}

struct hash *eapParasolRunningHash(char *paraHost, struct paraPstat2Job **retList)
/* Return hash of parasol IDs with jobs running. Hash has paraPstat2Job values.
 * Optionally return list as well as hash */
{
struct hash *runningHash = hashNew(0);
struct paraPstat2Job *job, *jobList = eapParasolRunningList(paraHost);
for (job = jobList; job != NULL; job = job->next)
    hashAdd(runningHash, job->parasolId, job);
if (retList != NULL)
    *retList = jobList;
return runningHash;
}

void eapPathForCommand(char *command, char path[PATH_LEN])
/* Figure out path associated with command */
{
char sysCommand[PATH_LEN*2];
safef(sysCommand, sizeof(sysCommand), "bash -c 'which %s'", command);
edwOneLineSystemResult(sysCommand, path, PATH_LEN);
eraseTrailingSpaces(path);
}


char *eapStepFromCommandLine(char *commandLine)
/* Given command line looking like 'edwCdJob step parameters to program' return 'step' */
{
int commandSize = strlen(commandLine);
char dupeCommand[commandSize+1];
strcpy(dupeCommand, commandLine);
char *line = dupeCommand;
char *word1 = nextWord(&line);
char *word2 = nextWord(&line);

/* Figure out real command in there */
char *command = word2;
if (word2 == NULL || !sameString(word1, "edwCdJob"))// We want to be alerted if command line changes
    {
    warn("Oh no, eapSchedule changed on us?");
    command = word1;
    }
return cloneString(command);
}

struct eapStep *eapStepFromName(struct sqlConnection *conn, char *name)
/* Get eapStep record from database based on name. */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from eapStep where name = '%s'", name);
return eapStepLoadByQuery(conn, query);
}

struct eapStep *eapStepFromNameOrDie(struct sqlConnection *conn, char *analysisStep)
/* Get analysis step of given name, or complain and die. */
{
struct eapStep *step = eapStepFromName(conn, analysisStep);
if (step == NULL)
    errAbort("Can't find %s in eapStep table", analysisStep);
return step;
}

struct eapSoftware *eapSoftwareFromName(struct sqlConnection *conn, char *name)
/* Get eapSoftware record by name */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from eapSoftware where name = '%s'", name);
return eapSoftwareLoadByQuery(conn, query);
}

void eapMd5ForCommand(char *command, char md5[33])
/* Figure out md5 associated with command */
{
char path[PATH_LEN];
eapPathForCommand(command, path);
edwMd5File(path, md5);
}

static void checkMd5OnCommand(char *usedIn, char *command, char *md5Hex)
/* Basically do a 'which' to find path, and then calc md5sum.
 * The usedIn parameter is just for context during error reporting. It
 * is otherwise ignored.*/
{
char path[PATH_LEN];
eapPathForCommand(command, path);
char md5[33];
edwMd5File(path, md5);
if (!sameString(md5, md5Hex))
    {
    errAbort("Need to update eapSoftware %s used in %s\nOld md5 %s, new md5 %s",
	command, usedIn, md5Hex, md5);
    }
}

unsigned eapCheckVersions(struct sqlConnection *conn, char *stepName)
/* Check that we are running tracked versions of everything. */
{
/* Figure out current version of step. */
char query[512];
sqlSafef(query, sizeof(query), "select max(id) from eapStepVersion where step='%s'", stepName);
long long stepVersionId = sqlQuickLongLong(conn, query);

/* Get list of eapStepSoftware corresponding to that step. */
sqlSafef(query, sizeof(query), "select * from eapStepSoftware where step='%s'", stepName);
struct eapStepSoftware *ss, *ssList = eapStepSoftwareLoadByQuery(conn, query);

for (ss = ssList; ss != NULL; ss = ss->next)
    {
    sqlSafef(query, sizeof(query), 
	"select * from eapSwVersion where software='%s' order by id desc limit 1", ss->software);
    struct eapSwVersion *sv = eapSwVersionLoadByQuery(conn, query);
    checkMd5OnCommand(ss->step, sv->software, sv->md5);
    }
eapStepSoftwareFreeList(&ssList);
return stepVersionId;
}

void eapSoftwareUpdateMd5ForStep(struct sqlConnection *conn, char *analysisStep)
/* Update MD5s on all software used by step. */
{
uglyAbort("I should get to this soon.");
}

int eapJobAdd(struct sqlConnection *conn, char *commandLine, int cpusRequested)
/* Add job to edwAnalyisJob table and return job ID. */
{
struct eapJob job =
   {
   .commandLine = commandLine,
   .cpusRequested = cpusRequested
   };
eapJobSaveToDb(conn, &job, "eapJob", 0);
return sqlLastAutoId(conn);
}

static boolean stepVersionCheck(struct sqlConnection *conn, struct eapStepVersion *sv, 
    struct eapStepSoftware *ssList)
/* Return TRUE if all the software in step checks out as the latest version */
{
/* Make indexed bundle of eapStepSwVersion keyed by swVersionId */
struct rbTree *bundle = intValTreeNew();
char query[512];
sqlSafef(query,sizeof(query),
    "select * from eapStepSwVersion where stepVersionId=%u", sv->id);
struct eapStepSwVersion *ssv, *ssvList = eapStepSwVersionLoadByQuery(conn, query);
for (ssv = ssvList; ssv != NULL; ssv = ssv->next)
    {
    intValTreeAdd(bundle, ssv->swVersionId, ssv);
    }

boolean allMatch = TRUE;
struct eapStepSoftware *ss;
for (ss = ssList; ss != NULL; ss = ss->next)
    {
    /* Get latest version. */
    sqlSafef(query, sizeof(query), 
	   "select max(id) from eapSwVersion where software='%s'", ss->software);
    unsigned softwareVersion = sqlQuickNum(conn, query);

    /* If no latest version or latest version not recorded, then we'll need a new version. */
    if (softwareVersion == 0 || intValTreeLookup(bundle, softwareVersion) == NULL)
	{
	allMatch = FALSE;
	break;
	}
    }
eapStepSwVersionFreeList(&ssvList);
intValTreeFree(&bundle);
return allMatch;
}

static unsigned updateStep(struct sqlConnection *conn, char *stepName, 
    struct eapStepSoftware *ssList)
/* Update database making a new step version which is returned.  Also
 * associate latest version of all software with this step version */
{
/* Figure out current version, which may end up 0 if no version at all yet. */
char query[512];
sqlSafef(query, sizeof(query), "select max(version) from eapStepVersion where step='%s'", stepName);
int version = sqlQuickNum(conn, query) + 1;

/* Insert new stepVersion into table and grad id */
sqlSafef(query, sizeof(query), 
    "insert eapStepVersion(step,version) values ('%s',%d)", stepName, version);
sqlUpdate(conn, query);
unsigned stepVersionId = sqlLastAutoId(conn);

/* Make connections with all software */
struct eapStepSoftware *ss;
for (ss = ssList; ss != NULL; ss = ss->next)
    {
    sqlSafef(query, sizeof(query),
        "select max(id) from eapSwVersion where software='%s'", ss->software);
    unsigned swVersionId = sqlQuickNum(conn, query);
    if (swVersionId == 0) 
	internalErr();
    sqlSafef(query, sizeof(query),
        "insert eapStepSwVersion (stepVersionId,swVersionId) values (%u,%u)"
	, stepVersionId, swVersionId);
    sqlUpdate(conn, query);
    }
return stepVersionId;
}


unsigned eapCurrentStepVersion(struct sqlConnection *conn, char *stepName)
/* Returns current version (id in eapStepVersion) for step.  
 * Behind the scenes it checks the software used by step against the current
 * step version.  If any has changed (or if no version yet exists) it creates
 * a new version and returns it.  In case of no change it just returns id
 * of currently tracked version. */
{
/* Get current version of step */
char query[512];
sqlSafef(query,sizeof(query), 
    "select * from eapStepVersion where step='%s' order by version desc limit 1", stepName);
struct eapStepVersion *sv = eapStepVersionLoadByQuery(conn, query);


/* Get list of all software used by the step */
sqlSafef(query, sizeof(query), "select * from eapStepSoftware where step='%s'", stepName);
struct eapStepSoftware *ssList = eapStepSoftwareLoadByQuery(conn, query);

/* Figure out if need to make a new step version.  This will be true if anything changes. */
boolean needNewVersion = TRUE;
if (sv != NULL)
    {
    if (stepVersionCheck(conn, sv, ssList))
	needNewVersion = FALSE;
    }

/* Update step if need be and in any case get set to return current version */
int curVersion;
if (needNewVersion)
    curVersion = updateStep(conn, stepName, ssList);
else
    curVersion = sv->id;

/* Clean up and go home. */
eapStepSoftwareFreeList(&ssList);
eapStepVersionFree(&sv);
return curVersion;
}
