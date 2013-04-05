/* edwMakeValidFile - Add range of ids to valid file table. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "bigWig.h"
#include "bigBed.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwMakeValidFile - Add range of ids to valid file table.\n"
  "usage:\n"
  "   edwMakeValidFile startId endId\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void makeValidFastq( struct sqlConnection *conn, char *path, struct edwFile *eFile, 
	struct edwAssembly *assembly, struct edwValidFile *vf)
/* Fill out fields of vf.  Create sample subset. */
{
uglyf("Theoritically I would be makeValidFastq(%s)\n", eFile->submitFileName);
}

void makeValidBigBed( struct sqlConnection *conn, char *path, struct edwFile *eFile, 
	struct edwAssembly *assembly, char *format, struct edwValidFile *vf)
{
struct bbiFile *bbi = bigBedFileOpen(path);
vf->sampleCount = vf->itemCount = bigBedItemCount(bbi);
struct bbiSummaryElement sum = bbiTotalSummary(bbi);
vf->basesInSample = vf->basesInItems = sum.sumData;
vf->sampleCoverage = (double)sum.validCount/assembly->baseCount;
vf->depth = (double)sum.sumData/assembly->baseCount;
bigBedFileClose(&bbi);
}

void makeValidBigWig( struct sqlConnection *conn, char *path, struct edwFile *eFile, 
	struct edwAssembly *assembly, char *format, struct edwValidFile *vf)
{
uglyf("Theoritically I would be makeValidBigWig(%s, %s)\n", eFile->submitFileName, format);
}

void makeValidBam( struct sqlConnection *conn, char *path, struct edwFile *eFile, 
	struct edwAssembly *assembly, char *format, struct edwValidFile *vf)
{
uglyf("Theoritically I would be makeValidBam(%s, %s)\n", eFile->submitFileName, format);
}

void makeValidFile(struct sqlConnection *conn, struct edwFile *eFile, struct cgiParsedVars *tags)
/* If possible make a edwValidFile record for this.  Makes sure all the right tags are there,
 * and then parses file enough to determine itemCount and the like.  For some files, like fastqs,
 * it will take a subset of the file as a sample so can do QA without processing the whole thing. */
{
struct edwValidFile *vf;
AllocVar(vf);
strcpy(vf->licensePlate, eFile->licensePlate);
vf->fileId = eFile->id;
vf->format = hashFindVal(tags->hash, "format");
vf->outputType = hashFindVal(tags->hash, "output_type");
vf->experiment = hashFindVal(tags->hash, "experiment");
vf->replicate = hashFindVal(tags->hash, "replicate");
vf->validKey = hashFindVal(tags->hash, "valid_key");
vf->enrichedIn = hashFindVal(tags->hash, "enriched_in");
vf->ucscDb = hashFindVal(tags->hash, "ucsc_db");
vf->samplePath = "";
if (vf->format && vf->outputType && vf->experiment && vf->replicate && 
	vf->validKey && vf->enrichedIn && vf->ucscDb)
    {
    /* Look up assembly. */
    char *ucscDb = vf->ucscDb;
    char query[256];
    safef(query, sizeof(query), "select * from edwAssembly where ucscDb='%s'", vf->ucscDb);
    struct edwAssembly *assembly = edwAssemblyLoadByQuery(conn, query);
    if (assembly == NULL)
        errAbort("Couldn't find assembly corresponding to %s", ucscDb);

    /* Make path to file */
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s%s", edwRootDir, eFile->edwFileName);

    /* And dispatch according to format. */
    char *format = vf->format;
    if (sameString(format, "fastq"))
	makeValidFastq(conn, path, eFile, assembly, vf);
    else if (sameString(format, "broadPeak") || sameString(format, "narrowPeak") || 
	     sameString(format, "bigBed"))
	{
	makeValidBigBed(conn, path, eFile, assembly, format, vf);
	}
    else if (sameString(format, "bigWig"))
        {
	makeValidBigWig(conn, path, eFile, assembly, format, vf);
	}
    else if (sameString(format, "bam"))
        {
	makeValidBam(conn, path, eFile, assembly, format, vf);
	}
    else if (sameString(format, "unknown"))
        {
	}
    else
        {
	errAbort("Unrecognized format %s for %s\n", format, eFile->edwFileName);
	}

    /* Create edwValidRecord with our result */
    edwValidFileSaveToDb(conn, vf, "edwValidFile", 512);
    }
}

void edwMakeValidFile(int startId, int endId)
/* edwMakeValidFile - Add range of ids to valid file table.. */
{
/* Make list with all files in ID range */
struct sqlConnection *conn = sqlConnect(edwDatabase);
char query[256];
safef(query, sizeof(query), 
    "select * from edwFile where id>=%d and id<=%d and md5 != '' and deprecated = ''", 
    startId, endId);
struct edwFile *eFile, *eFileList = edwFileLoadByQuery(conn, query);
uglyf("doing %d files with id's between %d and %d\n", slCount(eFileList), startId, endId);

for (eFile = eFileList; eFile != NULL; eFile = eFile->next)
    {
    uglyf("processing %s %s\n", eFile->licensePlate, eFile->submitFileName);
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s%s", edwRootDir, eFile->edwFileName);

    if (eFile->tags) // All ones we care about have tags
        {
	struct cgiParsedVars *tags = cgiParsedVarsNew(eFile->tags);
	makeValidFile(conn, eFile, tags);
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
edwMakeValidFile(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
