/* cdwFixInvisibleIndexFiles - Bring in invisible index files.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwFixInvisibleIndexFiles - Bring in invisible index files.\n"
  "usage:\n"
  "   cdwFixInvisibleIndexFiles bam .bai\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void cdwFixInvisibleIndexFiles(char *type, char *indexSuffix)
/* cdwFixInvisibleIndexFiles - Bring in invisible index files. */
{
struct sqlConnection *conn = cdwConnect();
char query[512];
sqlSafef(query, sizeof(query), "select * from cdwFile where submitFileName like '%%.%s'", type);
struct cdwFile *file, *fileList = cdwFileLoadByQuery(conn, query);
uglyf("Got %d %s files\n", slCount(fileList), type);
for (file = fileList; file != NULL; file = file->next)
     {
     char submitIndexName[PATH_LEN];
     safef(submitIndexName, sizeof(submitIndexName), "%s%s", file->submitFileName, indexSuffix);
     sqlSafef(query, sizeof(query), "select * from cdwFile where submitFileName like '%s'", 
	submitIndexName);
     struct cdwFile *indexFile = cdwFileLoadByQuery(conn, query);
     if (indexFile == NULL)
         verbose(1, "Can't find index for %s %s\n", file->cdwFileName, file->submitFileName);
     else
         {
	 if (indexFile->next != NULL)
	     errAbort("Multiple indexes match %s", file->submitFileName);
	 char fileCdwPath[PATH_LEN];
	 safef(fileCdwPath, sizeof(fileCdwPath), "%s%s", cdwRootDir, file->cdwFileName);
	 char symLinkNew[PATH_LEN];
	 safef(symLinkNew, sizeof(symLinkNew), "%s%s", fileCdwPath, indexSuffix);
	 if (fileExists(symLinkNew))
	     {
	     verbose(1, "Skipping existing %s\n", symLinkNew);
	     }
	 else
	     {
	     char relIndexPath[PATH_LEN];
	     safef(relIndexPath, sizeof(relIndexPath), "../../../%s", indexFile->cdwFileName);
	     verbose(1, "ln -s %s %s\n", relIndexPath, symLinkNew);
	     makeSymLink(relIndexPath, symLinkNew);
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
cdwFixInvisibleIndexFiles(argv[1], argv[2]);
return 0;
}
