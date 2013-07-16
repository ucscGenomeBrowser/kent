/* randomEst - Select random ESTs from database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "fa.h"
#include "hdb.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "randomEst - Select random ESTs from database\n"
  "usage:\n"
  "   randomEst database count output.fa\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void randomEst(char *database, int count, char *output)
/* randomEst - Select random ESTs from database. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
int i, elIx, okCount = 0;
struct slName *list = NULL, *el;
FILE *f = NULL;
char **array = NULL;
struct dnaSeq *seq;
struct hash *uniqHash = newHash(0);

hSetDb(database);
printf("Scanning database\n");
sr = sqlGetResult(conn, "NOSQLINJ select acc,type,direction from mrna");
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (sameString(row[1], "EST") && sameString(row[2], "3"))
        {
	el = newSlName(row[0]);
	slAddHead(&list, el);
	++okCount;
	}
    }
sqlFreeResult(&sr);
printf("Got %d 3' ESTs\n", okCount);
AllocArray(array, okCount);
for (i=0, el = list; el != NULL; el = el->next, ++i)
    array[i] = el->name;

printf("Selecting %d to put into %s\n", count, output);
f = mustOpen(output, "w");
for (i=0; i<count; ++i)
    {
    char *name;
    elIx = rand()%okCount;
    name = array[elIx];
    if (!hashLookup(uniqHash, name))
	{
	hashAdd(uniqHash, name, NULL);
	seq = hRnaSeq(name);
	faWriteNext(f, seq->name, seq->dna, seq->size);
	freeDnaSeq(&seq);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4 || !isdigit(argv[2][0]))
    usage();
randomEst(argv[1], atoi(argv[2]), argv[3]);
return 0;
}
