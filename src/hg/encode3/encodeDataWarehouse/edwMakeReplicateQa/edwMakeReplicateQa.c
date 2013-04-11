/* edwMakeReplicateQa - Do qa level comparisons of replicates.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "jksql.h"
#include "genomeRangeTree.h"
#include "bigWig.h"
#include "bigBed.h"
#include "bamFile.h"
#include "portable.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwMakeReplicateQa - Do qa level comparisons of replicates.\n"
  "usage:\n"
  "   edwMakeReplicateQa startId endId\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct edwValidFile *findElderReplicates(struct sqlConnection *conn, struct edwValidFile *vf)
/* Find all replicates of same output and format type for experiment that are elder
 * (fileId less than your file Id).  Younger replicates are responsible for taking care 
 * of correlations with older ones.  Sorry younguns, it's like social security. */
{
char query[256];
safef(query, sizeof(query), 
    "select * from edwValidFile where fileId<%d and experiment='%s' and format='%s'"
    " and outputType='%s'"
    , vf->fileId, vf->experiment, vf->format, vf->outputType);
return edwValidFileLoadByQuery(conn, query);
}

void doReplicatePair(struct sqlConnection *conn, 
    struct edwFile *elderEf, struct edwValidFile *elderVf,
    struct edwFile *youngerEf, struct edwValidFile *youngerVf)
/* Do processing on a pair of replicates. */
{
uglyf("And now for the show:\n\t%s\t%s\n", elderEf->submitFileName, youngerEf->submitFileName);
}

void doReplicateQa(struct sqlConnection *conn, struct edwFile *ef)
/* Try and do replicate level QA - find matching file and do correlation-like
 * things. */
{
/* Get validated file info.  If not validated we don't bother. */
struct edwValidFile *vf = edwValidFileFromFileId(conn, ef->id);
if (vf == NULL)
    return;

char *replicate = vf->replicate;
if (!isEmpty(replicate) && !sameString(replicate, "both"))
    {
    /* Try to find other replicates of same experiment, format, and output type. */
    struct edwValidFile *elder, *elderList = findElderReplicates(conn, vf);

    for (elder = elderList; elder != NULL; elder = elder->next)
        {
	doReplicatePair(conn, edwFileFromIdOrDie(conn, elder->fileId), elder, ef, vf);
	}
    }
edwValidFileFree(&vf);
}

void edwMakeReplicateQa(int startId, int endId)
/* edwMakeReplicateQa - Do qa level comparisons of replicates.. */
{
/* Make list with all files in ID range */
struct sqlConnection *conn = sqlConnect(edwDatabase);
char query[256];
safef(query, sizeof(query), 
    "select * from edwFile where id>=%d and id<=%d and endUploadTime != 0 "
    "and updateTime != 0 and deprecated = ''", 
    startId, endId);
struct edwFile *ef, *efList = edwFileLoadByQuery(conn, query);

for (ef = efList; ef != NULL; ef = ef->next)
    {
    doReplicateQa(conn, ef);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwMakeReplicateQa(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
