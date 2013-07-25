/* edwCorrectFileTags - Use this to correct tags in the edwFile table and corresponding fields in the edwValidFile table without forcing a validateManifest rerun or a reupload.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fieldedTable.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwCorrectFileTags - Use this to correct tags in the edwFile table and corresponding fields in\n"
  "the edwValidFile table without forcing a validateManifest rerun or a reupload.\n"
  "usage:\n"
  "   edwCorrectFileTags corrected.tab\n"
  "where corrected.tab is a tab separated file with a header line starting with # that labels\n"
  "the columns.  The column 'accession' must be included, and should contain the ENCFF identifier\n"
  "of the file.  The other columns will substitute for the corresponding manifest file columns\n"
  "in the original upload.  Optionally you can also add new columns.  Do *not* include md5_sum,\n"
  "size, valid_key, or file_name columns.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void checkForbiddenFields(struct fieldedTable *table, char **forbiddenFields, int forbiddenCount)
/* Make sure table doesn't include forbidden fields. */
{
int i;
for (i=0; i<forbiddenCount; ++i)
    {
    char *forbidden = forbiddenFields[i];
    if (stringArrayIx(forbidden, table->fields, table->fieldCount) >= 0)
        errAbort("Forbidden field %s in %s.", forbidden, table->name);
    }
}

long long edwFileIdForLicensePlate(struct sqlConnection *conn, char *licensePlate)
/* Return id in edwFile table that corresponds to licensePlate, or 0 if it doesn't exist */
{
char query[128];
sqlSafef(query, sizeof(query), "select fileId from edwValidFile where licensePlate='%s'", 
    licensePlate);
return sqlQuickLongLong(conn, query);
}

long long edwNeedFileIdForLicensePlate(struct sqlConnection *conn, char *licensePlate)
/* Return id in edwFile table that corresponds to licensePlate, or die with error message
 * if it doesn't exist. */
{
long long result = edwFileIdForLicensePlate(conn, licensePlate);
if (result == 0)
    errAbort("%s doesn't exist in warehouse.", licensePlate);
return result;
}

void edwCorrectFileTags(char *tabFileName)
/* edwCorrectFileTags - Use this to correct tags in the edwFile table and corresponding fields 
 * in the edwValidFile table without forcing a validateManifest rerun or a reupload.. */
{
struct sqlConnection *conn = edwConnectReadWrite();
char *requiredFields[] = {"accession",};
char *forbiddenFields[] = {"md5_sum", "size", "valid_key", "file_name"};
struct fieldedTable *table = fieldedTableFromTabFile(tabFileName, tabFileName,
	requiredFields, ArraySize(requiredFields));
checkForbiddenFields(table, forbiddenFields, ArraySize(forbiddenFields));
int accessionIx = stringArrayIx("accession", table->fields, table->fieldCount);

struct fieldedRow *fr;
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    char *acc = fr->row[accessionIx];
    long long id = edwNeedFileIdForLicensePlate(conn, acc);
    struct edwFile *ef = edwFileFromId(conn, id);
    int i;
    char *tags = ef->tags;
    for (i=0; i<table->fieldCount; ++i)
        {
	if (i != accessionIx)
	    tags = cgiStringNewValForVar(tags, table->fields[i], fr->row[i]);
	}
    edwFileResetTags(conn, ef, tags);
    edwFileFree(&ef);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
edwCorrectFileTags(argv[1]);
return 0;
}
