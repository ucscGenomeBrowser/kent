/* Track search code used by hgTracks CGI */

/* Copyright (C) 2012 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include <pthread.h>
#include "common.h"
#include "search.h"
#include "hCommon.h"
#include "memalloc.h"
#include "obscure.h"
#include "dystring.h"
#include "hash.h"
#include "cheapcgi.h"
#include "hPrint.h"
#include "htmshell.h"
#include "cart.h"
#include "hgTracks.h"
#include "web.h"
#include "jksql.h"
#include "hdb.h"
#include "mdb.h"
#include "fileUi.h"
#include "trix.h"
#include "jsHelper.h"
#include "imageV2.h"
#include "hgConfig.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "hubPublic.h"
#include "hubSearchText.h"
#include "errCatch.h"


#define TRACK_SEARCH_FORM        "trackSearch"
#define SEARCH_RESULTS_FORM      "searchResults"
#define TRACK_SEARCH_CURRENT_TAB "tsCurTab"
#define TRACK_SEARCH_SIMPLE      "tsSimple"
#define TRACK_SEARCH_ON_NAME     "tsName"
#define TRACK_SEARCH_ON_TYPE     "tsType"
#define TRACK_SEARCH_ON_GROUP    "tsGroup"
#define TRACK_SEARCH_ON_DESCR    "tsDescr"
#define TRACK_SEARCH_SORT        "tsSort"
#define TRACK_SEARCH_ON_HUBS     "tsIncludePublicHubs"

// the list of found tracks
struct slRef *tracks = NULL;

// for advanced search only:
// associates hub id's to hub urls that have search results,
// used to get huburls onto hgTrackUi links
struct hash *hubIdsToUrls = NULL;

static int gCmpGroup(const void *va, const void *vb)
/* Compare groups based on label. */
{
const struct group *a = *((struct group **)va);
const struct group *b = *((struct group **)vb);
return strcmp(a->label, b->label);
}

// Would like to do a radio button choice ofsorts
enum sortBy
    {
    sbRelevance=0,
    sbAbc      =1,
    sbHierarchy=2,
    };
static int gCmpTrackHierarchy(const void *va, const void *vb)
/* Compare tracks based on longLabel. */
{
const struct slRef *aa = *((struct slRef **)va);
const struct slRef *bb = *((struct slRef **)vb);
const struct track *a = ((struct track *) aa->val);
const struct track *b = ((struct track *) bb->val);
if ( tdbIsFolder(a->tdb) && !tdbIsFolder(b->tdb))
    return -1;
else if (!tdbIsFolder(a->tdb) &&  tdbIsFolder(b->tdb))
    return 1;
if ( tdbIsContainer(a->tdb) && !tdbIsContainer(b->tdb))
    return -1;
else if (!tdbIsContainer(a->tdb) &&  tdbIsContainer(b->tdb))
    return 1;
if (!tdbIsContainerChild(a->tdb) &&  tdbIsContainerChild(b->tdb))
    return -1;
else if ( tdbIsContainerChild(a->tdb) && !tdbIsContainerChild(b->tdb))
    return 1;
return strcasecmp(a->longLabel, b->longLabel);
}

static int gCmpTrack(const void *va, const void *vb)
/* Compare tracks based on longLabel. */
{
const struct slRef *aa = *((struct slRef **)va);
const struct slRef *bb = *((struct slRef **)vb);
const struct track *a = ((struct track *) aa->val);
const struct track *b = ((struct track *) bb->val);
return strcasecmp(a->longLabel, b->longLabel);
}

static void findTracksSort(struct slRef **pTrack, enum sortBy sortBy)
{
if (sortBy == sbHierarchy)
    slSort(pTrack, gCmpTrackHierarchy);
else if (sortBy == sbAbc)
    slSort(pTrack, gCmpTrack);
else
    slReverse(pTrack);
}

static int getFormatTypes(char ***pLabels, char ***pTypes)
{
char *crudeTypes[] = {
    ANYLABEL,
    "bam",
    "psl",
    "chain",
    "netAlign",
    "maf",
    "bed",
    "bigBed",
    "ctgPos",
    "expRatio",
    "genePred",
    "broadPeak",
    "narrowPeak",
    "rmsk",
    "bedGraph",
    "bigWig",
    "wig",
    "wigMaf"
    };
// Non-standard:
// type altGraphX
// type axt
// type bed5FloatScore
// type bed5FloatScoreWithFdr
// type chromGraph
// type clonePos
// type coloredExon
// type encodeFiveC
// type factorSource
// type ld2
// type logo
// type maf
// type sample
// type wigMafProt 0.0 1.0

char *nicerTypes[] = {
    ANYLABEL,
    "Alignment binary (bam) - binary SAM",
    "Alignment Blast (psl) - Blast output",
    "Alignment Chains (chain) - Pairwise alignment",
    "Alignment Nets (netAlign) - Net alignments",
    "Alignments (maf) - multiple alignment format",
    "bed - browser extensible data",
    "bigBed - self index, often remote bed format",
    "ctgPos - Contigs",
    "expRatio - Expression ratios",
    "Genes (genePred) - Gene prediction and annotation",
    "Peaks Broad (broadPeak) - ENCODE large region peak format",
    "Peaks Narrow (narrowPeak) - ENCODE small region peak format",
    "Repeats (rmsk) - Repeat masking",
    "Signal (bedGraph) - graphically represented bed data",
    "Signal (bigWig) - self index, often remote wiggle format",
    "Signal (wig) - wiggle format",
    "Signal (wigMaf) - multiple alignment wiggle"
    };

int ix = 0, count = sizeof(crudeTypes)/sizeof(char *);
char **labels;
char **values;
AllocArray(labels, count);
AllocArray(values, count);
for (ix=0;ix<count;ix++)
    {
    labels[ix] = cloneString(nicerTypes[ix]);
    values[ix] = cloneString(crudeTypes[ix]);
    }
*pLabels = labels;
*pTypes = values;
return count;
}

static struct trackDb *getSuperTrackTdbs(struct trackDb *tdbList)
/* Supertracks are not in the main trackDbList returned by hubAddTracks, find them
 * here since our search hits may hit them */
{
struct hash *superTrackHash = hashNew(0);
struct trackDb *tdb, *ret = NULL;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    if (tdbIsSuperTrackChild(tdb) && hashFindVal(superTrackHash, tdb->parent->track) == NULL)
        {
        hashAdd(superTrackHash, tdb->parent->track, tdb->parent);
        }
    }
struct hashEl *hel, *helList = hashElListHash(superTrackHash);
for (hel = helList; hel != NULL; hel = hel->next)
    slAddHead(&ret, (struct trackDb *)hel->val);
return ret;
}

static void hubTdbListToTrackList(struct trackDb *tdbList, struct track **trackList,
                                struct slName *trackNames)
