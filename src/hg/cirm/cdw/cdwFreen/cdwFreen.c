/* cdwFreen - Temporary scaffolding frequently repurposed. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"
#include "tagStorm.h"
#include "rql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwFreen - Temporary scaffolding frequently repurposed\n"
  "usage:\n"
  "   cdwFreen user fileId\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void cdwFreen(char *userEmail, char *fileIdString)
/* cdwFreen - Temporary scaffolding frequently repurposed. */
{
struct sqlConnection *conn = cdwConnect();
struct cdwUser *user = cdwMustGetUserFromEmail(conn, userEmail);
int fileId = sqlUnsigned(fileIdString);
struct cdwFile *ef = cdwFileFromIdOrDie(conn, fileId);
boolean authorized = cdwCheckAccess(conn, ef, user, cdwAccessRead);
printf("%s is %s authorized on %s (%s)\n", userEmail, (authorized ? "indeed" : "not"),
    fileIdString, ef->submitFileName); 
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
cdwFreen(argv[1], argv[2]);
return 0;
}
