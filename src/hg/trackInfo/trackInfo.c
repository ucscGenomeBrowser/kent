/* trackInfo - Centralized repository for track information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "trackDb.h"
#include "sqlList.h"
#include "hdb.h"
#include "obscure.h"
#include "jksql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trackInfo - Centralized repository for track information\n"
  "usage:\n"
  "   trackInfo database trackDb.sql track.ra\n"
  );
}

void trackInfo(char *database, char *sqlFile, char *raFile)
/* Load track info from ra file into database. */
{
struct trackDb *btList = NULL, *bt;
char *tab = "trackDb.tab";
FILE *f = mustOpen(tab, "w");
char *create;
struct sqlConnection *conn = sqlConnect(database);
char query[256];
char *end;

btList = trackDbFromRa(raFile);
for (bt = btList; bt != NULL; bt = bt->next)
    trackDbTabOut(bt, f);
carefulClose(&f);
printf("Loaded %d track descriptions from %s\n", slCount(btList), raFile);

readInGulp(sqlFile, &create, NULL);
trimSpaces(&create);
end = create + strlen(create)-1;
if (end == ';') *end = 0;
sqlRemakeTable(conn, "trackDb", create);
sprintf(query, "load data local infile '%s' into table trackDb", tab);
sqlUpdate(conn, query);
sqlDisconnect(&conn);
printf("Loaded database %s\n", database);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
trackInfo(argv[1], argv[2], argv[3]);
return 0;
}