/* Recursively convert a (struct trackDb *) to a (struct track *) */
{
struct trackDb *tmp, *next;
for (tmp = tdbList; tmp != NULL; tmp = next)
    {
    next = tmp->next;
    if (slNameInList(trackNames, tmp->track))
        {
        struct track *t = trackFromTrackDb(tmp);
        slAddHead(trackList, t);
        }
    if (tmp->subtracks)
        hubTdbListToTrackList(tmp->subtracks, trackList, trackNames);
    }
}

static void hubTdbListAddSupers(struct trackDb *tdbList, struct track **trackList,
                                struct slName *trackNames)
/* a track we are looking for might be a super track and thus not in tdbList, look for it here */
{
struct trackDb *tmp, *superTrackDbs = getSuperTrackTdbs(tdbList);
for (tmp = superTrackDbs; tmp != NULL; tmp = tmp->next)
    {
    if (slNameInList(trackNames, tmp->track))
        {
        struct track *tg = trackFromTrackDb(tmp);
        slAddHead(trackList, tg);
        }
    }
}

struct hubSearchTracks
/* A helper struct for collapsing a (struct hubSearchText *) into just the parts
 * we need for looking up the track hits */
    {
    struct hubSearchTracks *next;
    char *hubUrl; // the url to this hub which is used as a key into the search hash
    int hubId;
    struct hubConnectStatus *hub; // the hubStatus result
    struct slName *searchedTracks; // the track names the search terms matched against
    };

struct paraFetchData
/* A helper struct for managing connecting to many hubs in parallel  and adding the
 * relevant tracks to the global (struct slRef *)tracks struct. */
    {
    struct paraFetchData *next;
    char *hubName; // the name of the hub for measureTiming results
    struct hubSearchTracks *hst; // the tracks we are adding to the search results
    struct track *tlist; // the resulting tracks to add to the global trackList
    long searchTime; // how many milliseconds did it take to search this hub
    boolean done;
    };

// helper variables for connecting to hubs in parallel
pthread_mutex_t pfdMutex = PTHREAD_MUTEX_INITIALIZER;
struct paraFetchData *pfdListInitial = NULL;
struct paraFetchData *pfdList = NULL;
struct paraFetchData *pfdRunning = NULL;
struct paraFetchData *pfdDone = NULL;

void *addUnconnectedHubSearchResults(void *x)
/* Add a not yet connected hub to the search results */
{
struct paraFetchData *pfd = NULL;
boolean allDone = FALSE;
while(1)
    {
    pthread_mutex_lock(&pfdMutex);
    // the wait function will set pfdList = NULL, so don't start up any more
    // stuff if that happens:
    if (!pfdList)
        allDone = TRUE;
    else
        {
        pfd = slPopHead(&pfdList);
        slAddHead(&pfdRunning, pfd);
        }
    pthread_mutex_unlock(&pfdMutex);
    if (allDone)
        return NULL;
    struct hubSearchTracks *hst = pfd->hst;
    struct track *tracksToAdd = NULL;
    long startTime = clock1000();
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
        pfd->done = FALSE;
        boolean foundFirstGenome = FALSE;
        struct hash *trackDbNameHash = newHash(5);
        struct trackDb *tdbList = hubAddTracks(hst->hub, database, &foundFirstGenome, trackDbNameHash, NULL);
        if (measureTiming)
            measureTime("After connecting to hub %s: '%d': ", hst->hubUrl, hst->hubId);
        // get composite and subtracks into trackList
        hubTdbListToTrackList(tdbList, &tracksToAdd, hst->searchedTracks);
        hubTdbListAddSupers(tdbList, &tracksToAdd, hst->searchedTracks);
        pfd->done = TRUE;
        pfd->tlist = tracksToAdd;
        pfd->searchTime = clock1000() - startTime;;
        }
    errCatchEnd(errCatch);
    pthread_mutex_lock(&pfdMutex);
    slRemoveEl(&pfdRunning, pfd);
    slAddHead(&pfdDone, pfd);
    if (measureTiming)
        measureTime("Finished finding tracks for hub '%s': ", pfd->hubName);
    pthread_mutex_unlock(&pfdMutex);
    }
//always return NULL for pthread_create()
return NULL;
}

static void hubSearchHashToPfdList(char *descSearch, struct hash *searchResultsHash,
                                    struct hash *hubLookup, struct sqlConnection *conn)
/* getHubSearchResults() returned a hash of search hits to various hubs, convert that
 * into something we can work on in parallel */
{
struct hubSearchTracks *ret = NULL;
struct hashCookie cookie = hashFirst(searchResultsHash);
struct hash *hubUrlsToTrackList = hashNew(0);
struct hashEl *hel = NULL;
struct dyString *trackName = dyStringNew(0);
struct slName *connectedHubs = hubConnectHubsInCart(cart);
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct hubSearchText *hst = (struct hubSearchText *)hel->val;
    struct hubEntry *hubInfo = (struct hubEntry *) hashFindVal(hubLookup, hst->hubUrl);
    if (isNotEmpty(hubInfo->errorMessage))
        continue;
    // if we were already connected to this hub, then it's search hits
    // were already taken care of by the regular search code
    char hubId[256];
    safef(hubId, sizeof(hubId), "%d", hubInfo->id);
    if (slNameInList(connectedHubs, hubId))
        continue;
    struct hubConnectStatus *hub = hubConnectStatusForId(conn, hubInfo->id);
    // the hubSearchText contains multiple entries per lookup in order to form
    // the hgHubConnect UI. We only need one type of hit here:
    struct hubSearchTracks *found = hashFindVal(hubUrlsToTrackList, hst->hubUrl);
    for (; hst != NULL; hst = hst->next)
        {
        // don't add results for matches to the hub descriptionUrl, only to track names/descs
        if (isNotEmpty(hst->track) && hst->textLength != hubSearchTextMeta)
            {
            // hst->textLength=hubSearchTextLong denotes hits to track description
            if (isNotEmpty(descSearch) && hst->textLength != hubSearchTextLong)
                continue;
            if (!found)
                {
                AllocVar(found);
                found->hubUrl = hst->hubUrl;
                found->searchedTracks = NULL;
                found->hub = hub;
                found->hubId = hubInfo->id;
                slAddHead(&ret, found);
                hashAdd(hubUrlsToTrackList, hst->hubUrl, found);
                hashAdd(hubIdsToUrls, hubId, hst->hubUrl);
                }
            dyStringPrintf(trackName, "%s%d_%s", hubTrackPrefix, hubInfo->id, hst->track);
            slNameStore(&found->searchedTracks, cloneString(trackName->string));
            dyStringClear(trackName);
            }
        }
    }
