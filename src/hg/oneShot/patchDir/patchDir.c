/* patchDir - Patch directions of some ESTs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "ra.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "patchDir - Patch directions of some ESTs\n"
  "usage:\n"
  "   patchDir database authurId est.ra\n"
  );
}

void patchDir(char *database, char *authorId, char *raFile, char *outTab)
/* patchDir - Patch directions of some ESTs. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char query[256], **row;
int i, rowSize = 18;
char *three = "3";
char *five = "5";
char *acc;
char authors[1024];
struct hash *fiveHash = newHash(0), *threeHash = newHash(0);
struct lineFile *lf = lineFileOpen(raFile, TRUE);
struct hash *ra;
FILE *f;
int autMatchCount = 0, fiveCount = 0, threeCount = 0;

sqlSafef(query, sizeof query, "select name from author where id = %s", authorId);
if (sqlQuickQuery(conn, query, authors, sizeof(authors)) == NULL)
    errAbort("%s is not a valid author ID", authorId);
printf("scanning %s for %s\n", raFile, authors);
while ((ra = raNextRecord(lf)) != NULL)
    {
    char *aut = hashFindVal(ra, "aut");
    if (aut != NULL && sameString(aut, authors))
        {
	char *dir = hashFindVal(ra, "dir");
	char *acc = hashFindVal(ra, "acc");
	if (acc == NULL)
	    {
	    warn("Couldn't find accession line %d of %s", lf->lineIx, lf->fileName);
	    continue;
	    }

	++autMatchCount;
	if (dir != NULL)
	   {
	   if (dir[0] == '5')
	       {
	       ++fiveCount;
	       hashAdd(fiveHash, acc, NULL);
	       }
	   else if (dir[0] == '3')
	       {
	       ++threeCount;
	       hashAdd(threeHash, acc, NULL);
	       }
	   }
	}
    hashFree(&ra);
    }
printf("Got %d matches including %d 5' and %d 3'\n", 
	autMatchCount, fiveCount, threeCount);
lineFileClose(&lf);

f = mustOpen(outTab, "w");
sqlSafef(query, sizeof query, "select * from mrna where author = %s", authorId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    acc = row[1];
    if (hashLookup(fiveHash, acc))
        row[3] = five;
    else if (hashLookup(threeHash, acc))
        row[3] = three;
    for (i=0; i<rowSize-1; ++i)
        fprintf(f, "%s\t", row[i]);
    fprintf(f, "%s\n", row[rowSize-1]);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
carefulClose(&f);
printf("Try updating database with %s\n", outTab);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 5)
    usage();
patchDir(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
