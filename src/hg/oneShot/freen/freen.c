/* freen - My Pet Freen. */
#include "common.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "freen - My pet freen\n"
  "usage:\n"
  "   freen hgN\n");
}

char *contamClones[] =
   {
   "AC007777",
   "AC008027",
   "AC008248",
   "AC008972",
   "AC009624",
   "AC011090",
   "AC011335",
   "AC016087",
   "AC019017",
   "AC019351",
   "AC021555",
   "AC021569",
   "AC021574",
   "AC023079",
   "AC023934",
   "AC024373",
   "AC024562",
   "AC025002",
   "AC025003",
   "AC025326",
   "AC025598",
   "AC026940",
   "AC027158",
   "AC035148",
   "AC064799",
   "AC069076",
   "AC069192",
   };


void contam(char *database, char **badClones, int badCloneCount)
/* Look at clones and see if in database. */
{
struct sqlConnection *conn = sqlConnect(database);
int i;
char buf[256];
char query[256];
char *res;

for (i=0; i<badCloneCount; ++i)
    {
    char *clone = badClones[i];
    sprintf(query, "select name from clonePos where name like '%s%%'",
    	clone);
    res = sqlQuickQuery(conn, query, buf, sizeof(buf));
    if (res != NULL)
        printf("%s\n", res);
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2 )
    usage();
contam(argv[1], contamClones, ArraySize(contamClones));
return 0;
}
