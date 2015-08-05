/* htmlCheck - Do a little reading and verification of html file. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "errAbort.h"
#include "options.h"
#include "hdb.h"
#include "htmlPage.h"
#include "verbose.h"


/* default column to check */
char *col = "url";

/* default url prefix  */
char *prefix = "";

/* error string to look for in returned HTML */
char *errString = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkUrlsInTable - verify links in a table\n"
  "usage:\n"
  "   checkUrlsInTable database table\n"
  "options:\n"
  "    -strict          - warn about 302 redirects, etc.\n"
  "    -col=XX          - column to check (default '%s')\n"
  "    -prefix=XX       - prefix to column value to generate url\n"
  "    -errString=XX    - error string to detect in returned HTML\n"
  , col);
}

static struct optionSpec options[] = {
   {"strict", OPTION_BOOLEAN},
   {"col", OPTION_STRING},
   {"prefix", OPTION_STRING},
   {"errString", OPTION_STRING},
   {NULL, 0},
};


int checkUrlsInTable(char *database, char *table, boolean strict)
/* Read url. Switch on command and dispatch to appropriate routine. */
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
char url[256];
sqlSafef(query, sizeof(query),
                "select %s from %s where %s is not null and %s <> ''", 
                        col, table, col, col);
struct slName *item, *items;
items = sqlQuickList(conn, query);
int errs = 0;
char *fullText;
struct htmlStatus *status;
for (item = items; item != NULL; item = item->next)
    {
    safef(url, sizeof(url), "%s%s", prefix, item->name);
    verbose(4, "%s\n", url);
    usleep(100);
    fullText = htmlSlurpWithCookies(url, NULL);
    status = htmlStatusParse(&fullText);
    if (status == NULL)
        {
        printf("%s\tNULL\n", url);
        errs++;
        continue;
        }
    if (status->status == 200)
        {
        /* OK */
        verbose(3, "%s\t200\n", url);
        if (errString != NULL)
            {
            if (strstr(fullText, errString))
                {
                printf("%s\t%s\n", url, errString);
                errs++;
                }
            }
        continue;
        }
    if (!strict)
        {
        if (status->status == 302)  // "temporary" redirect
            {
            verbose(2, "%s\t302\n", url);
            continue;
            }
        }
    printf("%s\t%d\n", url, status->status);
    errs++;
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
col = optionVal("col", col);
prefix = optionVal("prefix", prefix);
errString = optionVal("errString", NULL);
return checkUrlsInTable(database, table, optionExists("strict"));
}
