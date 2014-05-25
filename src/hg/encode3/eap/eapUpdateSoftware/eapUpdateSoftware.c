/* eapUpdateSoftware - Get a new version of software.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "eapDb.h"
#include "eapLib.h"

char *clVersion = NULL;
char *clMd5 = NULL;
enum eapRedoPriority redoPriority = erpNoRedo;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eapUpdateSoftware - Get a new version of software.\n"
  "usage:\n"
  "   eapUpdateSoftware software 'notes on update'\n"
  "options:\n"
  "   -version=version  - update version # with this\n"
  "   -md5=md5hex       - use this md5 sum rather than having program calculate it\n"
  "   -redoPriority     - -1 for no redo, 0 for agnostic, 1 recommended upgrade, 2 must redo\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"version", OPTION_STRING},
   {"md5", OPTION_STRING},
   {"redoPriority", OPTION_INT},
   {NULL, 0},
};

void eapUpdateSoftware(char *software, char *notes)
/* eapUpdateSoftware - Get a new version of software.. */
{
char query[512];
struct sqlConnection *conn = eapConnectReadWrite();
char *version = clVersion;
if (version == NULL)
    {
    sqlSafef(query, sizeof(query),
	"select version from eapSwVersion where software='%s' order by id desc", software);
    version = sqlQuickString(conn, query);
    if (version == NULL)
       errAbort("Can't find existing version of %s, please use version option on command line",
	    software);
    }

char *md5 = clMd5;
char md5Hex[33];
if (md5 == NULL)
    {
    eapMd5ForCommand(software, md5Hex);
    md5 = md5Hex;
    }

sqlSafef(query, sizeof(query), 
    "select count(*) from eapSwVersion where software='%s'"
    "   and version='%s' and md5='%s'", software, version, md5);
int existing = sqlQuickNum(conn, query);
if (existing > 0)
    errAbort("Warn %d existing %s %s %s in eapSwVersion", existing, software, version, md5);

sqlSafef(query, sizeof(query),
    "insert eapSwVersion (software, version, md5, redoPriority, notes) "
                " values ('%s', '%s', '%s', %d, '%s')"
    , software, version, md5, redoPriority, notes);
sqlUpdate(conn, query);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clVersion = optionVal("version", clVersion);
clMd5 = optionVal("md5", clMd5);
redoPriority = optionInt("redoPriority", redoPriority);
if (redoPriority < -1 || redoPriority > 2)
    errAbort("redoPriority must be between -1 and 2");
eapUpdateSoftware(argv[1], argv[2]);
return 0;
}
