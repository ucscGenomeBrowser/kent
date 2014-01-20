/* eapLib - library shared by analysis pipeline modules */

extern char *eapEdwCacheDir;
/* Where data warehouse files are cached in a place that the cluster can access. */

extern char *eapValDataDir;
/* Where information sufficient to validate a file lives.  This includes genomes of
 * several species indexed for alignment. */

extern char *eapTempDir;
/* This temp dir will contain a subdir for each job.  The edwFinish program will
 * remove these if the job went well.  If the job didn't go well they'll probably
 * be empty.  There's some in-between cases though. */

extern char *eapJobTable;
/* Main analysis job table in encodeDataWarehouse. */

extern char *eapParaHost;
/* Parasol host name. A machine running paraHub */

extern char *eapParaQueues;
/* Root directory to parasol job results queues, where parasol (eventually) stores
 * results of jobs that successfully complete or crash. */

struct paraPstat2Job *eapParasolRunningList(char *paraHost);
/* Return list of running jobs  in paraPstat2Job format. */

struct hash *eapParasolRunningHash(char *paraHost, struct paraPstat2Job **retList);
/* Return hash of parasol IDs with jobs running. Hash has paraPstat2Job values.
 * Optionally return list as well as hash */

char *eapStepFromCommandLine(char *commandLine);
/* Given command line looking like 'edwCdJob step parameters to program' return 'step' */
