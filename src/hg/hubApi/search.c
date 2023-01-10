#include "dataApi.h"
#include "hgFind.h"
#include "cartTrackDb.h"
#include "cartJson.h"

static struct trackDb *addSupers(struct trackDb *trackList)
/* Insert supertracks into the hierarchy. */
{
struct trackDb *newList = NULL;
struct trackDb *tdb, *nextTdb;
struct hash *superHash = newHash(5);

for(tdb = trackList; tdb;  tdb = nextTdb)
    {
    nextTdb = tdb->next;
    if (tdb->parent)
        {
        // part of a super track
        if (hashLookup(superHash, tdb->parent->track) == NULL)
            {
            hashStore(superHash, tdb->parent->track);

            slAddHead(&newList, tdb->parent);
            }
        slAddTail(&tdb->parent->subtracks, tdb);
        }
    else
        slAddHead(&newList, tdb);
    }
slReverse(&newList);
return newList;
}

static void hashTdbNames(struct trackDb *tdb, struct hash *trackHash)
/* Store the track names for lookup, except for knownGene which gets its own special
 * category in the UI */
{
struct trackDb *tmp;
for (tmp = tdb; tmp != NULL; tmp = tmp->next)
    {
    if (tmp->subtracks)
        hashTdbNames(tmp->subtracks, trackHash);
    if (!sameString(tmp->table, tmp->track))
        hashAdd(trackHash, tmp->track, tmp);
    hashAdd(trackHash, tmp->table, tmp);
    }
}

void hashTracksAndGroups(struct cart *cart, char *db)
/* get the list of tracks available for this assembly. Note that this implicitly
 * makes connected hubs show up in the trackList struct, which means we get item
 * search for connected hubs for free */
{
static struct trackDb *tdbList = NULL;
static struct grp *grpList = NULL;
if (tdbList != NULL && grpList != NULL)
    return;

// hgFindTrackHash/hgFindGroupHash are globals from hgFind.h
if (!hgFindTrackHash)
    hgFindTrackHash = hashNew(0);
if (!hgFindGroupHash)
    hgFindGroupHash  = hashNew(0);
cartTrackDbInit(cart, &tdbList, &grpList, FALSE);
if (!tdbList)
    errAbort("Error getting tracks for %s", db);
struct trackDb *superList = addSupers(tdbList);
tdbList = superList;
hashTdbNames(superList, hgFindTrackHash);
struct grp *g;
for (g = grpList; g != NULL; g = g->next)
    if (!sameString(g->name, "allTracks") && !sameString(g->name, "allTables"))
        hashStore(hgFindGroupHash, g->name);
}

struct searchCategory *makeCategsFromString(char *categoriesString, char *db, struct cart *bogusCart)
/* Turn a ';' separated string of table names into a list of searchCategory */
{
struct searchCategory *ret = NULL, *category = NULL;
int maxCategs = 1024;
char *chopped[maxCategs];
int numCategs = chopString(categoriesString, ",", chopped, maxCategs);
int i;
for (i = 0; i < numCategs; i++)
    {
    char *categName = chopped[i];
    if (!sameString(categName, "allTracks"))
        category = makeCategory(bogusCart, categName, NULL, db, hgFindGroupHash);
    else
        {
        struct hashEl *hel, *helList = hashElListHash(hgFindTrackHash);
        for (hel = helList; hel != NULL; hel = hel->next)
            {
            struct trackDb *tdb = hel->val;
            if (!sameString(tdb->track, "knownGene") && !sameString(tdb->table, "knownGene"))
                slAddHead(&category, makeCategory(bogusCart, tdb->track, NULL, db, hgFindGroupHash));
            }
        // the hgFindTrackHash can contain both a composite track (where makeCategory would make
        // categories for each of the subtracks, and a subtrack, where makeCategory just makes a
        // single category, which means our final list can contain duplicate categories, so do a
        // uniqify here so we only have one category for each category
        slUniqify(&category, cmpCategories, searchCategoryFree);
        }
    if (category != NULL)
        {
        if (ret)
            slCat(&ret, category);
        else
            ret = category;
        }
    }
return ret;
}

void getSearchResults(char *searchTerm, char *db, char *hubUrl, char *categories)
/* Output search results for db, potentially limited by categories */
{
initGenbankTableNames(db);
struct cart *bogusCart = cartOfNothing();
cartAddString(bogusCart, "db", db);
hashTracksAndGroups(bogusCart, db);
struct jsonWrite *jw = apiStartOutput();
struct searchCategory *searchCategoryList = NULL;
if (isNotEmpty(categories))
    searchCategoryList = makeCategsFromString(categories, db, bogusCart);
else
    searchCategoryList = getAllCategories(bogusCart, db, hgFindGroupHash);
struct hgPositions *hgp = NULL;
jsonWriteString(jw, "genome", db);
hgp = hgPositionsFind(db, searchTerm, "", "searchExample", bogusCart, FALSE, measureTiming, searchCategoryList);
if (hgp)
    hgPositionsJson(jw, db, hgp, NULL);
apiFinishOutput(0, NULL, jw);
}

void apiSearch(char *words[MAX_PATH_INFO])
/* 'search' function */
{
char *hubUrl = cgiOptionalString("hubUrl");
char *extraArgs = verifyLegalArgs(argSearch);
if (extraArgs)
    apiErrAbort(err400, err400Msg, "extraneous arguments found for function /search'%s'", extraArgs);

// verify required genome and searchTerm parameters exist
char *db = cgiOptionalString("genome");
if (isEmpty(db))
    apiErrAbort(err400, err400Msg, "missing URL variable genome=<ucscDb> name for endpoint '/search/");
char *searchTerm = cgiOptionalString(argSearchTerm);
if (isEmpty(searchTerm))
    apiErrAbort(err400, err400Msg, "missing URL variable %s=<search query> for endpoint '/search/", argSearchTerm);

char *categories = cgiOptionalString(argCategories);

getSearchResults(searchTerm, db, hubUrl, categories);
}
