/* dnaseHg38Batch - Create batch to run dnase hotspot runs on hg38 on replicated data. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "obscure.h"
#include "htmshell.h"
#include "../../encodeDataWarehouse/inc/encodeDataWarehouse.h"
#include "../../encodeDataWarehouse/inc/edwLib.h"
#include "eapDb.h"
#include "eapLib.h"
#include "eapGraph.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "dnaseHg38Batch - Create batch to run dnase hotspot runs on hg38 on replicated data\n"
  "usage:\n"
  "   dnaseHg38Batch outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void dnaseHg38Batch(char *outDir)
/* dnaseHg38Batch - Create batch to run dnase hotspot runs on hg38 on replicated data. */
{
struct sqlConnection *conn = eapConnect();
makeDirsOnPath(outDir);
char query[1024];
sqlSafef(query, sizeof(query),
    "select distinct experiment from xf where  ucscDb='%s' and format='bam' "
    " and dataType='DNase-seq' and lab='UW' and deprecated='' and errorMessage='' "
    " and replicate=2 order by experiment", 
	"hg38");
struct slName *exp, *expList = sqlQuickList(conn, query);
uglyf("Got %d replicated DNase-seq experiments from UW\n", slCount(expList));
for (exp = expList; exp != NULL; exp = exp->next)
    {
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s/%s.pooled", outDir, exp->name);
    FILE *f = mustOpen(path, "w");
    sqlSafef(query, sizeof(query), 
        "select licensePlate from xf where ucscDb='%s' and format='bam' "
	" and experiment='%s' and errorMessage='' and deprecated='' "
	, "hg38", exp->name);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    while ((row = sqlNextRow(sr)) != NULL)
        {
	fprintf(f, "%s%s.bam\n", eapEdwCacheDir, row[0]);
	}
    sqlFreeResult(&sr);
    carefulClose(&f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
dnaseHg38Batch(argv[1]);
return 0;
}
