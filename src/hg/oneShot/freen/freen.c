/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "fa.h"

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen file");
}

void freen(char *database, char *outName)
/* Print borf as fasta. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
FILE *f = mustOpen(outName, "w");

/* Get list of spliced ones. */
sr = sqlGetResult(conn, 
	" select name,seq from refMrna");
while ((row = sqlNextRow(sr)) != NULL)
    {
    faWriteNext(f,row[0], row[1], strlen(row[1]));
    }

}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
   usage();
freen(argv[1], argv[2]);
}
