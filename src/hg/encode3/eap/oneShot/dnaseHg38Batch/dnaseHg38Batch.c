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

void oneRep(char *outDir, FILE *jobFile, struct sqlConnection *conn,
    char *experiment, char *repQuery, char *repName)
/* Make output file for experiment and replicate */
{
char query[1024];
sqlSafef(query, sizeof(query), 
    "select licensePlate from xf where ucscDb='%s' and format='bam' "
    " and experiment='%s' and errorMessage='' and deprecated=''%s"
    , "hg38", experiment, repQuery);
struct slName *fileList = sqlQuickList(conn, query);
if (fileList != NULL)
    {
    char expRep[PATH_LEN];
    safef(expRep, sizeof(expRep), "xyz/%s_%s", experiment, repName);
    if (fileList->next != NULL)  // Multiple file case
        {
	char path[PATH_LEN];
	safef(path, sizeof(path), "%s/%s_%s.lst", outDir, experiment, repName);
	FILE *f = mustOpen(path, "w");
	struct slName *file;
	for (file = fileList; file != NULL; file = file->next)
	    {
	    fprintf(f, "%s%s.bam\n", eapEdwCacheDir, file->name);
	    }
	carefulClose(&f);
	fprintf(jobFile, 
	    "edwCdJob eap_pool_hotspot hg38 %s.lst 36 %s.narrowPeak %s.broadPeak %s.bigWig\n",
	    expRep, expRep, expRep, expRep);
	}
    else  // Single file case
        {
	fprintf(jobFile, 
	    "edwCdJob eap_run_hotspot hg38 %s%s.bam 36 %s.narrowPeak %s.broadPeak %s.bigWig\n",
	    eapEdwCacheDir, fileList->name, expRep, expRep, expRep);
	}
    slFreeList(&fileList);
    }
}

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
char jobListFile[PATH_LEN];
safef(jobListFile, sizeof(jobListFile), "%s/%s", outDir, "jobList");
FILE *f = mustOpen(jobListFile, "w");
for (exp = expList; exp != NULL; exp = exp->next)
    {
    oneRep(outDir, f, conn, exp->name, "", "pooled");
    oneRep(outDir, f, conn, exp->name, "and replicate='1'", "1");
    oneRep(outDir, f, conn, exp->name, "and replicate='2'", "2");
    }
carefulClose(&f);

char metaFile[PATH_LEN];
safef(metaFile, sizeof(metaFile), "%s/%s", outDir, "meta.tab");
f = mustOpen(metaFile, "w");
fprintf(f, "#accession\tbiosample\tsex\ttaxon\n");
for (exp = expList; exp != NULL; exp = exp->next)
    {
    sqlSafef(query, sizeof(query),
       "select biosample,sex,taxon  from edwExperiment,edwBiosample "
       "where edwExperiment.biosample = edwBiosample.term "
       "and edwExperiment.accession = '%s' and sex != 'B'", exp->name);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    while ((row = sqlNextRow(sr)) != NULL)
	fprintf(f, "%s\t%s\t%s\t%s\n", exp->name, row[0], row[1], row[2]);
    sqlFreeResult(&sr);
    }
carefulClose(&f);
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
