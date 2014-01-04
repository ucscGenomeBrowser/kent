/* eapLib - library shared by analysis pipeline modules */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "eapLib.h"

char *eapTempDir = "/hive/groups/encode/encode3/encodeAnalysisPipeline/tmp/";
char *eapEdwCacheDir = "/hive/groups/encode/encode3/encodeAnalysisPipeline/edwCache/";
char *eapValDataDir = "/hive/groups/encode/encode3/encValData/";