struct hubSearchTracks *t;
for (t = ret; t != NULL; t = t->next)
    {
    struct paraFetchData *pfd;
    AllocVar(pfd);
    pfd->hubName = t->hubUrl;
    pfd->hst = t;
    slAddHead(&pfdList, pfd);
    slAddHead(&pfdListInitial, CloneVar(pfd));
    }

if (measureTiming)
    measureTime("Finished converting hubSearchText to hubSearchTracks and pfd");
}

void waitForSearchResults(int numThreads, pthread_t *threadList)
/* Run each thread and kill the ones that take too long */
{
// only wait 5 seconds, if something is in the cache we can show it
// otherwise just ignore, nobody should wait more than 5 seconds
// for a simple track search. Although note that just getting to
// this point can take quite a while depending on hgcentral
// connections and obtaining trackDb.
int maxTime = 5 * 1000;
int waitTime = 0;
int lockStatus = 0;
struct paraFetchData *pfd;
while(1)
    {
    sleep1000(50);
    waitTime += 50;
    boolean allDone = TRUE;
    // we don't want to block in the event one of the child threads is
    // taking forever
    lockStatus = pthread_mutex_trylock(&pfdMutex);
    if (pfdList || pfdRunning)
        allDone = FALSE;
    if (allDone)
        {
        if (lockStatus == 0) // we aquired the lock
            pthread_mutex_unlock(&pfdMutex);
        break;
        }
    if (waitTime >= maxTime)
        {
        if (lockStatus == 0) // we aquired the lock
            pthread_mutex_unlock(&pfdMutex);
        break;
        }
    if (lockStatus == 0) // release the lock if we got it
        pthread_mutex_unlock(&pfdMutex);
    }

// now that we've waited the maximum time we need to kill
// any running threads and add the results of any threads
// that ran successfully
lockStatus = pthread_mutex_trylock(&pfdMutex);
struct paraFetchData *neverRan = pfdList;
if (lockStatus == 0)
    {
    // prevent still running threads from continuing
    pfdList = NULL;
    if (measureTiming)
        fprintf(stdout, "<span class='timing'>Successfully aquired lock, adding any succesful thread data\n<br></span>");
    for (pfd = pfdDone; pfd != NULL; pfd = pfd->next)
        {
        struct track *t;
        for (t = pfd->tlist; t != NULL; t = t->next)
            refAdd(&tracks, t);
        if (measureTiming)
            measureTime("'%s' search times", pfd->hubName);
        }
    for (pfd = neverRan; pfd != NULL; pfd = pfd->next)
        if (measureTiming)
            measureTime("'%s' search times: never ran", pfd->hubName);
    }
else
    {
    // Should we warn or something that results are still waiting? As of now
    // just silently return instead, and note that no unconnected hub data
    // will show up (we get connected hub results for free because of
    // trackDbCaching)
    if (measureTiming)
        measureTime("Timed out searching hubs");
    }
if (lockStatus == 0)
    pthread_mutex_unlock(&pfdMutex);
}

void addHubSearchResults(struct slName *nameList, char *descSearch)
/* add public hubs to the track list */
{
struct sqlConnection *conn = hConnectCentral();
char *hubSearchTableName = hubSearchTextTableName();
char *publicTable = cfgOptionEnvDefault("HGDB_HUB_PUBLIC_TABLE",
    hubPublicTableConfVariable, defaultHubPublicTableName);
char *statusTable = cfgOptionEnvDefault("HGDB_HUB_STATUS_TABLE",
    hubStatusTableConfVariable, defaultHubStatusTableName);
struct dyString *extra = dyStringNew(0);
if (nameList)
    {
    struct slName *tmp = NULL;
    for (tmp = nameList; tmp != NULL; tmp = tmp->next)
        {
        sqlDyStringPrintf(extra, "label like '%%%s%%'", tmp->name);
        if (tmp->next)
            sqlDyStringPrintf(extra, " and ");
        }
    }

if (sqlTableExists(conn, hubSearchTableName))
    {
    struct hash *searchResultsHash = hashNew(0);
    struct hash *pHash = hashNew(0);
    struct slName *hubsToPrint = NULL;
    addPublicHubsToHubStatus(cart, conn, publicTable, statusTable);
    struct hash *hubLookup = buildPublicLookupHash(conn, publicTable, statusTable, &pHash);
    char *db = cloneString(trackHubSkipHubName(database));
    tolowers(db);
    getHubSearchResults(conn, hubSearchTableName, descSearch, isNotEmpty(descSearch), db, hubLookup, &searchResultsHash, &hubsToPrint, dyStringCannibalize(&extra));
    hubSearchHashToPfdList(descSearch, searchResultsHash, hubLookup, conn);
    if (measureTiming)
        measureTime("after querying hubSearchText table and ready to start threads");
    int ptMax = atoi(cfgOptionDefault("parallelFetch.threads", "20"));
    int pfdListCount = 0, pt;
    if (ptMax > 0)
        {
        pfdListCount = slCount(pfdList);
        pthread_t *threads = NULL;
        ptMax = min(ptMax, pfdListCount);
        if (ptMax > 0)
            {
            AllocArray(threads, ptMax);
            for (pt = 0; pt < ptMax; pt++)
                {
                int rc = pthread_create(&threads[pt], NULL, addUnconnectedHubSearchResults, NULL);
                if (rc )
                    errAbort("Unexpected error in pthread_create");
                pthread_detach(threads[pt]);
                // this thread will just happily keep working until waitForSearchResults() finishes,
                // moving it's completed work onto pfdDone, so we can safely detach
                }
            }
        waitForSearchResults(ptMax, threads);
        }
    }
if (measureTiming)
    measureTime("Total time spent searching hubs");
}

static void simpleSearchForTracks(char *simpleEntry)
// Performs the simple search and returns the found tracks.
{
// Prepare for trix search
if (!isEmpty(simpleEntry))
    {
    int trixWordCount = 0;
    char *tmp = cloneString(simpleEntry);
    char *val = nextWord(&tmp);
    struct slName *el, *trixList = NULL;
    while (val != NULL)
        {
        slNameAddTail(&trixList, val);
        trixWordCount++;
        val = nextWord(&tmp);
        }
    if (trixWordCount > 0 && !isHubTrack(database))
        {
        // Unfortunately trixSearch can't handle the slName list
        int i;
        char **trixWords = needMem(sizeof(char *) * trixWordCount);
        for (i = 0, el = trixList; el != NULL; i++, el = el->next)
            trixWords[i] = strLower(el->name);

        // Now open the trix file
        char trixFile[HDB_MAX_PATH_STRING];
        getSearchTrixFile(database, trixFile, sizeof(trixFile));
        struct trix *trix = trixOpen(trixFile);

        struct trixSearchResult *tsList = trixSearch(trix, trixWordCount, trixWords, tsmExpand);
        for ( ; tsList != NULL; tsList = tsList->next)
            {
            struct track *track = (struct track *) hashFindVal(trackHash, tsList->itemId);
            if (track != NULL)  // It is expected that this is NULL
                {               // (e.g. when trix references trackDb tracks which have no tables)
                refAdd(&tracks, track);
                }
            }
        //trixClose(trix);  // don't bother (this is a CGI that is about to end)
        }
    }
}

