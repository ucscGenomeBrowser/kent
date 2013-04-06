/* edwMakeEnrichments - Scan through database and make a makefile to calc. enrichments and 
 * store in database.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "cheapcgi.h"
#include "pipeline.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwMakeEnrichments - Scan through database and make a makefile to calc. enrichments and \n"
  "store in database.\n"
  "usage:\n"
  "   edwMakeEnrichments startFileId endFileId\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void doFastqEnrichmentPipeline(struct edwFile *ef, char *fastqPath, struct edwValidFile *vf, 
    char *enriched)
{
char *samplePath = vf->samplePath;
assert(samplePath != NULL);

uglyf("Running super complex pipeline\n");
uglyf("TODO: remove hardcoded scratch path\n");
uglyf("Somehow should be running bwa with %s vs reads in %s\n", vf->ucscDb, samplePath);

/* Set up pipeline to run starting on from encMakeValidFile pass.  The pipeline will produce a 
 * sam file which we just write to output. Doing it in a pipe lets us avoid yet another
 * temp file, and helps bwa go a bit faster. */
char genoFile[PATH_LEN];
safef(genoFile, sizeof(genoFile), "/scratch/kent/encValData/%s/bwaData/%s.fa", 
    vf->ucscDb, vf->ucscDb);
char *cmd1[] = {"bwa", "aln", genoFile, samplePath, NULL,};
char *cmd2[] = {"bwa", "samse", genoFile, "-", samplePath, NULL,};
char **cmds[] = {cmd1, cmd2, NULL};
struct pipeline *pl = pipelineOpen(cmds, pipelineRead, NULL, NULL);
FILE *s = pipelineFile(pl);

/* Set up a temp file for sam output. */
char samFileName[PATH_LEN];
safef(samFileName, PATH_LEN, "%sedwSamXXXXXX", edwTempDir());
int samFd = mkstemp(samFileName);
FILE *d = fdopen(samFd, "w");

/* Copy pipeline output to sam temp file. */
int c;
while ((c = fgetc(s)) != EOF)
     fputc(c, d);
carefulClose(&d);

/* Wait for pipeline to end. */
int err = pipelineWait(pl);
uglyf("Past the wait err = %d\n", err);
pipelineFree(&pl);
uglyf("Past the free\n");

uglyf(">>>please check out sam files in %s\n", samFileName);
}

void doEnrichments(struct sqlConnection *conn, struct edwFile *ef, char *path,
    char *format, char *enriched)
/* Calculate enrichments on file. */
{
char query[256];
safef(query, sizeof(query), "select * from edwValidFile where fileId=%d", ef->id);
struct edwValidFile *vf = edwValidFileLoadByQuery(conn, query);
if (vf == NULL)
    return;	/* We can only work if have validFile table entry */
if (sameString(format, "fastq"))
    doFastqEnrichmentPipeline(ef, path, vf, enriched);
else if (sameString(format, "bigWig"))
    verbose(2,"Got bigWig\n");
else if (sameString(format, "bigBed"))
    verbose(2,"Got bigBed\n");
else if (sameString(format, "narrowPeak"))
    verbose(2,"Got narrowPeak.bigBed\n");
else if (sameString(format, "broadPeak"))
    verbose(2,"Got broadPeak.bigBed\n");
else if (sameString(format, "bam"))
    verbose(2,"Got bam\n");
else if (sameString(format, "unknown"))
    verbose(2, "Unknown format in doEnrichments(%s), that's chill.", ef->edwFileName);
else
    errAbort("Unrecognized format %s in doEnrichments(%s)", format, path);

}

void edwMakeEnrichments(int startFileId, int endFileId)
/* edwMakeEnrichments - Scan through database and make a makefile to calc. enrichments and store 
 * in database. */
{
/* Make list with all files in ID range */
struct sqlConnection *conn = sqlConnect(edwDatabase);
char query[256];
safef(query, sizeof(query), 
    "select * from edwFile where id>=%d and id<=%d and md5 != '' and deprecated = ''", 
    startFileId, endFileId);
struct edwFile *file, *fileList = edwFileLoadByQuery(conn, query);
uglyf("doing %d files with id's between %d and %d\n", slCount(fileList), startFileId, endFileId);

for (file = fileList; file != NULL; file = file->next)
    {
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s%s", edwRootDir, file->edwFileName);
    uglyf("processing %s aka %s\n", file->submitFileName, path);

    if (file->tags) // All ones we care about have tags
        {
	struct cgiParsedVars *tags = cgiParsedVarsNew(file->tags);
	char *format = hashFindVal(tags->hash, "format");
	char *enriched = hashFindVal(tags->hash, "enriched_in");
	if (format != NULL && enriched != NULL)
	    {
	    doEnrichments(conn, file, path, format, enriched);
	    }
	cgiParsedVarsFree(&tags);
	}
    }

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwMakeEnrichments(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
