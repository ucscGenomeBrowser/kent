/* hgSoftberryHom - Make table storing Softberry protein homology information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "jksql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSoftberryHom - Make table storing Softberry protein homology information\n"
  "usage:\n"
  "   hgSoftberryHom database file(s).pro\n");
}

void makeTabLines(char *fileName, FILE *f)
/* Loop through file and write out tab-separated records to f. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *s, *sharpParts[8], *protParts[128];
int lineSize, sharpCount, protCount;
int i,j;
char *label, *gi;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] != '>')
        continue;
    line += 1;
    label = nextWord(&line);
    if (label == NULL)
	errAbort("Bad format A line %d of %s\n", lf->lineIx, lf->fileName);
    if (strchr(line, '#') == NULL)
        continue;
    sharpCount = chopString(line, "#", sharpParts, ArraySize(sharpParts));
    if (sharpCount != 3)
	errAbort("Bad format B line %d of %s\n", lf->lineIx, lf->fileName);
    s = trimSpaces(sharpParts[1]);
    protCount = chopString(s, "\001", protParts, ArraySize(protParts));
    if (protCount < 1)
	errAbort("Bad format C line %d of %s\n", lf->lineIx, lf->fileName);
    for (i=0; i<protCount; ++i)
        {
	s = protParts[i];
	gi = nextWord(&s);
	if (s == NULL) s == "";
	fprintf(f, "%s\t%s\t%s\n", label, gi, s);
	}
    }
lineFileClose(&lf);
}

void hgSoftberryHom(char *database, int fileCount, char *files[])
/* hgSoftberryHom - Make table storing Softberry protein homology information. */
{
int i;
char *fileName;
char *table = "softberryHom";
char *tabFileName = "softberryHom.tab";
FILE *f = mustOpen(tabFileName, "w");
struct sqlConnection *conn = NULL;
struct dyString *ds = newDyString(2048);

for (i=0; i<fileCount; ++i)
    {
    fileName = files[i];
    printf("Processing %s\n", fileName);
    makeTabLines(fileName, f);
    }
carefulClose(&f);

uglyAbort("All for now");

#ifdef SOON
/* Create table if it doesn't exist, delete whatever is
 * already in it, and fill it up from tab file. */
conn = sqlConnect(database);
printf("Loading %s table\n", table);
sqlMaybeMakeTable(conn, table, createTable);
sqlUpdate(conn, 
dyStringPrintf(ds, "DELETE from %s", table);
sqlUpdate(conn, ds->string);
dyStringClear(ds);
dyStringPrintf(ds, "LOAD data local infile '%s' into table %s", 
    tabFileName, table);
sqlUpdate(conn, ds->string);
sqlDisconnect(&conn);
#endif /* SOON */
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 3)
    usage();
hgSoftberryHom(argv[1], argc-2, argv+2);
return 0;
}
