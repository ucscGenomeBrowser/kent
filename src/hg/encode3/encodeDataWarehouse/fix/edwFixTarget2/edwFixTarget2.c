/* edwFixTarget2 - Fix ucsc_db/ucscDb tag on some macs2 generated files in test database. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFixTarget2 - Fix ucsc_db/ucscDb tag on some macs2 generated files in test database.\n"
  "usage:\n"
  "   edwFixTarget2 out.sql\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void deprecate(FILE *f, long long fileId, char *why)
/* Deprecate file for given reason */
{
fprintf(f, 
    "update edwFile set deprecated='%s' where id=%lld;\n", why, fileId);
}

void edwFixTarget2(char *outSql)
/* edwFixTarget2 - Fix ucsc_db/ucscDb tag on some macs2 generated files in test database.. */
{
FILE *fSql = mustOpen(outSql, "w");
struct sqlConnection *conn = edwConnect();
struct edwAssembly *targetAssembly = edwAssemblyForUcscDb(conn, "male.hg19");

char query[512];
sqlSafef(query, sizeof(query), 
    "select * from edwAnalysisRun where createStatus>0 "
    " and analysisStep like 'macs2_dnase_%%'");
struct edwAnalysisRun *run, *runList = edwAnalysisRunLoadByQuery(conn, query);
uglyf("Got %d analysisRun\n", slCount(runList));

for (run = runList; run != NULL; run = run->next)
    {
    struct edwValidFile *bamVf = edwValidFileFromFileId(conn, run->firstInputId);
    sqlSafef(query, sizeof(query), "select * from edwAssembly where id=%u", run->assemblyId);
    struct edwAssembly *runAssembly = edwAssemblyLoadByQuery(conn, query);

    if (sameString(bamVf->ucscDb, targetAssembly->name))
        {
	fprintf(fSql, "#run=%s\tbam=%s\n",  
	    runAssembly->name, bamVf->ucscDb);
	boolean runOk = TRUE;
	if (runAssembly->id == targetAssembly->id)
	    {
	    /* Nothing to do, run already correct */
	    }
	else if (sameString(runAssembly->name, "hg19") 
		|| sameString(runAssembly->name, "centro.hg19"))
	    {
	    /* Output ok, but need to update. */
	    fprintf(fSql, "update edwAnalysisRun set assemblyId=%u where id=%u;\n",
		targetAssembly->id, run->id);
	    }
	else
	    {
	    /* Run too far away, need to deprecate files actually */
	    runOk = FALSE;
	    }

	int i;
	for (i=0; i<run->createCount; ++i)
	    {
	    struct edwValidFile *vf = edwValidFileFromFileId(conn, run->createFileIds[i]);
	    if (runOk)
	        {
		if (!sameString(vf->ucscDb, bamVf->ucscDb))
		    {
		    struct edwFile *ef = edwFileFromId(conn, vf->fileId);
		    fprintf(fSql, "update edwValidFile set ucscDb='%s' where fileId=%u;\n",
			targetAssembly->name, vf->fileId);
		    fprintf(fSql, "update edwFile set tags='%s' where id=%u;\n",
			cgiStringNewValForVar(ef->tags, "ucsc_db", targetAssembly->name), ef->id);
		    edwFileFree(&ef);
		    }
		}
	    else
	        {
		char reason[256];
		safef(reason, sizeof(reason), "File not really derived from %s, from %s instead",
		    vf->ucscDb,  bamVf->ucscDb);
		deprecate(fSql, vf->fileId, reason);
		}
	    edwValidFileFree(&vf);
	    }
	}
    }
carefulClose(&fSql);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
edwFixTarget2(argv[1]);
return 0;
}
