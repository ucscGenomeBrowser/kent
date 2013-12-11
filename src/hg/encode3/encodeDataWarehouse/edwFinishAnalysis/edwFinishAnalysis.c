/* edwFinishAnalysis - Look for analysis jobs that have completed and integrate results into 
 * database. */
#include <uuid/uuid.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "cheapcgi.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "encode3/encode3Valid.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFinishAnalysis - Look for analysis jobs that have completed and integrate results into\n"
  "database.\n"
  "usage:\n"
  "   edwFinishAnalysis now\n"
  "Where 'now' is just a parameter that is ignored for now.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

FILE *edwPopen(char *command, char *mode)
/* do popen or die trying */
{
/* Because of bugs with popen(...,"r") and programs that use stdin otherwise
 * it's probably better to use Mark's pipeline library,  but it is ever so
 * much harder to use... */
FILE *f = popen(command,  mode);
if (f == NULL)
    errnoAbort("Can't popen(%s, %s)", command, mode);
return f;
}

void edwPclose(FILE **pF)
/* Close pipe file or die trying */
{
FILE *f = *pF;
if (f != NULL)
    {
    int err = pclose(f);
    if (err != 0)
        errnoAbort("Can't pclose(%p)", f);
    *pF = NULL;
    }
}

void edwMd5File(char *fileName, char md5Hex[33])
/* call md5sum utility to calculate md5 for file and put result in hex format md5Hex 
 * This ends up being about 30% faster than library routine md5HexForFile,
 * however since there's popen() weird interactions with  stdin involved
 * it's not suitable for a general purpose library.  Environment inside edw
 * is controlled enough it should be ok. */
{
char command[PATH_LEN + 16];
safef(command, sizeof(command), "md5sum %s", fileName);
FILE *f = edwPopen(command, "r");
char line[2*PATH_LEN];
if (fgets(line, sizeof(line), f) == NULL)
    errAbort("Can't get line from %s", command);
memcpy(md5Hex, line, 32);
md5Hex[32] = 0;
edwPclose(&f);
}

static char *sharedVal(struct cgiDictionary *dictList, char *var)
/* Return value if var is present and identical in all members of dictList */
{
char *val = NULL;
struct cgiDictionary *dict;
for (dict = dictList; dict != NULL; dict = dict->next)
    {
    struct cgiVar *cgi = hashFindVal(dict->hash, var);
    if (cgi == NULL)
        return NULL;
    char *newVal = cgi->val;
    if (val == NULL)
        val = newVal;
    else if (!sameString(val, newVal))
        return NULL;
    }
return val;
}

char *fakeTags(struct sqlConnection *conn, struct edwAnalysisRun *run, int fileIx, char *validKey)
/* Return tags for out output */
{
/* Fill in tags we actually know will be there */
struct dyString *dy = dyStringNew(0);
cgiEncodeIntoDy("format", run->outputFormats[fileIx], dy);
cgiEncodeIntoDy("valid_key", validKey, dy);
char outputType[FILENAME_LEN];
splitPath(run->outputFiles[fileIx], NULL, outputType, NULL);
cgiEncodeIntoDy("output_type", outputType, dy);

/* Next set up stuff to handle variable parts we only put in if all inputs agree. */

/* Get list of input files */
struct edwFile *ef, *efList = NULL;
int i;
for (i=0; i<run->inputFileCount; ++i)
    {
    ef = edwFileFromId(conn, run->inputFiles[i]);
    slAddTail(&efList, ef);
    }

/* Make list of dictionaries corresponding to list of files. */
struct cgiDictionary *dict, *dictList = NULL;
for (ef = efList; ef != NULL; ef = ef->next)
    {
    dict = cgiDictionaryFromEncodedString(ef->tags);
    slAddTail(&dictList, dict);
    }

/* Loop through fields that might be shared putting them into tags if they are. */
static char *fakeIfShared[] = {"experiment", "replicate", "enriched_in", 
    "technical_replicate", "paired_end", };
for (i=0; i<ArraySize(fakeIfShared); ++i)
    {
    char *var = fakeIfShared[i];
    char *val = sharedVal(dictList, var);
    if (val != NULL)
        cgiEncodeIntoDy(var, val, dy);
    }

/* Clean up and go home */
edwFileFreeList(&efList);
cgiDictionaryFreeList(&dictList);
return dyStringCannibalize(&dy);
}

