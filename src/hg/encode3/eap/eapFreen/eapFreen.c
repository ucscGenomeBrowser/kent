/* eapFreen - Little program to help test things and figure them out.  It has no permanent one fixed purpose.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"
#include "obscure.h"
#include "net.h"
#include "../../encodeDataWarehouse/inc/encodeDataWarehouse.h"
#include "../../encodeDataWarehouse/inc/edwLib.h"
#include "htmlPage.h"
#include "eapDb.h"
#include "eapLib.h"
#include "eapGraph.h"

char *submitUrl = "hgwdev-kent.gi.ucsc.edu/cgi-bin/edwScriptSubmit";
char *statusUrl = "hgwdev-kent.gi.ucsc.edu/cgi-bin/edwScriptSubmitStatus";
char *userId = "jagegixelumaveteyigu";
char *password = "cdefbdb6677d4c94a9f274c52e2c8dfa";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eapFreen - Little program to help test things and figure them out.  It has no permanent one fixed purpose.\n"
  "usage:\n"
  "   eapFreen manifestUrl\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void eapFreen(char *manifestUrl)
/* eapFreen - Little program to help test things and figure them out.  It has no permanent one 
 * fixed purpose.. */
{
/* Construct dyString for URL */
struct dyString *dyUrl = dyStringNew(0);
dyStringPrintf(dyUrl, "https://%s?user=%s&password=%s", submitUrl, userId, password);
dyStringPrintf(dyUrl, "&url=%s", cgiEncode(manifestUrl));
uglyf("%s\n", dyUrl->string);


/* Grab response */
struct htmlPage *page = htmlPageGet(dyUrl->string);
puts(page->fullText);

dyStringClear(dyUrl);
dyStringPrintf(dyUrl, "https://%s?user=%s&password=%s", statusUrl, userId, password);
dyStringPrintf(dyUrl, "&url=%s", cgiEncode(manifestUrl));
uglyf("%s\n", dyUrl->string);

page = htmlPageGet(dyUrl->string);
puts(page->fullText);

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
eapFreen(argv[1]);
return 0;
}
