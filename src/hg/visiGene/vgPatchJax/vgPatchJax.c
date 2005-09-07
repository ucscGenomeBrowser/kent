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

static char const rcsid[] = "$Id: vgPatchJax.c,v 1.2 2005/09/07 23:44:14 kent Exp $";

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

struct hash *getCaptions(char *fileName, struct sqlConnection *conn)
/* Read caption file, add it to database, and return 
 * a hash full of caption internal id's keyed by the caption external
 * ID's */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);
char *extId, *text;
int id;
struct dyString *query = dyStringNew(0);

struct slName *sqlQuickList(struct sqlConnection *conn, char *query);
while (lineFileNextReal(lf, &text))
    {
    char *escaped;
    extId = nextWord(&text);
    escaped = makeEscapedString(text, '"');
    dyStringClear(query);
    dyStringAppend(query, "insert into caption values(default, \"");
    dyStringAppend(query, escaped);
    dyStringAppend(query, "\")");
    sqlUpdate(conn, query->string);
    id = sqlLastAutoId(conn);
    hashAddInt(hash, extId, id);
    freez(&escaped);
    }
dyStringFree(&query);
return hash;
}

void vgPatchJax(char *database, char *dir)
/* vgPatchJax - Patch Jackson labs part of visiGene database. */
{
struct sqlConnection *vgConn = sqlConnect(database);
struct sqlConnection *jaxConn = sqlConnect("jackson");
struct dyString *query = dyStringNew(0);
struct dyString *caption = dyStringNew(0);
char **row;
struct slName *imageId, *imageIdList = sqlQuickList(vgConn,
	"select submitId from imageFile where fullLocation = 5");
struct sqlResult *sr;

uglyf("Updating %d jackson labs records\n", slCount(imageIdList));
for (imageId = imageIdList; imageId != NULL; imageId = imageId->next)
    {
    dyStringClear(caption);
    dyStringClear(query);
    dyStringPrintf(query,
	"select imageNote from IMG_ImageNote "
	"where _Image_key = %s "
	"order by sequenceNum"
	, imageId->name);
    sr = sqlGetResult(jaxConn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
       dyStringAppend(caption, row[0]);
    sqlFreeResult(&sr);

    if (caption->stringSize > 0)
	{
	char *escaped = makeEscapedString(caption->string, '"');
	int id;
	subChar(escaped, '\t', ' ');
	subChar(escaped, '\n', ' ');
	dyStringClear(query);
	dyStringAppend(query, "insert into caption values(default, \"");
	dyStringAppend(query, escaped);
	dyStringAppend(query, "\")");
	sqlUpdate(vgConn, query->string);
	id = sqlLastAutoId(vgConn);

	dyStringClear(query);
	dyStringPrintf(query, 
	   "update imageFile set caption=%d ", id);
	dyStringPrintf(query,
	   "where fullLocation = 5 and submitId='%s'", imageId->name);
	sqlUpdate(vgConn, query->string);
	uglyf("%s\n", query->string);
	}
    }

sqlDisconnect(&jaxConn);
sqlDisconnect(&vgConn);
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
