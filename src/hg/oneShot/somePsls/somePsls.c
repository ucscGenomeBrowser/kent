/* somePsls - Get some psls from database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"
#include "jksql.h"
#include "hdb.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "somePsls - Get some psls from database\n"
  "usage:\n"
  "   somePsls database table in.lst out.psl\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void somePsls(char *database, char *table, char *inList, char *outPsl)
/* somePsls - Get some psls from database. */
{
char *words[1], **row;
FILE *f = mustOpen(outPsl, "w");
struct lineFile *lf = lineFileOpen(inList, TRUE);
int count = 0, found = 0;
char query[256];
struct psl *psl;
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
while (lineFileRow(lf, words))
    {
    sqlSafef(query, sizeof query, "select * from %s where qName = '%s'", table, words[0]);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	psl = pslLoad(row+1);
	pslTabOut(psl, f);
	pslFree(&psl);
	}
    sqlFreeResult(&sr);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
somePsls(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
