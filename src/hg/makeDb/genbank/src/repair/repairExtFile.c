/* repairExtFile - repair operations on the gbSeq/gbExtFile */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "gbVerb.h"
#include "refPepRepair.h"

static char const rcsid[] = "$Id: repairExtFile.c,v 1.2 2006/01/20 19:25:34 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"dryRun", OPTION_BOOLEAN},
    {"verbose", OPTION_INT},
    {NULL, 0}
};
boolean gDryRun = FALSE;  /* don't actually update database */

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s\n\n"
         "repairExtFile [options] db repairTask\n"
         "\n"
         "Repair the gbSeq and gbExtFile tables\n"
         "repairTask is one of:\n"
         "   refPepList - list refSeq peptide gbSeqs that don't point to\n"
         "                valid gbExtFile entries.\n"
         "   refPepRepair - repair refSeq peptide gbSeqs that don't point to\n"
         "                  valid gbExtFile entries.\n"
         "\n"
         "Options:\n"
         "  -verbose=n\n"
         "           1 - stats only\n"
         "           2 - sequences repaired\n"
         "          >3 - sql trace\n"
         "  -dryRun - don't actually update the database\n",
         msg);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *repairTask;
struct sqlConnection *conn;
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage("wrong # args");
gbVerbInit(optionInt("verbose", 0));
gDryRun = optionExists("dryRun");

conn = sqlConnect(argv[1]);
repairTask = argv[2];

if (sameString(repairTask, "refPepList"))
    refPepList(conn, stdout);
else if (sameString(repairTask, "refPepRepair"))
    refPepRepair(conn, gDryRun);
else
    errAbort("invalid repairTask: %s", repairTask);

return 0;
}



/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
