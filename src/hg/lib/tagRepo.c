#include "common.h"
#include "trackDb.h"
#include "grp.h"
#include "hubConnect.h"
#include "trackHub.h"
#include "tagRepo.h"

struct tagRepo
{
struct tagRepo *next;
char *fileName;
struct tagStorm *storm;
struct slPair *hashes; // keyed off tag
} ;

static struct hash *tagRepoHash;

struct tagRepo *tagRepoFromFile(char *tagStormFile)
{
struct tagRepo *repo;
if (tagRepoHash == NULL)
    tagRepoHash = newHash(5);

if ((repo = hashFindVal(tagRepoHash, tagStormFile)) == NULL)
    {
    AllocVar(repo);
    repo->fileName = cloneString(tagStormFile);
    repo->storm = tagStormFromFile(tagStormFile);
    hashAdd(tagRepoHash, tagStormFile, repo);
    }

return repo;
}

struct hash *tagRepoTagHash(struct tagRepo *repo, char *tag)
{
struct slPair *pair;

if ((pair = slPairFind(repo->hashes, tag)) == NULL)
    {
    AllocVar(pair);
    pair->name = cloneString(tag);
    pair->val = tagStormUniqueIndex(repo->storm, tag);
    slAddHead(&repo->hashes, pair);
    }

return pair->val;
}

struct slPair *tagRepoPairs(char *tagStormFile, char *tagName,  char *tagValue)
{
struct tagRepo *repo = tagRepoFromFile(tagStormFile);

struct hash *hash = tagRepoTagHash(repo, tagName);
struct tagStanza *stanza = hashFindVal(hash, tagValue);
return tagListIncludingParents(stanza);
}

static void preloadHubs(char *database)
{
struct hubConnectStatus *hubs = hubConnectGetHubs();
for(; hubs; hubs = hubs->next)
    {
    struct trackHub *trackHub = hubs->trackHub;
    struct trackHubGenome *hubGenome = trackHubFindGenome(trackHub, database);
    struct hashEl *hel;

    if ((hel = hashLookup(hubGenome->settingsHash, "tagStorm")) != NULL)
        {
        char *absStormName  = trackHubRelativeUrl(trackHub->url, hel->val);
        tagRepoFromFile(absStormName);
        }
    }
}

struct slPair *tagRepoAllPairs(char *database)
{
preloadHubs(database);

if (tagRepoHash == NULL)
    return NULL;

struct hashCookie cookie = hashFirst(tagRepoHash);
struct hashEl *hel;
struct slPair *pairList = NULL;
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct tagRepo *repo = hel->val;
    struct slPair *pair;

    struct hash *fieldHash = tagStormFieldHash(repo->storm);
    struct hashCookie cookie2 = hashFirst(fieldHash);
    while ((hel = hashNext(&cookie2)) != NULL)
        {
        AllocVar(pair);
        pair->name = hel->name;
        pair->val = hel->name;
        slAddHead(&pairList, pair);
        //struct slPair *allValues = tagRepoAllValues(tagPair->name);
        //allValues->name = NULL;
        }
    }

return pairList;
}

struct slPair *tagRepoAllValues(char *database, char *tag)
{
preloadHubs(database);
if (tagRepoHash == NULL)
    return NULL;

struct hashCookie cookie = hashFirst(tagRepoHash);
struct hashEl *hel;
struct slPair *pairList = NULL, *pair;
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct tagRepo *repo = hel->val;
    struct hash *valueHash = tagStormCountTagVals(repo->storm, tag, NULL);
    struct hashCookie cookie2 = hashFirst(valueHash);
    while ((hel = hashNext(&cookie2)) != NULL)
        {
        AllocVar(pair);
        pair->name = hel->name;
        slAddHead(&pairList, pair);

        }
    }
return pairList;
}
