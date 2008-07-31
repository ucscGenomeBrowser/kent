/* customTrackTester - test custom tracks functions in library */

#include "common.h"
#include "obscure.h"
#include "options.h"
#include "portable.h"
#include "hdb.h"
#include "customFactory.h"

static char const rcsid[] = "$Id: customTrackTester.c,v 1.3.76.1 2008/07/31 05:21:39 markd Exp $";

void usage()
/* explain usage and exit */
{
errAbort(
    "customTrackTester - test program for custom tracks library\n\n"
    "usage:\n"
    "    customTrackTester [options] task [args...]\n"
    "         where task/args can be:\n"
    "                   parse inputFile [trashFile]\n"
    "                   check outFile expectedFile\n"
    "options:\n"
    "   -db=<db>  - set database (defaults to %s)\n", hDefaultDb()
    );
}

static struct optionSpec optionSpecs[] = {
    {"db", OPTION_STRING},
    {NULL, 0}
};

static void parseCustomTracks(char *db, char *inFile, char *trashFile)
/* parse tracks from input file, and also from trashfile if not null */
{
char *text;
struct customTrack *ctList = NULL, *oldCts = NULL;

readInGulp(inFile, &text, NULL);
/* read new CT's from input */
ctList = customFactoryParse(db, text, FALSE, NULL);
verbose(3, "parsed %d tracks from %s\n", slCount(ctList), inFile);
if (trashFile)
    {
    /* read old CT's from trash file */
    oldCts = customFactoryParse(db, trashFile, TRUE, NULL);
    /* merge old and new */
    ctList = customTrackAddToList(ctList, oldCts, NULL, TRUE);
    }
/* save to new trash file */
static struct tempName tn;
makeTempName(&tn, "ctTest", ".bed");
customTracksSaveFile(db, ctList, tn.forCgi);

/* reload from new trash file */
ctList = NULL;
ctList = customFactoryParse(db, tn.forCgi, TRUE, NULL);
customTracksSaveFile(db, ctList, "stdout");

/* cleanup */
unlink(tn.forCgi);
}

static void checkCustomTracks(char *db, char *outFile, char *expectedFile)
/* compare track lines of output file with expected.  Return error
 * settings are not a proper subset */
{
struct hash *expHash = hashNew(0);
struct customTrack *ct = NULL, *expCt = NULL;
struct customTrack *newCts = customFactoryParse(db, outFile, TRUE, NULL);
struct customTrack *expCts = customFactoryParse(db, expectedFile, TRUE, NULL);
verbose(3, "found %d tracks in output file %s, %d tracks in expected file %s\n",
                slCount(newCts), outFile, slCount(expCts), expectedFile);
for (ct = expCts; ct != NULL; ct = ct->next)
    hashAdd(expHash, ct->tdb->tableName, ct);
for (ct = newCts; ct != NULL; ct = ct->next)
    {
    if ((expCt = hashFindVal(expHash, ct->tdb->tableName)) == NULL)
        errAbort("ct %s not found in expected", ct->tdb->tableName);
    verbose(3, "output settings: %s %s\n%s\n", ct->tdb->tableName, 
                                ct->tdb->shortLabel, ct->tdb->settings);
    verbose(3, "expected settings: %s %s\n%s\n", expCt->tdb->tableName,
                                expCt->tdb->shortLabel, expCt->tdb->settings);
    struct hash *newSettings = trackDbHashSettings(ct->tdb);
    struct hash *expSettings = trackDbHashSettings(expCt->tdb);
    struct hashCookie hc = hashFirst(expSettings);
    struct hashEl *hel;
    while ((hel = hashNext(&hc)) != NULL)
        {
        char *setting = (char *)hel->name;
        /* ignore DB table name -- it will always differ */
        if (sameString(setting, "dbTableName"))
            continue;
        char *expVal = (char *)hel->val;
        char *newVal = NULL;
        if ((newVal = (char *)hashFindVal(newSettings, setting)) == NULL)
            errAbort("ct %s setting %s not found in new", 
                        ct->tdb->tableName, setting);
        if (differentString(newVal, expVal))
            errAbort("ct %s setting %s differs from expected: %s should be %s",
                        ct->tdb->tableName, setting, newVal, expVal);
        }
    }
}

int main(int argc, char *argv[])
{
optionInit(&argc, argv, optionSpecs);
char *db = optionVal("db", hDefaultDb());
if (argc < 2)
    usage();
char *task = argv[1];
if (sameString(task, "parse"))
    {
    if (argc < 3)
        usage();
    char *trashFile = NULL;
    char *inFile = argv[2];
    if (argc > 3)
        trashFile = argv[3];
    parseCustomTracks(db, inFile, trashFile);
    }
else if (sameString(task, "check"))
    {
    if (argc < 4)
        usage();
    char *outFile = argv[2];
    char *expFile = argv[3];
    checkCustomTracks(db, outFile, expFile);
    }
else
    usage();
return 0;
}
