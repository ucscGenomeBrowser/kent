/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "jksql.h"

static char const rcsid[] = "$Id: freen.c,v 1.64 2006/02/02 16:24:48 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file \n");
}

int level = 0;


void freen(char *fileName)
/* Test some hair-brained thing. */
{
struct slName *id, *idList = readAllLines(fileName);
struct sqlConnection *conn = sqlConnect("visiGene");
int i;
char query[256];
char *s;
double randSeed;
struct sqlResult *sr;
char **row;
struct dyString *dy = dyStringNew(10*1024);
int batchSize = 80000;
struct hash *hash = hashNew(18);

for (id = idList; id != NULL; id = id->next)
    {
    hashAdd(hash, id->name, NULL);
    }

safef(query, sizeof(query), "select rand()");
randSeed = sqlQuickDouble(conn, query);
srand(randSeed*12345);

shuffleList(idList, 1);
id = idList;
uglyTime(NULL);

#ifdef SOON
for (i=0; i<batchSize; ++i)
     {
     safef(query, sizeof(query), "select preparation from image where id=%s",
     	id->name);
     s = sqlQuickString(conn, query);
     id = id->next;
     }
uglyTime("%d like %s", batchSize, query);

for (i=0; i<batchSize; ++i)
     {
     safef(query, sizeof(query), 
     	"select submissionSet.privateUser from image,submissionSet "
	"where image.id=%s and image.submissionSet=submissionSet.id"
	, id->name);
     s = sqlQuickString(conn, query);
     id = id->next;
     }
uglyTime("%d like %s", batchSize, query);

for (i=0; i<batchSize; ++i)
     {
     safef(query, sizeof(query), 
     	"select genotype.alleles from image,specimen,genotype "
	"where image.id=%s and image.specimen=specimen.id "
	"and specimen.genotype=genotype.id"
	, id->name);
     s = sqlQuickString(conn, query);
     id = id->next;
     }
uglyTime("%d like %s", batchSize, query);

dyStringClear(dy);
dyStringAppend(dy,
    "select id,preparation from image where id in (");
for (i=0; i<batchSize; ++i)
    {
    if (i != 0)
        dyStringAppendC(dy, ',');
    dyStringAppend(dy, id->name);
    id = id->next;
    }
dyStringAppend(dy, ")");
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashLookup(hash, row[0]);
    }
uglyTime("%d simple in", batchSize);

dyStringClear(dy);
dyStringAppend(dy,
    "select image.id,submissionSet.privateUser from image,submissionSet "
    "where image.submissionSet=submissionSet.id "
    "and image.id in (");
for (i=0; i<batchSize; ++i)
    {
    if (i != 0)
        dyStringAppendC(dy, ',');
    dyStringAppend(dy, id->name);
    id = id->next;
    }
dyStringAppend(dy, ")");
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashLookup(hash, row[0]);
    }
uglyTime("%d single join in", batchSize);
#endif /* SOON */

dyStringClear(dy);
dyStringAppend(dy,
    "select image.id,genotype.alleles from image,specimen,genotype "
    "where genotype.id=specimen.genotype "
    "and specimen.id = image.specimen "
    "and image.id in (");
for (i=0; i<batchSize; ++i)
    {
    if (i != 0)
        dyStringAppendC(dy, ',');
    dyStringAppend(dy, id->name);
    id = id->next;
    }
dyStringAppend(dy, ")");
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashLookup(hash, row[0]);
    }
uglyTime("%d double join in", batchSize);

}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
