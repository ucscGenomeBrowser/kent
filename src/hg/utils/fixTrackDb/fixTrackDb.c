/* fixTrackDb - check for data accessible for everything in trackDb. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"
#include "trackDb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "fixTrackDb - check for data accessible for everything in trackDb table\n"
  "usage:\n"
  "   fixTrackDb database trackDbTable\n"
  "options:\n"
  "  -gbdbList=list - list of files to confirm existance of bigDataUrl files\n"
  );
}

static struct hash *gbdbHash = NULL;

/* Command line validation table. */
static struct optionSpec options[] = {
   {"gbdbList", OPTION_STRING},
   {NULL, 0},
};

static void drop(char *trackDb, char *name)
{
printf("delete from %s where tableName = '%s';\n", trackDb, name);
}

static boolean checkAvail(char *db, char *trackDb, struct trackDb *tdbList, boolean top)
{
struct trackDb *tdb;
boolean foundOne = FALSE;
struct hash *superHash = newHash(0);

for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    if (top && (tdb->parent != NULL))
        {
        if (hashLookup(superHash, tdb->parent->track) == NULL)
            hashAdd(superHash, tdb->parent->track, 0);
        }
    if (tdb->subtracks != NULL) // composite
        {
        if (!checkAvail(db, trackDb, tdb->subtracks, FALSE))
            {
            // none of the children exist, drop composite
            drop(trackDb,tdb->track);
            continue;
            }
        foundOne = TRUE;
        }
    else if (tdbIsDownloadsOnly(tdb) || trackDataAccessibleHash(db, tdb, gbdbHash))
        {
        foundOne = TRUE;
        }
    else
        {
        drop(trackDb,tdb->track);
        continue;
        }

    // this tdb has data
    if (top && (tdb->parent != NULL))
        {
        struct hashEl *hel;

        if ((hel = hashLookup(superHash, tdb->parent->track)) == NULL)
            errAbort("can't find superTrack %s\n", tdb->parent->track);

        // record that supertrack has at least one child
        hel->val = NULL + 1;
        }
    }
if (top)
    {
    struct hashCookie cook = hashFirst(superHash);
    struct hashEl* hel;
    
    while((hel = hashNext(&cook)) != NULL)
        {
        // none of the children exist, drop supertrack
        if (hel->val == NULL)
            drop(trackDb,(char *)hel->name);
        }
    }
return foundOne;
}

static struct trackDb *loadTdb(char *db, char *trackDb)
// load the trackDb table
{
struct hash *loaded = hashNew(0);
extern boolean loadOneTrackDb(char *db, char *where, char *tblSpec,
                              struct trackDb **tdbList, struct hash *loaded);
struct trackDb *tdbList = NULL;
if (!loadOneTrackDb(db, NULL, trackDb, &tdbList, loaded))
    errAbort("can't open %s\n", trackDb);
hashFree(&loaded);

/* fill in supertrack fields, if any in settings */
trackDbSuperMarkup(tdbList);
trackDbAddTableField(tdbList);
return tdbList;
}

void fixTrackDb(char *db, char *trackDb)
/* fixTrackDb - check for data accessible for everything in trackDb. */
{
struct trackDb *tdbList = loadTdb(db, trackDb);
tdbList = trackDbLinkUpGenerations(tdbList);
checkAvail(db, trackDb, tdbList, TRUE);
}

struct hash *hashLines(char *fileName)
/* Read all lines in file and put them in a hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[1];
struct hash *hash = newHash(0);
while (lineFileRow(lf, row))
    hashAdd(hash, row[0], NULL);
lineFileClose(&lf);
return hash;
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();

char *gbdbList = NULL;
gbdbList = optionVal("gbdbList", gbdbList);
if (gbdbList)
    gbdbHash = hashLines(gbdbList);

fixTrackDb(argv[1], argv[2]);
return 0;
}
