/* hgNibSeq - convert DNA to nibble-a-base and store location in database. */
#include "common.h"
#include "portable.h"
#include "fa.h"
#include "nib.h"
#include "jksql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgNibSeq - convert DNA to nibble-a-base and store location in database\n"
  "usage:\n"
  "   hgNibSeq database destDir file(s).fa\n"
  "This will create .nib versions of all the input .fa files in destDir\n"
  "and store pointers to them in the database.\n");
}

void hgNibSeq(char *database, char *destDir, int faCount, char *faNames[])
/* hgNibSeq - convert DNA to nibble-a-base and store location in database. */
{
char dir[256], name[128], ext[64];
char nibName[512];
struct sqlConnection *conn = sqlConnect(database);
char query[512];
int i, seqCount;
char *faName;
struct dnaSeq *seq;
unsigned long total = 0;

makeDir(destDir);
for (i=0; i<faCount; ++i)
    {
    faName = faNames[i];
    splitPath(faName, dir, name, ext);
    sprintf(nibName, "%s/%s.nib", destDir, name);
    printf("Processing %s to %s\n", faName, nibName);
    seq = faReadDna(faName);
    uglyf("Read DNA\n");
    nibWrite(seq, nibName);
    uglyf("Wrote nib\n");
    sprintf(query, "INSERT into chromInfo VALUES('%s', %d, '%s')",
        name, seq->size, nibName);
    sqlUpdate(conn,query);
    total += seq->size;
    freeDnaSeq(&seq);
    }
sqlDisconnect(&conn);
printf("%lu total bases\n", total);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 4)
    usage();
hgNibSeq(argv[1], argv[2], argc-3, argv+3);
return 0;
}
