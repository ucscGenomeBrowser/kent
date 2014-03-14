/* eapFreen - Little program to help test things and figure them out.  It has no permanent one fixed purpose.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "net.h"
#include "../../encodeDataWarehouse/inc/encodeDataWarehouse.h"
#include "../../encodeDataWarehouse/inc/edwLib.h"
#include "eapDb.h"
#include "eapLib.h"
#include "eapGraph.h"
#include "longList.h"

char *host = "ec2-54-193-134-19.us-west-1.compute.amazonaws.com";
char *userId = "XXXXXXXX";
char *password = "xxxxxxxxxxxxxxxx";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eapFreen - Little program to help test things and figure them out.  It has no permanent one fixed purpose.\n"
  "usage:\n"
  "   eapFreen type input.json\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void eapFreen(char *type, char *jsonFile)
/* eapFreen - Little program to help test things and figure them out.  It has no permanent one 
 * fixed purpose.. */
{
if (!sameString(type, "software") && !sameString(type, "software_version") 
    && !sameString(type, "analysis_step") && !sameString(type, "analysis_run"))
    errAbort("Unrecognized type %s", type);
char *jsonText = NULL;
size_t jsonSize = 0;
readInGulp(jsonFile, &jsonText, &jsonSize);

/* Construct dyString for URL */
struct dyString *dyUrl = dyStringNew(0);
dyStringPrintf(dyUrl, "http://%s:%s@%s/%s/", userId, password, host, type);
uglyf("%s\n", dyUrl->string);

/* Construct dyString for http header */
struct dyString *dyHeader = dyStringNew(0);
dyStringPrintf(dyHeader, "Content-length: %d\r\n", (int)jsonSize);
dyStringPrintf(dyHeader, "Content-type: text/javascript\r\n");

/* Send header and then JSON */
int sd = netOpenHttpExt(dyUrl->string, "POST", dyHeader->string);
mustWriteFd(sd, jsonText, jsonSize);

/* Grab response */
struct dyString *dyText = netSlurpFile(sd);
close(sd);
puts(dyText->string);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
eapFreen(argv[1], argv[2]);
return 0;
}