static void advancedSearchForTracks(struct sqlConnection *conn,struct group *groupList,
                                             char *nameSearch, char *typeSearch, char *descSearch,
                                             char *groupSearch, struct slPair *mdbPairs,
                                             boolean includeHubResults)
// Performs the advanced search and returns the found tracks.
{
int tracksFound = 0;
int numMetadataNonEmpty = 0;
struct slPair *pair = mdbPairs;
for (; pair!= NULL;pair=pair->next)
    {
    if (!isEmpty((char *)(pair->val)))
        numMetadataNonEmpty++;
    }

if (!isEmpty(groupSearch) && sameString(groupSearch,ANYLABEL))
    groupSearch = NULL;
if (!isEmpty(typeSearch) && sameString(typeSearch,ANYLABEL))
    typeSearch = NULL;

if (isEmpty(nameSearch) && isEmpty(typeSearch) && isEmpty(descSearch)
&& isEmpty(groupSearch) && numMetadataNonEmpty == 0)
    return;

// First do the metaDb searches, which can be done quickly for all tracks with db queries.
struct hash *matchingTracks = NULL;

if (numMetadataNonEmpty)
    {
    struct mdbObj *mdbObj, *mdbObjs = mdbObjRepeatedSearch(conn,mdbPairs,TRUE,FALSE);
    if (mdbObjs)
        {
        for (mdbObj = mdbObjs; mdbObj != NULL; mdbObj = mdbObj->next)
            {
            if (matchingTracks == NULL)
                matchingTracks = newHash(0);
            hashAddInt(matchingTracks, mdbObj->obj, 1);
            }
        mdbObjsFree(&mdbObjs);
        }
    if (matchingTracks == NULL)
        return;
    }

// Set the word lists up once
struct slName *nameList = NULL;
if (!isEmpty(nameSearch))
    nameList = slNameListOfUniqueWords(cloneString(nameSearch),TRUE); // TRUE means respect quotes
struct slName *descList = NULL;
if (!isEmpty(descSearch))
    descList = slNameListOfUniqueWords(cloneString(descSearch),TRUE);

struct group *group;
for (group = groupList; group != NULL; group = group->next)
    {
    if (isEmpty(groupSearch) || sameString(group->name, groupSearch))
        {
        if (group->trackList == NULL)
            continue;

        struct trackRef *tr;
        for (tr = group->trackList; tr != NULL; tr = tr->next)
            {
            struct track *track = tr->track;
            char *trackType = cloneFirstWord(track->tdb->type); // will be spilled
            if ((matchingTracks == NULL || hashLookup(matchingTracks, track->track) != NULL)
            && (  isEmpty(typeSearch)
               || (sameWord(typeSearch, trackType) && !tdbIsComposite(track->tdb)))
            && (isEmpty(nameSearch) || searchNameMatches(track->tdb, nameList))
            && (isEmpty(descSearch) || searchDescriptionMatches(track->tdb, descList)))
                {
                if (track != NULL)
                    {
                    tracksFound++;
                    refAdd(&tracks, track);
                    }
                else
                    warn("found group track is NULL.");
                }
            if (track->subtracks != NULL)
                {
                struct track *subTrack;
                for (subTrack = track->subtracks; subTrack != NULL; subTrack = subTrack->next)
                    {
                    trackType = cloneFirstWord(subTrack->tdb->type); // will be spilled
                    if (  (matchingTracks == NULL
                       || hashLookup(matchingTracks, subTrack->track) != NULL)
                    && (isEmpty(typeSearch) || sameWord(typeSearch, trackType))
                    && (isEmpty(nameSearch) || searchNameMatches(subTrack->tdb, nameList))
                    && (isEmpty(descSearch) // subtracks inherit description
                        || searchDescriptionMatches(subTrack->tdb, descList)
                        || (tdbIsCompositeChild(subTrack->tdb) && subTrack->parent
                            && searchDescriptionMatches(subTrack->parent->tdb, descList))))
                        {
                        if (track != NULL)
                            {
                            tracksFound++;
                            refAdd(&tracks, subTrack);
                            }
                        else
                            warn("found subtrack is NULL.");
                        }
                    }
                }
            }
        }
    }
if (measureTiming)
    measureTime("searched native tracks: ");
if (includeHubResults)
    addHubSearchResults(nameList,descSearch);
}

