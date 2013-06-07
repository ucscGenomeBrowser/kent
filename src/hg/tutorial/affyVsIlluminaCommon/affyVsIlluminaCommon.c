/* affyVsIlluminaCommon - Compare SNPs shared in Affy 250k and Illumina 300k platforms.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "affyVsIlluminaCommon - Compare SNPs shared in Affy 250k and Illumina 300k platforms.\n"
  "usage:\n"
  "   affyVsIlluminaCommon database\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void affyVsIlluminaCommon(char *database)
/* affyVsIlluminaCommon - Compare SNPs shared in Affy 250k and Illumina 300k platforms.. */
{
struct sqlConnection *conn = sqlConnect(database);

/* Make up a hash full of all rsIDs from Affy 250k table. */
struct hash *affyHash = hashNew(18);
struct sqlResult *sr = sqlGetResult(conn, "NOSQLINJ select rsId from snpArrayAffy250Nsp");
char **row;
int affyCount = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(affyHash, row[0], NULL);
    ++affyCount;
    }
sqlFreeResult(&sr);

/* Loop through Illumina counting up what's shared. */
sr = sqlGetResult(conn, "NOSQLINJ select name from snpArrayIllumina300");
int illuminaCount = 0;
int bothCount = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (hashLookup(affyHash, row[0]) != NULL)
        ++bothCount;
    ++illuminaCount;
    }
sqlFreeResult(&sr);


printf("Illumina 300k %d,  Affy 250k %d, both %d\n", illuminaCount, affyCount, bothCount);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
affyVsIlluminaCommon(argv[1]);
return 0;
}
