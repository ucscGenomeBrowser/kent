/* eapLib - library shared by analysis pipeline modules */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "portable.h"
#include "../../../../parasol/inc/jobResult.h"
#include "../../../../parasol/inc/paraMessage.h"
#include "eapLib.h"

char *eapEdwCacheDir = "/hive/groups/encode/encode3/encodeAnalysisPipeline/edwCache/";
/* Where data warehouse files are cached in a place that the cluster can access. */

char *eapValDataDir = "/hive/groups/encode/encode3/encValData/";
/* Where information sufficient to validate a file lives.  This includes genomes of
 * several species indexed for alignment. */

char *eapTempDir = "/hive/groups/encode/encode3/encodeAnalysisPipeline/tmp/";
/* This temp dir will contain a subdir for each job.  The edwFinish program will
 * remove these if the job went well.  If the job didn't go well they'll probably
 * be empty.  There's some in-between cases though. */

char *eapJobTable = "edwAnalysisJob";
/* Main analysis job table in encodeDataWarehouse. */

char *eapParaHost = "ku";
/* Parasol host name. A machine running paraHub */

char *eapParaQueues = "/hive/groups/encode/encode3/encodeAnalysisPipeline/queues";
/* Root directory to parasol job results queues, where parasol (eventually) stores
 * results of jobs that successfully complete or crash. */

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
    warn("Oh no, edwScheduleJob changed on us?");
    command = word1;
    }
return cloneString(command);
}

