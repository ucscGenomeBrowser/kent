/* joinTwoInfo - Look at two columns in two tables in mySQL and see how joinable they look.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "jksql.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "joinTwoInfo - Look at two columns in two tables in mySQL and see how joinable they look.\n"
  "usage:\n"
  "   joinTwoInfo db1.table1.column1 db2.table2.column2\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct slName *getColumn(char *db, char *table, char *column, struct lm *lm)
/* Get list of all items in column allocated in lm. */
{
struct slName *list = NULL, *el;
struct sqlConnection *conn = sqlConnect(db);
char query[512];
safef(query, sizeof(query), "select %s from %s", column, table);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = lmSlName(lm, row[0]);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
struct slName *lmSlName(struct lm *lm, char *name);
sqlDisconnect(&conn);
slReverse(&list);
return list;
}

struct hash *uniqHash(struct slName *list)
/* Return hash of unique items. */
{
struct hash *hash = hashNew(0);
struct slName *el;
for (el = list; el != NULL; el = el->next)
    hashStore(hash, el->name);
return hash;
}

int countInHash(struct slName *list, struct hash *hash)
/* Count number of items in list that are also in hash */
{
int count = 0;
struct slName *el;
for (el = list; el != NULL; el = el->next)
    {
    if (hashLookup(hash, el->name))
        ++count;
    }
return count;
}

int countUniqInHash(struct slName *list, struct hash *hash)
/* Count number of items in list that are also in hash */
{
struct hash *uniqHash = hashNew(0);
struct slName *el;
for (el = list; el != NULL; el = el->next)
    {
    if (hashLookup(hash, el->name))
	{
	hashStore(uniqHash, el->name);
	}
    }
int count = uniqHash->elCount;
hashFree(&uniqHash);
return count;
}


void joinTwoInfo(char *spec1, char *spec2)
/* joinTwoInfo - Look at two columns in two tables in mySQL and see how joinable they look.. */
{
char *s1[4], *s2[4];
struct lm *lm = lmInit(0);
int partCount = chopByChar(lmCloneString(lm, spec1), '.', s1, ArraySize(s1));
if (partCount != 3)
    usage();
partCount = chopByChar(lmCloneString(lm, spec2), '.', s2, ArraySize(s2));
if (partCount != 3)
    usage();

struct slName *list1 = getColumn(s1[0], s1[1], s1[2], lm);
struct hash *uniq1 = uniqHash(list1);
struct slName *list2 = getColumn(s2[0], s2[1], s2[2], lm);
struct hash *uniq2 = uniqHash(list2);
int countOneInTwo = countInHash(list1, uniq2);
int countTwoInOne = countInHash(list2, uniq1);
int countUniqOneInTwo = countUniqInHash(list1, uniq2);
int countUniqTwoInOne = countUniqInHash(list2, uniq1);
printf("%s: %d items, %d unique items, %d items (%d unique) in %s\n",
	spec1, slCount(list1), uniq1->elCount, countOneInTwo, countUniqOneInTwo, spec2);
printf("%s: %d items, %d unique items, %d items (%d unique) in %s\n",
	spec2, slCount(list2), uniq2->elCount, countTwoInOne, countUniqTwoInOne, spec1);

lmCleanup(&lm);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
joinTwoInfo(argv[1], argv[2]);
return 0;
}
