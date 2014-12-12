/* cdwUndeprecate - Undeprecate a list of file accessions.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cdw.h"
#include "cdwLib.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwUndeprecate - Undeprecate a list of file accessions.\n"
  "usage:\n"
  "   cdwUndeprecate accessions.txt\n"
  "where accessions.txt is a list of ENCFF accessions separated by spaces and or new lines.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void cdwUndeprecate(char *fileName)
/* cdwUndeprecate - Undeprecate a list of file accessions.. */
{
char **words, *buf;
int wordCount;
readAllWords(fileName, &words, &wordCount, &buf);
verbose(1, "Read %d accessions from %s\n", wordCount, fileName);

struct sqlConnection *conn = cdwConnectReadWrite();
int i;
for (i=0; i<wordCount; ++i)
    {
    char query[512];
    sqlSafef(query, sizeof(query), "select * from cdwValidFile where licensePlate='%s'", words[i]);
    struct cdwValidFile *vf = cdwValidFileLoadByQuery(conn, query);
    if (vf == NULL)
       errAbort("%s doesn't exist in cdwValidFile table", words[i]);
    sqlSafef(query, sizeof(query), "update cdwFile set deprecated='' where id=%u", vf->fileId);
    sqlUpdate(conn, query);
    }
verbose(1, "Undeprecated %d files\n", i);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cdwUndeprecate(argv[1]);
return 0;
}