#define MAX_FOUND_TRACKS 100
static void findTracksPageLinks(int tracksFound, int startFrom, int instance)
{
char id[256];
if (tracksFound <= MAX_FOUND_TRACKS)
    return;

// Opener
int willStartAt = 0;
int curPage  = (startFrom/MAX_FOUND_TRACKS) + 1;
int endAt = startFrom+MAX_FOUND_TRACKS;
if (endAt > tracksFound)
    endAt = tracksFound;
hPrintf("<span><em style='font-size:.9em;'>Listing %d - %d of %d tracks</em>&nbsp;&nbsp;&nbsp;",
        startFrom+1,endAt,tracksFound);

// << and <
if (startFrom >= MAX_FOUND_TRACKS)
    {
    safef(id, sizeof id, "ftpl%d-first", instance);
    hPrintf("<a href='../cgi-bin/hgTracks?%s=Search&%s=0' id='%s' title='First page of found tracks'"
	    ">&#171;</a>&nbsp;",
            TRACK_SEARCH,TRACK_SEARCH_PAGER,id);
    jsOnEventByIdF("click", id, "return findTracks.page(\"%s\",0);", TRACK_SEARCH_PAGER);

    safef(id, sizeof id, "ftpl%d-prev", instance);
    willStartAt = startFrom - MAX_FOUND_TRACKS;
    hPrintf("&nbsp;<a href='../cgi-bin/hgTracks?%s=Search&%s=%d' id='%s' "
	"title='Previous page of found tracks'>&#139;</a>&nbsp;",
            TRACK_SEARCH,TRACK_SEARCH_PAGER,willStartAt,id);
    jsOnEventByIdF("click", id, "return findTracks.page(\"%s\",%d);", TRACK_SEARCH_PAGER,willStartAt);
    }

// page number links
int lastPage = (tracksFound/MAX_FOUND_TRACKS);
if ((tracksFound % MAX_FOUND_TRACKS) > 0)
    lastPage++;

int thisPage = curPage - 3; // Window of 3 pages above and below
if (thisPage < 1)
    thisPage = 1;
for (;thisPage <= lastPage && thisPage <= curPage + 3; thisPage++)
    {
    safef(id, sizeof id, "ftpl%d-%d", instance, thisPage);
    if (thisPage != curPage)
        {
        willStartAt = ((thisPage - 1) * MAX_FOUND_TRACKS);
        endAt = willStartAt+ MAX_FOUND_TRACKS;
        if (endAt > tracksFound)
            endAt = tracksFound;
        hPrintf("&nbsp;<a href='../cgi-bin/hgTracks?%s=Search&%s=%d' id='%s' "
		"title='Page %d (%d - %d) tracks'>%d</a>&nbsp;",
                TRACK_SEARCH,TRACK_SEARCH_PAGER,willStartAt,id,thisPage,willStartAt+1,endAt,thisPage);
	jsOnEventByIdF("click", id, "return findTracks.page(\"%s\",%d);",TRACK_SEARCH_PAGER,willStartAt);
        }
    else
        hPrintf("&nbsp;<em style='color:%s;'>%d</em>&nbsp;",COLOR_DARKGREY,thisPage);
    }

// > and >>
if ((startFrom + MAX_FOUND_TRACKS) < tracksFound)
    {
    safef(id, sizeof id, "ftpl%d-next", instance);
    willStartAt = startFrom + MAX_FOUND_TRACKS;
    hPrintf("&nbsp;<a href='../cgi-bin/hgTracks?%s=Search&%s=%d' id='%s' "
	"title='Next page of found tracks'>&#155;</a>&nbsp;",
	TRACK_SEARCH,TRACK_SEARCH_PAGER,willStartAt,id);
    jsOnEventByIdF("click", id, "return findTracks.page(\"%s\",%d);",TRACK_SEARCH_PAGER,willStartAt);
	    
    safef(id, sizeof id, "ftpl%d-last", instance);
    willStartAt =  tracksFound - (tracksFound % MAX_FOUND_TRACKS);
    if (willStartAt == tracksFound)
        willStartAt -= MAX_FOUND_TRACKS;
    hPrintf("&nbsp;<a href='../cgi-bin/hgTracks?%s=Search&%s=%d' id='%s' title='Last page of found tracks' "
	    ">&#187;</a></span>\n",
            TRACK_SEARCH,TRACK_SEARCH_PAGER,willStartAt,id);
    jsOnEventByIdF("click", id, "return findTracks.page(\"%s\",%d);",TRACK_SEARCH_PAGER,willStartAt);
    }
}

static void displayFoundTracks(struct cart *cart, struct slRef *tracks, int tracksFound,
                               enum sortBy sortBy)