void finishGoodRun(struct sqlConnection *conn, struct edwAnalysisRun *run)
/* Looks like the job for the run completed successfully, so let's grab results
 * and store them permanently */
{
uglyf("Theoretically finishing good run in %s\n", run->tempDir);

/* Generate 16 bytes of random sequence with uuid generator */
unsigned char uu[16];
uuid_generate(uu);
uuid_unparse(uu, run->uuid);
uglyf("uuid %s\n", run->uuid);

/* Generate initial edwRun records for each of the file outputs. */
struct edwFile *ef, *efList = NULL;
int i;
for (i=0; i<run->outputFileCount; ++i)
    {
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s/%s", run->tempDir, run->outputFiles[i]);
    uglyf("processing %s\n", path);

    /* Make most of edwFile record */
    AllocVar(ef);
    ef->submitFileName = cloneString(path);
    ef->size = fileSize(path);
    edwMd5File(path, ef->md5);
    char *validKey = encode3CalcValidationKey(ef->md5, ef->size);
    ef->tags = fakeTags(conn, run, i, validKey);
    ef->startUploadTime = edwNow();
    ef->edwFileName = ef->errorMessage = ef->deprecated = "";

    /* Save to database so get file ID */
    edwFileSaveToDb(conn, ef, "edwFile", 0);
    ef->id = sqlLastAutoId(conn);

    /* Build up file name based on ID and rename Record time to rename as upload time. */
    char babyName[PATH_LEN], edwFileName[PATH_LEN];
    edwMakeFileNameAndPath(ef->id, ef->submitFileName, babyName, edwFileName);
    ef->edwFileName = cloneString(edwFileName);
    mustRename(path, edwFileName);
    ef->endUploadTime = edwNow();

    /* Update last few fields in database. */
    char query[PATH_LEN*2];
    sqlSafef(query, sizeof(query),
	"update edwFile set endUploadTime=%lld, edwFileName='%s' where id=%u",
	ef->endUploadTime, ef->edwFileName, ef->id);
    uglyf("%s\n", query);
    sqlUpdate(conn, query);

    slAddTail(&efList, ef);
    }
}

void edwFinishAnalysis(char *how)
/* edwFinishAnalysis - Look for analysis jobs that have completed and integrate results into 
 * database. */
{
struct sqlConnection *conn = edwConnectReadWrite();
char query[512];
sqlSafef(query, sizeof(query), "select * from edwAnalysisRun where complete = 0");
struct edwAnalysisRun *run, *runList = edwAnalysisRunLoadByQuery(conn, query);
verbose(1, "Got %d unfinished analysis\n", slCount(runList));

for (run = runList; run != NULL; run = run->next)
    {
    sqlSafef(query, sizeof(query), "select * from edwAnalysisJob where id=%u", run->jobId);
    struct edwAnalysisJob *job = edwAnalysisJobLoadByQuery(conn, query);
    char *command = job->commandLine;
    if (job->endTime > 0)
        {
	if (job->returnCode == 0)
	    {
	    verbose(2, "succeeded %s\n", command);
	    finishGoodRun(conn, run);
	    }
	else
	    {
	    verbose(1, "failed %s\n", command);
	    sqlSafef(query, sizeof(query), 
		"update edwAnalysisRun set complete = -1 where id=%u", run->id);
	    sqlUpdate(conn, query);
	    }
	}
    else
        {
	verbose(2, "running %s\n", command);
	}
    uglyAbort("All for now");
    };
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
edwFinishAnalysis(argv[1]);
return 0;
}
