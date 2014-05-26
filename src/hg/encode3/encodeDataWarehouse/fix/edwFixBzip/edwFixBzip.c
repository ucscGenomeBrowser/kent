/* edwFixBzip - Help change bzipped file to gzipped files.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "md5.h"
#include "portable.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFixBzip - Help change bzipped file to gzipped files.\n"
  "usage:\n"
  "   edwFixBzip now\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void mustRemove(char *file)
/* Delete file or die trying. */
{
int err = remove(file);
if (err != 0)
    errnoAbort("Couldn't remove %s", file);
}

void edwFixBzip(char *XXX)
/* edwFixBzip - Help change bzipped file to gzipped files.. */
{
struct sqlConnection *conn = edwConnectReadWrite();
struct edwFile *ef, *efList = edwFileLoadByQuery(conn, "select * from edwFile where edwFileName like '%.bz2'");
for (ef = efList; ef != NULL; ef = ef->next)
    {
    char *bzipPath = edwPathForFileId(conn, ef->id);
    char dir[PATH_LEN], root[FILENAME_LEN], ext[FILEEXT_LEN];
    splitPath(bzipPath, dir, root, ext);
    char gzipPath[PATH_LEN];
    safef(gzipPath, PATH_LEN, "%s%s%s", dir, root, ".gz");
    verbose(1, "Changing %s to %s\n", bzipPath, gzipPath);
    char *md5 = md5HexForFile(gzipPath);
    long long size = fileSize(gzipPath);
    verbose(1, "md5 is %s, size = %lld\n", md5, size);
    splitPath(ef->edwFileName, dir, root, ext);
    safef(gzipPath, PATH_LEN, "%s%s%s", dir, root, ".gz");
    char query[512];
    safef(query, sizeof(query), 
	"update edwFile set edwFileName='%s', md5='%s', size=%lld where id=%u",
	gzipPath, md5, size, ef->id);
    sqlUpdate(conn, query);
    mustRemove(bzipPath);
    verbose(1, "removed %s\n", bzipPath);
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
edwFixBzip(argv[1]);
return 0;
}
