/* vgChangeContributors - Change the contributor list in visiGene database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "vgChangeContributors - Change the contributor list in visiGene database\n"
  "usage:\n"
  "   vgChangeContributors database submissionSetId newContributors\n"
  "example:\n"
  "   vgChangeContributors visiGene 704 'Ueno N., Kitayama A., Terasaka C., Nomoto K., Shibamoto K., Hiroyo N.'\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int findOrAddIdTable(struct sqlConnection *conn, char *table, char *field, 
	char *value)
/* Get ID associated with field.value in table.  */
{
char query[256];
int id;
char *escValue = makeEscapedString(value, '"');
sqlSafef(query, sizeof(query), "select id from %s where %s = \"%s\"",
	table, field, escValue);
verbose(2, "%s\n", query);
id = sqlQuickNum(conn, query);
if (id == 0)
    {
    sqlSafef(query, sizeof(query), "insert into %s values(default, \"%s\")",
    	table, escValue);
    verbose(2, "%s\n", query);
    sqlUpdate(conn, query);
    id = sqlLastAutoId(conn);
    }
freeMem(escValue);
return id;
}

void vgChangeContributors(char *database, char *submissionSet, char *contributors)
/* vgChangeContributors - Change the contributor list in visiGene database. */
{
int submissionSetId = atoi(submissionSet);
struct sqlConnection *conn = sqlConnect(database);
struct slName *contrib, *contribList = slNameListFromComma(contributors);
struct dyString *dy = dyStringNew(0);

sqlDyStringAppend(dy, "update submissionSet set contributors='");
dyStringAppend(dy, contributors);
dyStringPrintf(dy, "' where id=%d", submissionSetId);
verbose(2, dy->string);
sqlUpdate(conn, dy->string);

dyStringClear(dy);
sqlDyStringPrintf(dy, "delete from submissionContributor where submissionSet=%d", 
    submissionSetId);
verbose(2, dy->string);
sqlUpdate(conn, dy->string);

for (contrib = contribList; contrib != NULL; contrib = contrib->next)
    {
    int contribId = findOrAddIdTable(conn, "contributor", "name", 
    	skipLeadingSpaces(contrib->name));
    dyStringClear(dy);
    sqlDyStringPrintf(dy, 
          "insert into submissionContributor values(%d, %d)",
	  submissionSetId, contribId);
    verbose(2, "%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    }
slFreeList(&contribList);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
vgChangeContributors(argv[1], argv[2], argv[3]);
return 0;
}
