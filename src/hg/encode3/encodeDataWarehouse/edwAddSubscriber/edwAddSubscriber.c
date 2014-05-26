/* edwAddSubscriber - Add a subscriber - a program that runs automatically when a file is 
 * received. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "encodeDataWarehouse.h"
#include "cheapcgi.h"
#include "edwLib.h"
#include "ra.h"

char *filePattern = "*";
char *dirPattern = "*";
char *tagPattern = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwAddSubscriber - Add a subscriber - a program that runs automatically when a file is\n"
  "received.\n"
  "usage:\n"
  "   edwAddSubscriber name 'command line including %%u for edw file ID'\n"
  "options:\n"
  "   '-filePattern=wildCardString' restrict subscription to files matching wildcard.\n"
  "   '-dirPattern=wildCardString' restrict subscription to files from given hub directories\n"
  "   -tagPattern=raFile restrict subscription to files that have tags matching vals in raFile\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"filePattern", OPTION_STRING},
   {"dirPattern", OPTION_STRING},
   {"tagPattern", OPTION_STRING},
   {NULL, 0},
};

void readFirstStanzaAsCgi(char *fileName, struct dyString *dy)
/* Read ra file's first stanza as tags and save them as if they were cgi-vars */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct slPair *pair, *pairList = raNextStanzAsPairs(lf);
lineFileClose(&lf);

for (pair = pairList; pair != NULL; pair = pair->next)
    {
    cgiEncodeIntoDy(pair->name, pair->val, dy);
    }
slPairFreeValsAndList(&pairList);
}

void edwAddSubscriber(char *name, char *command)
/* edwAddSubscriber - Add a subscriber - a program that runs automatically when a file is 
 * received. */
{
/* Check that there is a '%u' in command. */
if (!stringIn("%u", command))
    errAbort("Command does not include a %%u to substitute for file id");

struct dyString *tagAsCgi = dyStringNew(0);

if (tagPattern != NULL)
    readFirstStanzaAsCgi(tagPattern, tagAsCgi);

struct sqlConnection *conn = sqlConnect(edwDatabase);
struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, 
    "insert edwSubscriber (name, filePattern, dirPattern, tagPattern, onFileEndUpload) "
    "values ('%s','%s','%s','%s','%s')",
    name, filePattern, dirPattern, tagAsCgi->string, command);
sqlUpdate(conn, query->string);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
filePattern = optionVal("filePattern", filePattern);
dirPattern = optionVal("dirPattern", dirPattern);
tagPattern = optionVal("tagPattern", tagPattern);

if (argc != 3)
    usage();
edwAddSubscriber(argv[1], argv[2]);
return 0;
}
