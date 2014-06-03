/* edwFixRenamed - Deal with renamed ENCODE2 files by deprecating them and pointing to 
 * new versions. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* The input for this program was generated with:
 * mdbQuery "select fileName,objStatus from hg19 where objStatus like 'rename%'" -out=tab -table=metaDb_cricket > ~/renamedHg19.tab
 */

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
  "edwFixRenamed - Deal with renamed ENCODE2 files by deprecating them and pointing to\n"
  "new versions.\n"
  "usage:\n"
  "   edwFixRenamed database tabFile\n"
  "where tabFile is two columns: fileName and objStatus from the metaDb\n"
  "See source code for how this file was generated.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void edwFixRenamed(char *database, char *inTab)
/* edwFixRenamed - Deal with renamed ENCODE2 files by deprecating them and pointing to 
 * new versions. */
{
struct sqlConnection *conn = edwConnect();
struct lineFile *lf = lineFileOpen(inTab, TRUE);
char *row[2];
while (lineFileRowTab(lf, row))
    {
    /* Get fields in local variables. */
    char *oldFileName = row[0];
    char *objStatus = row[1];

    /* Get rid of bai name for bam,bai pairs. */
    char *comma = strchr(oldFileName, ',');
    if (comma != NULL)
        {
	if (!endsWith(comma, ".bai"))
	    errAbort("Unexpected conjoining of files line %d of %s", lf->lineIx, lf->fileName);
	*comma = 0;
	}

    /* Hunt for object it was renamed to in objStatus string. */
    char *dupe = cloneString(objStatus);
    char *s = dupe;
    char *newFileName;
    while ((newFileName = nextWord(&s)) != NULL)
        {
	if (startsWith("wgEncode", newFileName))
	    break;
	}

    /* Find record for old file and deprecate it. */
    char query[512];
    sqlSafef(query, sizeof(query), 
	"select * from edwFile where submitFileName like '%s/%%/%s'", database, oldFileName);
    struct edwFile *ef = edwFileLoadByQuery(conn, query);
    if (slCount(ef) != 1)
        errAbort("Expecting one result got %d for %s\n", slCount(ef), query);
    printf("# %s %s\n", oldFileName, objStatus);
    long long id = ef->id;
    char *reason = "Duplicate of renamed file in ENCODE2";
    printf("update edwFile set deprecated='%s' where id=%lld;\n", reason, id);

    /* Attempt to find record for new file and add it to replacedBy */
    if (newFileName != NULL)
	{
	sqlSafef(query, sizeof(query),
	    "select * from edwFile where submitFileName like '%s/%%/%s'", database, newFileName);
	struct edwFile *newEf = edwFileLoadByQuery(conn, query);
	if (newEf != NULL)
	    {
	    printf("update edwFile set replacedBy=%lld where id=%lld;\n", (long long)newEf->id, id);
	    }
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwFixRenamed(argv[1], argv[2]);
return 0;
}
