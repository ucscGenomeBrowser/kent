/* trackDbToTxt - Convert a trackDb table to a trackDb.txt file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "trackDb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trackDbToTxt - Convert a trackDb table to a trackDb.txt file\n"
  "usage:\n"
  "   trackDbToTxt database table directory\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

static void outTdb(FILE *f, char *directory, struct hash *alreadyOutHash, struct trackDb *tdb)
{
if (hashLookup(alreadyOutHash, tdb->track) != NULL)
    return;

hashAdd(alreadyOutHash, tdb->track, tdb);

fprintf(f, "track %s\n", tdb->track);
fprintf(f, "type %s\n", tdb->type);
if (!isEmpty(tdb->html))
    {
    char htmlName[1024];

    safef(htmlName, sizeof htmlName, "%s/%s.html", directory, tdb->track);
    FILE *htmlF = mustOpen(htmlName, "w");

    fputs(tdb->html, htmlF);
    fprintf(f, "html %s.html\n", tdb->track);
    fclose(htmlF);
    }

struct hashCookie cookie = hashFirst(tdb->settingsHash);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    {
    if (differentString(hel->name, "track")
        && differentString(hel->name, "type")
        && differentString(hel->name, "html"))
        {
        fprintf(f, "%s %s\n", hel->name, (char *)hel->val);
        }
    }

fputc('\n', f);
//trackDbTabOut(tdb,f);
}

static void outputParents(FILE *f, char *directory, struct hash *trackDbHash, struct hash *alreadyOutHash, struct trackDb *tdb)
{
if (tdb == NULL)
    return;

outputParents(f, directory, trackDbHash, alreadyOutHash, tdb->parent);

outTdb(f, directory, alreadyOutHash, tdb);
}

#ifdef NOTNOW
char *name;
if ((name = hashFindVal(tdb->settingsHash, "parent")) != NULL)
    {
    char *space;
    if ((space = strchr(name, ' ')) != NULL)
        *space = 0;
    printf("looking for parent %s\n", name);
    if (hashLookup(alreadyOutHash, name) == NULL)
        {
        struct trackDb *parent = hashMustFindVal(trackDbHash, name);
        outputParents(f, trackDbHash, alreadyOutHash, parent);
        outTdb(f, alreadyOutHash, parent);
        }
    }
}
#endif

static void walkList(FILE *f, char *directory, struct trackDb *list, struct hash *trackDbHash, struct hash *alreadyOutHash)
{
if (list == NULL)
    return;
    
struct trackDb *tdb;
for(tdb = list; tdb; tdb = tdb->next)
    {
    trackDbHashSettings(tdb);
    walkList(f, directory, tdb->subtracks, trackDbHash, alreadyOutHash);
    if (hashLookup(tdb->settingsHash, "bigDataUrl") != NULL)
    //&& (hashLookup(tdb->settingsHash, "parent") == NULL))
        {
        outputParents(f, directory, trackDbHash, alreadyOutHash, tdb->parent);
        outTdb(f, directory, alreadyOutHash, tdb);
        }
    }
}

static void trackDbToTxt(char *db, char *table, char *directory)
/* trackDbToTxt - Convert a trackDb table to a trackDb.txt file. */
{
struct sqlConnection *conn = sqlConnect(db);
struct trackDb *list = trackDbLoadWhere(conn, table, NULL);
struct hash *trackDbHash = newHash(0);
struct hash *alreadyOutHash = newHash(0);
makeDirs(directory);
char trackDbName[1024];
safef(trackDbName, sizeof trackDbName, "%s/trackDb.txt", directory);
FILE *f = mustOpen(trackDbName, "w");
//struct trackDb *tdb;

//loadOneTrackDb(db, NULL, table, &list, trackDbHash);

trackDbSuperMarkup(list);
list = trackDbLinkUpGenerations(list);
//list = trackDbPolishAfterLinkup(list, db);

walkList(f, directory, list, trackDbHash, alreadyOutHash);
#ifdef nOTNOW
for(tdb = list; tdb; tdb = tdb->next)
    {
    trackDbHashSettings(tdb);
    //hashAdd(trackDbHash, tdb->track, tdb);
    }
#endif
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
trackDbToTxt(argv[1], argv[2], argv[3]);
return 0;
}
