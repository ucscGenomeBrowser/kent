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

static char const rcsid[] = "$Id: vgPatchJax.c,v 1.4 2005/10/07 20:07:15 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "vgPatchJax - Patch Jackson labs part of visiGene database\n"
  "usage:\n"
  "   vgPatchJax database\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void vgPatchJax(char *database)
/* vgPatchJax - Patch Jackson labs part of visiGene database. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
struct hash *parts = newHash(0);
struct hash *forwarder = newHash(0);
struct slName *sharpList = NULL, *sharp;
struct dyString *dy = dyStringNew(0);

/* Scan once through bodyPart creating a hash of all names. */
sr = sqlGetResult(conn, "select name,id from bodyPart");
while ((row  = sqlNextRow(sr)) != NULL)
    hashAdd(parts, row[0], cloneString(row[1]));
sqlFreeResult(&sr);

/* Scan again for just cases that end in #, and decide whether need
 * to just remove #, or need to also update other bodyPart references
 * to another bodyPart that already exists without the # */
sr = sqlGetResult(conn, "select name,id from bodyPart where name like '%#'");
while ((row  = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    int len = strlen(name);
    char *unsharped = cloneStringZ(name, len-1);
    char *forwardVal;
    slNameAddHead(&sharpList, name);
    forwardVal = hashFindVal(parts, unsharped);
    if (forwardVal != NULL)
        hashAdd(forwarder, name, forwardVal);
    else
        freez(&forwardVal);
    }
sqlFreeResult(&sr);

/* At this point we have a list of all bodyParts that have sharps, and
 * a hash full of the ones that need forwarding in the expression table. */
for (sharp = sharpList; sharp != NULL; sharp = sharp->next)
    {
    char *forwardVal = hashFindVal(forwarder, sharp->name);
    dyStringClear(dy);
    if (forwardVal != NULL)
        {
	char *oldVal = hashFindVal(parts, sharp->name);
	dyStringPrintf(dy, "update expressionLevel set bodyPart = %s where bodyPart = %s",
		forwardVal, oldVal);
	sqlUpdate(conn, dy->string);
	dyStringClear(dy);
	dyStringPrintf(dy, "delete from bodyPart where id=%s and name=\"%s\"",
		oldVal, sharp->name);
	sqlUpdate(conn, dy->string);
	}
    else
        {
	int len = strlen(sharp->name);
	char *unsharped = cloneStringZ(sharp->name, len-1);
	dyStringPrintf(dy, "update bodyPart set name = \"%s\" where name = \"%s\"",
		unsharped, sharp->name);
	sqlUpdate(conn, dy->string);
	}
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
vgPatchJax(argv[1]);
return 0;
}
