/* htmlCheck - Do a little reading and verification of html file. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "errAbort.h"
#include "options.h"
#include "hdb.h"
#include "htmlPage.h"
#include "verbose.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkUrlsInTable - verify links in a table (column name 'url')\n"
  "usage:\n"
  "   checkUrlsInTable database table\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

// Might want to parameterize this later
#define urlColumn "url" 

int checkUrlsInTable(char *database, char *table)
/* Read url. Switch on command and dispatch to appropriate routine. */
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
sqlSafef(query, sizeof(query),
                "select %s from %s where %s is not null and %s <> ''", 
                        urlColumn, table, urlColumn, urlColumn);
struct slName *url, *urls;
urls = sqlQuickList(conn, query);
int errs = 0;
char *fullText;
struct htmlStatus *status;
for (url = urls; url != NULL; url = url->next)
    {
    fullText = htmlSlurpWithCookies(url->name, NULL);
    status = htmlStatusParse(&fullText);
    if (status == NULL)
        {
        printf("%s\tNULL\n", url->name);
        errs++;
        }
    else if (status->status == 200)
        {
        verbose(3, "%s\t200\n", url->name);
        continue;
        }
    else
        {
        printf("%s\t%d\n", url->name, status->status);
        errs++;
        }
    }
hFreeConn(&conn);
return errs;
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
char *database = argv[1];
char *table = argv[2];
return checkUrlsInTable(database, table);
}
