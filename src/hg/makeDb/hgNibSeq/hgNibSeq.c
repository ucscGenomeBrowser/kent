/* hgNibSeq - convert DNA to nibble-a-base and store location in database. */
#include "common.h"
#include "portable.h"
#include "fa.h"
#include "nib.h"
#include "jksql.h"
#include "cheapcgi.h"

boolean preMadeNib = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgNibSeq - convert DNA to nibble-a-base and store location in database\n"
  "usage:\n"
  "   hgNibSeq database destDir file(s).fa\n"
  "This will create .nib versions of all the input .fa files in destDir\n"
  "and store pointers to them in the database.\n"
  "options:\n"
  "   -preMadeNib  don't bother generating nib files, they exist already\n"
  );
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
struct dnaSeq *seq = NULL;
unsigned long total = 0;
int size;

makeDir(destDir);
for (i=0; i<faCount; ++i)
    {
    faName = faNames[i];
    splitPath(faName, dir, name, ext);
    sprintf(nibName, "%s/%s.nib", destDir, name);
    printf("Processing %s to %s\n", faName, nibName);
    if (preMadeNib)
        {
	FILE *nibFile;
	nibOpenVerify(nibName, &nibFile, &size);
	carefulClose(&nibFile);
	}
    else
	{
	seq = faReadDna(faName);
	if (seq != NULL)
	    {
	    size = seq->size;
	    uglyf("Read DNA\n");
	    nibWrite(seq, nibName);
	    uglyf("Wrote nib\n");
	    freeDnaSeq(&seq);
	    }
	}
    sprintf(query, "INSERT into chromInfo VALUES('%s', %d, '%s')",
        name, size, nibName);
    sqlUpdate(conn,query);
    total += size;
    }
sqlDisconnect(&conn);
printf("%lu total bases\n", total);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc < 4)
    usage();
preMadeNib = cgiBoolean("preMadeNib");
hgNibSeq(argv[1], argv[2], argc-3, argv+3);
return 0;
}
