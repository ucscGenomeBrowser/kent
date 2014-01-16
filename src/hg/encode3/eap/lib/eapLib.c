/* eapLib - library shared by analysis pipeline modules */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "eapLib.h"

char *eapEdwCacheDir = "/hive/groups/encode/encode3/encodeAnalysisPipeline/edwCache/";
/* Where data warehouse files are cached in a place that the cluster can access. */

char *eapValDataDir = "/hive/groups/encode/encode3/encValData/";
/* Where information sufficient to validate a file lives.  This includes genomes of
 * several species indexed for alignment. */

char *eapTempDir = "/hive/groups/encode/encode3/encodeAnalysisPipeline/tmp/";
/* This temp dir is not heavily used. */

char *eapJobTable = "edwAnalysisJob";
/* Main analysis job table in encodeDataWarehouse. */

char *eapParaHost = "ku";
/* Parasol host name. A machine running paraHub */

char *eapParaQueues = "/hive/groups/encode/encode3/encodeAnalysisPipeline/queues";
/* Root directory to parasol job results queues, where parasol (eventually) stores
 * results of jobs that successfully complete or crash. */


