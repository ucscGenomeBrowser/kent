/* hgExtFile - Add external file to database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "hdb.h"
#include "hgRelate.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgExtFile - Add external file to database\n"
  "usage:\n"
  "   hgExtFile database file(s)\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgExtFile(int fileCount, char *fileNames[])
/* hgExtFile - Add external file to database. */
{
struct sqlConnection *conn = hgStartUpdate();
int i;
char *fileName;
struct dyString *dy = newDyString(1024);
char dir[256],root[128],ext[64];

for (i=0; i<fileCount; ++i)
    {
    fileName = fileNames[i];
    splitPath(fileName, dir, root, ext);
    if (dir[0] == 0)
        errAbort("Files must have full path name");
    dyStringClear(dy);
    dyStringPrintf(dy, "INSERT into extFile VALUES(%d,'%s','%s',%d)", 
       hgNextId, root, fileName, fileSize(fileName));
    uglyf("%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    }
hgEndUpdate(&conn, "Add %d files to extFile starting with %s", fileCount, fileNames[0]);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc < 3)
    usage();
hSetDb(argv[1]);
hgExtFile(argc-2, argv+2);
return 0;
}
