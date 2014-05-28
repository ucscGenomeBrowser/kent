/* edwFixTechRepsFromTarballs - Add technical replicate info to files that used to be in 
 * tarballs. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFixTechRepsFromTarballs - Add technical replicate info to files that used to be in tarballs.\n"
  "usage:\n"
  "   edwFixTechRepsFromTarballs corrected.tab\n"
  "Run edwCorrectTags with corrected.tab to finish.  Also need to deprecate all files\n"
  "with submitFileName ending in _reject.fastq.gz\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void edwFixTechRepsFromTarballs(char *output)
/* edwFixTechRepsFromTarballs - Add technical replicate info to files that used to be in 
 * tarballs. */
{
/* Get list of files that were unpacked from tarballs */
struct sqlConnection *conn = edwConnect();
char query[512];
sqlSafef(query, sizeof(query), 
    "select * from edwFile where submitFileName like '%%.dir/%%'"
    " order by submitFileName");
struct edwFile *ef, *efList = edwFileLoadByQuery(conn, query);
verbose(1, "Found %d files matching *.dir/* criteria\n", slCount(efList));

/* Open output and write header */
FILE *f = mustOpen(output, "w");
fprintf(f, "#accession\ttechnical_replicate\toutput_type\n");

struct edwFile *prevEf = NULL;
char dir[PATH_LEN], prevDir[PATH_LEN] = "";
int techRep = 0;
for (ef = efList; ef != NULL; ef = ef->next)
    {
    struct edwValidFile *vf = edwValidFileFromFileId(conn, ef->id);

    /* Figure out if onto a new tarball/directory */
    splitPath(ef->submitFileName, dir, NULL, NULL);
    if (!sameString(dir, prevDir))
        techRep = 0;

    /* Deal with rejects just by merging them with previous tech rep.  Do a lot of checking
     * to make sure this really works. */
    char *outputType = vf->outputType;
    if (endsWith(ef->submitFileName, "_reject.fastq.gz"))
        {
	assert(prevEf != NULL);
	if (!endsWith(prevEf->submitFileName, "_pf.fastq.gz"))
	    errAbort("Not understanding order completely.");
	int prefixSize = strlen(ef->submitFileName) - strlen("_reject.fastq.gz");
	if (memcmp(ef->submitFileName, prevEf->submitFileName, prefixSize) != 0)
	    errAbort("Really not understanding order.");
	outputType = "rejected_reads";
	}
    else
        ++techRep;

    /* Write out license plate and replicate. */
    fprintf(f, "%s\t%d\t%s\n", vf->licensePlate, techRep, outputType);
    edwValidFileFree(&vf);

    /* Save current variables for next time round loop. */
    prevEf = ef;
    strcpy(prevDir, dir);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
edwFixTechRepsFromTarballs(argv[1]);
return 0;
}
