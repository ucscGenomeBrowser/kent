/* vgPatchJax - Patch Jackson labs part of visiGene database. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "jksql.h"
#include "portable.h"
#include "obscure.h"
#include "ra.h"

static char const rcsid[] = "$Id: vgPatchJax.c,v 1.3 2005/09/08 05:23:07 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "vgPatchJax - Patch Jackson labs part of visiGene database\n"
  "usage:\n"
  "   vgPatchJax database dir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int getSourceId(struct sqlConnection *conn,
	struct hash *hash, struct dyString *dy, 
	char *setUrl, char *itemUrl)
/* If setUrl is not in hash, then make up a new submission set.
 * Return submission set ID. */
{
int id;
static int uniq = 0;
if (hashLookup(hash, setUrl))
    id = hashIntVal(hash, setUrl);
else
    {
    dyStringClear(dy);
    dyStringPrintf(dy,
	"insert into submissionSource values(default, "
	"'Uniq%d', '', '%s', '%s')"
	, ++uniq, setUrl, itemUrl);
    uglyf("%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    id = sqlLastAutoId(conn);
    hashAddInt(hash, setUrl, id);
    }
return id;
}


void vgPatchJax(char *database, char *dir)
/* vgPatchJax - Patch Jackson labs part of visiGene database. */
{
struct sqlConnection *conn1 = sqlConnect(database);
struct sqlConnection *conn2 = sqlConnect(database);
struct sqlResult *sr;
char **row;
struct dyString *query = dyStringNew(0);
struct hash *urlHash = hashNew(0);
struct hash *nameHash = hashNew(0);
struct hashEl *list, *el;

sr = sqlGetResult(conn1, "select name,setUrl,itemUrl from submissionSet");
while ((row = sqlNextRow(sr)) != NULL)
    {
    int sourceId = getSourceId(conn2, urlHash, query, row[1], row[2]);
    hashAddInt(nameHash, row[0], sourceId);
    }
sqlFreeResult(&sr);

list = hashElListHash(nameHash);
for (el = list; el != NULL; el = el->next)
    {
    dyStringClear(query);
    dyStringPrintf(query,
    	"update submissionSet set submissionSource=%d "
	"where name = '%s'"
	, ptToInt(el->val), el->name);
    uglyf("%s\n", query->string);
    sqlUpdate(conn2, query->string);
    }
sqlFreeResult(&sr);

sqlDisconnect(&conn2);
sqlDisconnect(&conn1);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
vgPatchJax(argv[1], argv[2]);
return 0;
}
