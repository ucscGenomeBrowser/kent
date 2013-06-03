/* dupedInTable - Find things duped in a table. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "dupedInTable - Find things duped in a table\n"
  "usage:\n"
  "   dupedInTable database table field\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct nameCount
/* Name and count. */
    {
    struct nameCount *next;	/* Next in list. */
    char *name;			/* Name - allocated in hash. */
    int count;			/* Number of times seen. */
    };

void dupedInTable(char *database, char *table, char *field)
/* dupedInTable - Find things duped in a table. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
char query[256];
struct hash *hash = newHash(18);
struct nameCount *ncList = NULL, *nc;

sqlSafef(query, sizeof(query), "select %s from %s", field, table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    if ((nc = hashFindVal(hash, name)) == NULL)
        {
	AllocVar(nc);
	hashAddSaveName(hash, name, nc, &nc->name);
	slAddHead(&ncList, nc);
	}
    nc->count += 1;
    }
slReverse(&ncList);
for (nc = ncList;  nc != NULL; nc = nc->next)
    if (nc->count > 1)
	printf("%s %d\n", nc->name, nc->count);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
dupedInTable(argv[1], argv[2], argv[3]);
return 0;
}