// Routine for displaying found tracks
{
char id[256];
char javascript[1024];
hPrintf("<div id='found' style='display:none;'>\n"); // This div is emptied with 'clear' button
if (tracksFound < 1)
    {
    hPrintf("<p>No tracks found</p>\n");
    }
else
    {
    hPrintf("<form action='%s' name='%s' id='%s' method='post'>\n\n",
            hgTracksName(),SEARCH_RESULTS_FORM,SEARCH_RESULTS_FORM);
    cartSaveSession(cart);  // Creates hidden var of hgsid to avoid bad voodoo

    int startFrom = 0;
    hPrintf("<table id='foundTracks'>\n");

    // Opening view in browser button and foundTracks count
    #define ENOUGH_FOUND_TRACKS 10
    if (tracksFound >= ENOUGH_FOUND_TRACKS)
        {
        hPrintf("<tr><td nowrap colspan=3>\n");
        hPrintf("<INPUT TYPE=SUBMIT NAME='submit' VALUE='return to browser' class='viewBtn' "
                "style='font-size:.8em;'>");
        hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<span class='selCbCount'></span>\n");

        startFrom = cartUsualInt(cart,TRACK_SEARCH_PAGER,0);
        if (startFrom > 0 && startFrom < tracksFound)
            {
            int countUp = 0;
            for (countUp=0; countUp < startFrom;countUp++)
                {
                if (slPopHead(&tracks) == NULL) // memory waste
                    break;
                }
            }
        hPrintf("</td><td align='right' valign='bottom'>\n");
        findTracksPageLinks(tracksFound,startFrom,0);
        hPrintf("</td></tr>\n");
        }

    // Begin foundTracks table
    //hPrintf("<table id='foundTracks'><tr><td colspan='2'>\n");
    hPrintf("<tr><td colspan='2'>\n");
    hPrintf("</td><td align='right'>\n");
    hPrintf("</td></tr><tr bgcolor='#%s'><td>",HG_COL_HEADER);
    #define PM_BUTTON \
            "<IMG height=18 width=18 " \
            "id='btn_%s' src='../images/%s' title='%s all found tracks'>"
    hPrintf(PM_BUTTON,"plus_all",   "add_sm.gif",  "Select");
    hPrintf(PM_BUTTON,"minus_all","remove_sm.gif","Unselect");
    jsOnEventById("click", "btn_plus_all", "return findTracks.checkAllWithWait(true);");  
    jsOnEventById("click", "btn_minus_all", "return findTracks.checkAllWithWait(false);");  
    hPrintf("</td><td><b>Visibility</b></td><td colspan=2>&nbsp;&nbsp;<b>Track Name</b>\n");

    // Sort options?
    if (tracksFound >= ENOUGH_FOUND_TRACKS)
        {
        hPrintf("<span style='float:right;'>Sort:");
        cgiMakeOnEventRadioButtonWithClass(TRACK_SEARCH_SORT, "0", (sortBy == sbRelevance), 
	    NULL,"click", "findTracks.sortNow(this);");
        hPrintf("by Relevance");
        cgiMakeOnEventRadioButtonWithClass(TRACK_SEARCH_SORT, "1", (sortBy == sbAbc), 
	    NULL,"click", "findTracks.sortNow(this);");
        hPrintf("Alphabetically");
        cgiMakeOnEventRadioButtonWithClass(TRACK_SEARCH_SORT, "2", (sortBy == sbHierarchy), 
	    NULL,"click", "findTracks.sortNow(this);");
        hPrintf("by Hierarchy&nbsp;&nbsp;</span>\n");
        }
    hPrintf("</td></tr>\n");

    // Set up json for js functionality
    struct jsonElement *jsonTdbVars = newJsonObject(newHash(8));

    int trackCount=0;
    boolean containerTrackCount = 0;
    struct slRef *ptr;
    struct slName *connectedHubIds = hubConnectHubsInCart(cart);
    while((ptr = slPopHead(&tracks)))
        {
        if (++trackCount > MAX_FOUND_TRACKS)
            break;

        struct track *track = (struct track *) ptr->val;
        jsonTdbSettingsBuild(jsonTdbVars, track, FALSE); // FALSE: No config from track search
        char *hubId = NULL;
        if (isHubTrack(track->track))
            {
            char *trackNameCopy = cloneString(track->track);
            hubId = strchr(trackNameCopy, '_');
            hubId += 1;
            char *ptr2 = strchr(hubId, '_');
            if (ptr2 == NULL)
                errAbort("hub track '%s' not in correct format", track->track);
            *ptr2 = '\0';
            char *hubUrl = hashFindVal(hubIdsToUrls, hubId);
            if (hubUrl != NULL)
                {
                struct jsonElement *ele = jsonFindNamedField(jsonTdbVars, "", track->track);
                jsonObjectAdd(ele, "hubUrl", newJsonString(hubUrl));
                }
            }

        if (tdbIsFolder(track->tdb)) // supertrack
            hPrintf("<tr class='bgLevel4' valign='top' class='found'>\n");
        else if (tdbIsContainer(track->tdb))
            hPrintf("<tr class='bgLevel3' valign='top' class='found'>\n");
        else
            hPrintf("<tr class='bgLevel2' valign='top' class='found'>\n");

        hPrintf("<td align='center'>\n");

        // Determine visibility and checked state
        track->visibility = tdbVisLimitedByAncestors(cart, track->tdb, TRUE, TRUE);
        boolean checked = ( track->visibility != tvHide );
        if (tdbIsContainerChild(track->tdb))
            {
            // Don't need all 4 states here.  Visible=checked&&enabled
            checked = fourStateVisible(subtrackFourStateChecked(track->tdb,cart));
            // Checked is only if subtrack level vis is also set!
            checked = (checked && ( track->visibility != tvHide ));
            }
        // if we haven't already connected to this hub, then by default
        // we need to unselect every checkbox
        if (isHubTrack(track->track) && !slNameInList(connectedHubIds, hubId))
            checked = FALSE;

        // Setup the check box
        #define CB_HIDDEN_VAR "<INPUT TYPE=HIDDEN disabled=true NAME='%s_sel' VALUE='%s'>"
        // subtracks and folder children get "_sel" var. ("_sel" var is temp on folder children)
        if (tdbIsContainerChild(track->tdb) || tdbIsFolderContent(track->tdb))
            hPrintf(CB_HIDDEN_VAR,track->track,checked?"1":CART_VAR_EMPTY);
        #define CB_SEEN "<INPUT TYPE=CHECKBOX id='%s_sel_id' VALUE='on' class='selCb' %s>"
        hPrintf(CB_SEEN,track->track,(checked ? " CHECKED" : ""));
	safef(id, sizeof id, "%s_sel_id", track->track); // XSS Filter?
	jsOnEventById("click", id, "findTracks.clickedOne(this,true);");  
        hPrintf("</td><td>\n");

        // Setup the visibility drop down
        #define VIS_HIDDEN_VAR "<INPUT TYPE=HIDDEN disabled=true NAME='%s' VALUE='%s'>"
        hPrintf(VIS_HIDDEN_VAR,track->track,CART_VAR_EMPTY); // All tracks get vis hidden var
	
	safef(id, sizeof id, "%s_id", track->track); // XSS Filter?
	safef(javascript, sizeof javascript, "findTracks.changeVis(this);");
	struct slPair *event = slPairNew("change", cloneString(javascript));
        if (tdbIsFolder(track->tdb))
            {
            hideShowDropDownWithClassAndExtra(track->track, id, (track->visibility != tvHide),
                                              "normalText visDD", event);
            }
        else
            {
            hTvDropDownClassWithJavascript(NULL, id, track->visibility,track->canPack,
                                           "normalText seenVis",event);
            }

        // If this is a container track, allow configuring...
        if (tdbIsContainer(track->tdb) || tdbIsFolder(track->tdb))
            {
            containerTrackCount++; // Using onclick ensures return to search tracks on submit
            hPrintf("&nbsp;<IMG SRC='../images/folderWrench.png' style='cursor:pointer;' "
                    "id='%s_confSet' title='Configure this track container...' "
                    ">&nbsp;", track->track);
            safef(id, sizeof id, "%s_confSet", track->track); // XSS Filter?
            char hubConfigUrl[4096];
            safef(hubConfigUrl, sizeof(hubConfigUrl), "%s", track->track);
            if (isHubTrack(track->track))
                {
                char *hubUrl = hashFindVal(hubIdsToUrls, hubId);
                if (hubUrl != NULL)
                    safefcat(hubConfigUrl, sizeof(hubConfigUrl), "&hubUrl=%s", hubUrl);
                }
            jsOnEventByIdF("click", id, "findTracks.configSet(\"%s\");", hubConfigUrl);
            }
//#define SHOW_PARENT_FOLDER
#ifdef SHOW_PARENT_FOLDER
        else if (tdbIsContainerChild(track->tdb) || tdbIsFolderContent(track->tdb))
            {
            struct trackDb *parentTdb =
                            tdbIsContainerChild(track->tdb) ? tdbGetContainer(track->tdb)
                                                            : tdbGetImmediateFolder(track->tdb);
            if (parentTdb != NULL) // Using href will not return to search tracks on submit
                hPrintf("&nbsp;<A HREF='../cgi-bin/hgTrackUi?g=%s'><IMG SRC='../images/folderC.png'"
                        " title='Navigate to parent container...'></A>&nbsp;", parentTdb->track);
            }
#endif///def SHOW_PARENT_FOLDER
        hPrintf("</td>\n");

        // shortLabel has description popup and longLabel has "..." metadata
        char hgTrackUiUrl[4096];
        safef(hgTrackUiUrl, sizeof(hgTrackUiUrl), "%s", trackUrl(track->track, NULL));
        if (isHubTrack(track->track))
            {
            char *hubUrl = hashFindVal(hubIdsToUrls, hubId);
            if (hubUrl != NULL)
                safefcat(hgTrackUiUrl, sizeof(hgTrackUiUrl), "&hubUrl=%s", hubUrl);
            }
        hPrintf("<td><a target='_top' id='%s_dispFndTrk' "
                "href='%s' title='Display track details'>%s</a></td>\n",
                track->track, hgTrackUiUrl, track->shortLabel);
        safef(id, sizeof id, "%s_dispFndTrk", track->track);
        jsOnEventByIdF("click", id, "popUp.hgTrackUi('%s',true); return false;", track->track);
        hPrintf("<td>%s", track->longLabel);
        compositeMetadataToggle(database, track->tdb, NULL, TRUE, FALSE);
        hPrintf("</td></tr>\n");
        }
    //hPrintf("</table>\n");

    // Closing view in browser button and foundTracks count
    hPrintf("<tr><td nowrap colspan=3>");
    hPrintf("<INPUT TYPE=SUBMIT NAME='submit' VALUE='Return to Browser' class='viewBtn' "
            "style='font-size:.8em;'>");
    hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<span class='selCbCount'></span>");
    if (tracksFound >= ENOUGH_FOUND_TRACKS)
        {
        hPrintf("</td><td align='right' valign='top'>\n");
        findTracksPageLinks(tracksFound,startFrom,1);
        hPrintf("</td></tr>\n");
        }
    hPrintf("</table>\n");

    if (containerTrackCount > 0)
        hPrintf("<BR><IMG SRC='../images/folderWrench.png'>&nbsp;Tracks so marked are containers "
                "which group related data tracks.  Containers may need additional configuration "
                "(by clicking on the <IMG SRC='../images/folderWrench.png'> icon) before they can "
                "be viewed in the browser.<BR>\n");
        //hPrintf("* Tracks so marked are containers which group related data tracks.  These may "
        //        "not be visible unless further configuration is done.  Click on the * to "
        //        "configure these.<BR><BR>\n");
    hPrintf("\n</form>\n");

    // be done with json
    jsonTdbSettingsUse(jsonTdbVars);
    }
hPrintf("</div>"); // This div allows the clear button to empty it
}

