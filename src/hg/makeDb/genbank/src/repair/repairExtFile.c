/* repairExtFile - repair operations on the gbSeq/gbExtFile */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "gbVerb.h"
#include "refPepRepair.h"


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"dryRun", OPTION_BOOLEAN},
    {"accFile", OPTION_STRING},
    {"verbose", OPTION_INT},
    {NULL, 0}
};

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s\n\n"
         "repairExtFile [options] repairTask db [db2 ...]\n"
         "\n"
         "Repair the gbSeq and gbExtFile tables\n"
         "repairTask is one of:\n"
         "   refPepList - list refSeq peptide gbSeqs that don't point to\n"
         "                valid gbExtFile entries.\n"
         "   refPepRepair - repair refSeq peptide gbSeqs that don't point to\n"
         "                  valid gbExtFile entries.\n"
         "\n"
         "Options:\n"
         "  -dryRun - don't actually update the database\n"
         "  -accFile=file - file with list of accessions to operate on\n"
         "  -verbose=1\n"
         "           1 - stats only\n"
         "           2 - sequences repaired\n"
         "           3 - more details\n"
         "           4 - sql trace\n"
         "           5 - fasta scan\n",
         msg);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *repairTask, **dbs, *accFile = NULL;;
boolean dryRun;
int ndbs, i;
optionInit(&argc, argv, optionSpecs);
if (argc < 3)
    usage("wrong # args");
gbVerbInit(optionInt("verbose", 1));
dryRun = optionExists("dryRun");
accFile = optionVal("accFile", NULL);

repairTask = argv[1];
dbs = argv+2;
ndbs = argc-2;

if (sameString(repairTask, "refPepList"))
    {
    if (accFile != NULL)
        errAbort("-accFile is not valid for refPepList");
    for (i = 0; i < ndbs; i++)
        refPepList(dbs[i], stdout);
    }
else if (sameString(repairTask, "refPepRepair"))
    {
    for (i = 0; i < ndbs; i++)
        refPepRepair(dbs[i], accFile, dryRun);
    }
else
    errAbort("invalid repairTask: %s", repairTask);

return 0;
}



/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
