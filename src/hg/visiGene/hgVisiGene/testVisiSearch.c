/* testVisiSearch - test visiSearch module, which is hard to
 * do on the web, since it is called by the frame-maker
 * rather than an individual web page, and so no diagnostic
 * output can show up. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "jksql.h"
#include "visiSearch.h"

char *database = "visiGene";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testVisiSearch - test visiGene search module.  Give it the\n"
  "search terms in the command line and it will print a list of\n"
  "weighted visiGene image ID's\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}


static struct optionSpec options[] = {
   {NULL, 0},
};

void testVisiSearch(char *words[], int wordCount)
/* Test visiGene search module. */
{
struct sqlConnection *conn = sqlConnect(database);
struct dyString *search = dyStringNew(0);
struct visiMatch *matchList, *match;
int i;
int maxPrint=3;
for (i=0; i<wordCount; ++i)
    {
    if (i != 0)
       dyStringAppendC(search, ' ');
    dyStringAppend(search, words[i]);
    }
matchList = visiSearch(conn, search->string);
uglyTime("Searched time");
printf("%d matches to %s\n", slCount(matchList), search->string);
for (i=0, match = matchList; match != NULL && i<maxPrint; match = match->next,++i)
    {
    printf(" %d %f %d\n", match->imageId, match->weight,
    	bitCountRange(match->wordBits, 0, wordCount));
    }
#ifdef SOON
#endif /* SOON */
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
uglyTime(0);
testVisiSearch(argv+1, argc-1);
return 0;
}