void doSearchTracks(struct group *groupList)
{
webIncludeResourceFile("ui.dropdownchecklist.css");
jsIncludeFile("ui.dropdownchecklist.js",NULL);
// This line is needed to get the multi-selects initialized
jsIncludeFile("ddcl.js",NULL);

struct group *group;
char *groups[128];
char *labels[128];
int numGroups = 1;
groups[0] = ANYLABEL;
labels[0] = ANYLABEL;
char *nameSearch  = cartOptionalString(cart, TRACK_SEARCH_ON_NAME);
char *typeSearch  = cartUsualString(   cart, TRACK_SEARCH_ON_TYPE,ANYLABEL);
char *simpleEntry = cartOptionalString(cart, TRACK_SEARCH_SIMPLE);
char *descSearch  = cartOptionalString(cart, TRACK_SEARCH_ON_DESCR);
char *groupSearch = cartUsualString(  cart, TRACK_SEARCH_ON_GROUP,ANYLABEL);
boolean doSearch = sameString(cartOptionalString(cart, TRACK_SEARCH), "Search")
                   || cartUsualInt(cart, TRACK_SEARCH_PAGER, -1) >= 0;
boolean includeHubResults = cartUsualBoolean(cart, TRACK_SEARCH_ON_HUBS, TRUE);
struct sqlConnection *conn = NULL;
boolean metaDbExists = FALSE;
if (!isHubTrack(database))
    {
    conn = hAllocConn(database);
    metaDbExists = sqlTableExists(conn, "metaDb");
    }
int tracksFound = 0;
int cols;
char buf[512];

char *currentTab = cartUsualString(cart, TRACK_SEARCH_CURRENT_TAB, "simpleTab");
enum searchTab selectedTab = (sameString(currentTab, "advancedTab") ? advancedTab : simpleTab);

// NOTE: could support quotes in simple tab by detecting quotes and choosing
//       to use doesNameMatch() || doesDescriptionMatch()
if (selectedTab == simpleTab && !isEmpty(simpleEntry))
    stripChar(simpleEntry, '"');
trackList = getTrackList(&groupList, -2); // global
makeGlobalTrackHash(trackList);

// NOTE: This is necessary when container cfg by '*' results in vis changes
// This will handle composite/view override when subtrack specific vis exists,
// AND superTrack reshaping.

// Subtrack settings must be removed when composite/view settings are updated
parentChildCartCleanup(trackList,cart,oldVars);

slSort(&groupList, gCmpGroup);
struct hash *superHash = hashNew(8);
for (group = groupList; group != NULL; group = group->next)
    {
    groupTrackListAddSuper(cart, group, superHash);
    if (group->trackList != NULL)
        {
        groups[numGroups] = cloneString(group->name);
        labels[numGroups] = cloneString(group->label);
        numGroups++;
        if (numGroups >= ArraySize(groups))
            internalErr();
        }
    }
hashFree(&superHash);

safef(buf, sizeof(buf),"Search for Tracks in the %s %s Assembly",
      organism, hFreezeFromDb(database));
webStartWrapperDetailedNoArgs(cart, database, "", buf, FALSE, FALSE, FALSE, FALSE);

hPrintf("<div style='max-width:1080px;'>");
hPrintf("<form action='%s' name='%s' id='%s' method='get'>\n\n",
        hgTracksName(),TRACK_SEARCH_FORM,TRACK_SEARCH_FORM);
cartSaveSession(cart);  // Creates hidden var of hgsid to avoid bad voodoo
safef(buf, sizeof(buf), "%lu", clock1());
cgiMakeHiddenVar("hgt_", buf);  // timestamps page to avoid browser cache


hPrintf("<input type='hidden' name='db' value='%s'>\n", database);
hPrintf("<input type='hidden' name='%s' id='currentTab' value='%s'>\n",
        TRACK_SEARCH_CURRENT_TAB, currentTab);
hPrintf("<input type='hidden' name='%s' value=''>\n",TRACK_SEARCH_DEL_ROW);
hPrintf("<input type='hidden' name='%s' value=''>\n",TRACK_SEARCH_ADD_ROW);
hPrintf("<input type='hidden' name='%s' value=''>\n",TRACK_SEARCH_PAGER);

hPrintf("<div id='tabs' style='display:none; %s'>\n<ul>\n<li><a href='#simpleTab'>"
        "<B style='font-size:.9em;font-family: arial, Geneva, Helvetica, san-serif;'>Search</B>"
        "</a></li>\n<li><a href='#advancedTab'>"
        "<B style='font-size:.9em;font-family: arial, Geneva, Helvetica, san-serif;'>Advanced</B>"
        "</a></li>\n</ul>\n<div id='simpleTab' style='max-width:inherit;'>\n",
        cgiBrowser()==btIE?"width:1060px;":"max-width:inherit;");

hPrintf("<table id='simpleTable' style='width:100%%; font-size:.9em;'><tr><td colspan='2'>");
hPrintf("<input type='text' name='%s' id='simpleSearch' class='submitOnEnter' value='%s' "
        "style='max-width:1000px; width:100%%;'>\n",
        TRACK_SEARCH_SIMPLE,simpleEntry == NULL ? "" : simpleEntry);
jsOnEventById("keyup", "simpleSearch", "findTracks.searchButtonsEnable(true);");

hPrintf("</td></tr><td style='max-height:4px;'></td></tr></table>");
//hPrintf("</td></tr></table>");
hPrintf("<input type='submit' name='%s' id='searchSubmit' value='Search' "
        "style='font-size:.8em;'>\n", TRACK_SEARCH);
hPrintf("<input type='button'id='doSTClear1' name='clear' value='Clear' class='clear' "
        "style='font-size:.8em;'>\n");
jsOnEventById("click", "doSTClear1", "findTracks.clear();");
hPrintf("<input type='submit' name='submit' value='Cancel' class='cancel' "
        "style='font-size:.8em;'>\n");
hPrintf("</div>\n");

// Advanced tab
hPrintf("<div id='advancedTab' style='width:inherit;'>\n"
        "<table id='advancedTable' cellSpacing=0 style='width:inherit; font-size:.9em;'>\n");
cols = 8;

// Track Name contains
hPrintf("<tr><td colspan=3></td>");
hPrintf("<td nowrap><b style='max-width:100px;'>Track&nbsp;Name:</b></td>");
hPrintf("<td align='right'>contains</td>\n");
hPrintf("<td colspan='%d'>", cols - 4);
hPrintf("<input type='text' name='%s' id='nameSearch' class='submitOnEnter' value='%s' "
        "style='min-width:326px; font-size:.9em;'>",
        TRACK_SEARCH_ON_NAME, nameSearch == NULL ? "" : nameSearch);
jsOnEventById("keyup", "nameSearch", "findTracks.searchButtonsEnable(true);");
hPrintf("</td></tr>\n");

// Description contains
hPrintf("<tr><td colspan=2></td><td align='right'>and&nbsp;</td>");
hPrintf("<td><b style='max-width:100px;'>Description:</b></td>");
hPrintf("<td align='right'>contains</td>\n");
hPrintf("<td colspan='%d'>", cols - 4);
hPrintf("<input type='text' name='%s' id='descSearch' value='%s' class='submitOnEnter' "
        "style='max-width:536px; width:536px; font-size:.9em;'>",
        TRACK_SEARCH_ON_DESCR, descSearch == NULL ? "" : descSearch);
jsOnEventById("keyup", "descSearch", "findTracks.searchButtonsEnable(true);");
hPrintf("</td></tr>\n");

hPrintf("<tr><td colspan=2></td><td align='right'>and&nbsp;</td>\n");
hPrintf("<td><b style='max-width:100px;'>Group:</b></td>");
hPrintf("<td align='right'>is</td>\n");
hPrintf("<td colspan='%d'>", cols - 4);
cgiMakeDropListFullExt(TRACK_SEARCH_ON_GROUP, labels, groups, numGroups, groupSearch,
    NULL, NULL, "min-width:40%; font-size:.9em;", "groupSearch");
hPrintf("</td></tr>\n");

// Track Type is (drop down)
hPrintf("<tr><td colspan=2></td><td align='right'>and&nbsp;</td>\n");
hPrintf("<td nowrap><b style='max-width:100px;'>Data Format:</b></td>");
hPrintf("<td align='right'>is</td>\n");
hPrintf("<td colspan='%d'>", cols - 4);
char **formatTypes = NULL;
char **formatLabels = NULL;
int formatCount = getFormatTypes(&formatLabels, &formatTypes);
cgiMakeDropListFullExt(TRACK_SEARCH_ON_TYPE, formatLabels, formatTypes, formatCount, typeSearch,
    NULL, NULL, "'min-width:40%; font-size:.9em;", "typeSearch");
hPrintf("</td></tr>\n");

// include public hubs in output:
hPrintf("<tr><td colspan=2></td><td align='right'>and&nbsp;</td>\n");
hPrintf("<td nowrap><b style='max-width:100px;'>Include Public Hub Search Results:</b></td>");
hPrintf("<td></td>");
hPrintf("<td colspan='%d'>", cols - 4);
cgiMakeCheckBox(TRACK_SEARCH_ON_HUBS, includeHubResults);
hPrintf("NOTE: Including public hubs in results may be slow");
hPrintf("</td></tr>\n");

// mdb selects
struct slPair *mdbSelects = NULL;
if (metaDbExists)
    {
    struct slPair *mdbVars = mdbVarsSearchable(conn,TRUE,FALSE); // Tables but not file only objects
    mdbSelects = mdbSelectPairs(cart, mdbVars);
    char *output = mdbSelectsHtmlRows(conn,mdbSelects,mdbVars,cols,FALSE);  // not a fileSearch
    if (output)
        {
        puts(output);
        freeMem(output);
        }
    slPairFreeList(&mdbVars);
    }

hPrintf("</table>\n");
hPrintf("<input type='submit' name='%s' id='searchSubmit' value='Search' "
        "style='font-size:.8em;'>\n", TRACK_SEARCH);
hPrintf("<input type='button' id='doSTClear2' name='clear' value='Clear' class='clear' "
        "style='font-size:.8em;'>\n");
jsOnEventById("click", "doSTClear2", "findTracks.clear();");
hPrintf("<input type='submit' name='submit' value='Cancel' class='cancel' "
        "style='font-size:.8em;'>\n");
//hPrintf("<a target='_blank' href='../goldenPath/help/trackSearch.html'>help</a>\n");
hPrintf("</div>\n");

hPrintf("</div>\n");

hPrintf("</form>\n");
hPrintf("</div>"); // Restricts to max-width:1000px;
cgiDown(0.8);

if (measureTiming)
    measureTime("Rendered tabs");

if (doSearch)
    {
    // Now search
    long startTime = clock1000();
    if (hubIdsToUrls == NULL)
        hubIdsToUrls = hashNew(0);
    if (selectedTab==simpleTab && !isEmpty(simpleEntry))
        simpleSearchForTracks(simpleEntry);
    else if (selectedTab==advancedTab)
        advancedSearchForTracks(conn,groupList,nameSearch,typeSearch,descSearch,
                                         groupSearch,mdbSelects, includeHubResults);

    if (measureTiming)
        fprintf(stdout, "<span class='timing'>Searched for tracks: %lu<br></span>", clock1000()-startTime);

    // Sort and Print results
    if (selectedTab!=filesTab)
        {
        enum sortBy sortBy = cartUsualInt(cart,TRACK_SEARCH_SORT,sbRelevance);
        tracksFound = slCount(tracks);
        if (tracksFound > 1)
            findTracksSort(&tracks,sortBy);

        displayFoundTracks(cart,tracks,tracksFound,sortBy);

        if (measureTiming)
            measureTime("Displayed found tracks");
        }
    slPairFreeList(&mdbSelects);
    }
hFreeConn(&conn);

webNewSection("About Track Search");
if (metaDbExists)
    hPrintf("<p>Search for terms in track names, descriptions, groups, and ENCODE "
            "metadata.  If multiple terms are entered, only tracks with all terms "
            "will be part of the results.");
else
    hPrintf("<p>Search for terms in track descriptions, groups, and names. "
            "If multiple terms are entered, only tracks with all terms "
            "will be part of the results.");

hPrintf("<BR><a target='_blank' href='../goldenPath/help/trackSearch.html'>more help</a></p>\n");
webEndSectionTables();
}
