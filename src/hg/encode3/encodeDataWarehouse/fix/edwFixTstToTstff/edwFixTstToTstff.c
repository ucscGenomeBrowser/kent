/* edwFixTstToTstff - Change TST to TSTFF license plates on beta.. */

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
  "edwFixTstToTstff - Change TST to TSTFF license plates on beta.\n"
  "usage:\n"
  "   edwFixTstToTstff out.sql out.csh\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void oldToNewPlate(char *oldPlate, char *newPlate)
/* Insert two FF's in the crucial place. */
{
memcpy(newPlate, oldPlate, 3);
newPlate[3] = 'F';
newPlate[4] = 'F';
strcpy(newPlate+5, oldPlate+3);
}

void edwFixTstToTstff(char *outSql, char *outCsh)
/* edwFixTstToTstff - Change TST to TSTFF license plates on beta.. */
{
FILE *fSql = mustOpen(outSql, "w");
FILE *fSh = mustOpen(outCsh, "w");

/* Rename all license plate fields in edwValidFile */
char newPlate[64];
struct sqlConnection *conn = edwConnectReadWrite();
char query[512];
sqlSafef(query, sizeof(query),
    "select licensePlate from edwValidFile where licensePlate like 'TST%%'");
struct slName *tstPlate, *tstPlateList = sqlQuickList(conn, query);
for (tstPlate = tstPlateList; tstPlate != NULL; tstPlate = tstPlate->next)
    {
    oldToNewPlate(tstPlate->name, newPlate);
    fprintf(fSql, "update edwValidFile set licensePlate='%s' where licensePlate='%s';\n",
	    newPlate, tstPlate->name);
    }

/* Rename all files in edwFile */
sqlSafef(query, sizeof(query),
    "select * from edwFile where edwFileName like '%%/TST%%'");
struct edwFile *ef, *efList = edwFileLoadByQuery(conn, query);
for (ef = efList; ef != NULL; ef = ef->next)
    {
    char dir[PATH_LEN], name[FILENAME_LEN], ext[FILEEXT_LEN];
    splitPath(ef->edwFileName, dir, name, ext);
    oldToNewPlate(name, newPlate);
    char newFileName[PATH_LEN];
    safef(newFileName, sizeof(newFileName), "%s%s%s", dir, newPlate, ext);
    fprintf(fSql, "update edwFile set edwFileName='%s' where id=%u;\n", newFileName, ef->id);
    fprintf(fSh, "mv %s%s %s%s\n", edwRootDir, ef->edwFileName, edwRootDir, newFileName);
    }

carefulClose(&fSql);
carefulClose(&fSh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwFixTstToTstff(argv[1], argv[2]);
return 0;
}
