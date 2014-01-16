/* eapLib - library shared by analysis pipeline modules */

extern char *eapEdwCacheDir;
/* Where data warehouse files are cached in a place that the cluster can access. */

extern char *eapValDataDir;
/* Where information sufficient to validate a file lives.  This includes genomes of
 * several species indexed for alignment. */

extern char *eapTempDir;
/* This temp dir is not heavily used. */

extern char *eapJobTable;
/* Main analysis job table in encodeDataWarehouse. */

extern char *eapParaHost;
/* Parasol host name. A machine running paraHub */

extern char *eapParaQueues;
/* Root directory to parasol job results queues, where parasol (eventually) stores
 * results of jobs that successfully complete or crash. */

