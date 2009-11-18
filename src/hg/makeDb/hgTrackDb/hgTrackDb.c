/* hgTrackDb - Create trackDb table from text files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlList.h"
#include "jksql.h"
#include "trackDb.h"
#include "hui.h"
#include "hdb.h"
#include "hVarSubst.h"
#include "obscure.h"
#include "portable.h"
#include "dystring.h"

static char const rcsid[] = "$Id: hgTrackDb.c,v 1.54 2009/11/18 17:41:23 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTrackDb - Create trackDb table from text files.\n\n"
  "Note that the browser supports multiple trackDb tables, usually\n"
  "in the form: trackDb_YourUserName. Which particular trackDb\n"
  "table the browser uses is specified in the hg.conf file found\n"
  "either in your home directory as '.hg.conf' or in the web \n"
  "server's cgi-bin directory as 'hg.conf'.\n"
  "usage:\n"
  "   hgTrackDb [options] org database trackDb_$(USER) trackDb.sql hgRoot\n"
  "\n"
  "Options:\n"
  "  -visibility=vis.ra - A ra file used to override the initial visibility\n"
  "   settings in trackDb.ra.  This is used to configure the initial setting\n"
  "   for special-purpose browsers.  All visibility will be set to hide and\n"
  "   then specific track are modified using the track and visibility fields\n"
  "   in this file.\n"
  "  -priority=priority.ra - A ra file used to override the priority settings\n"
  "  -hideFirst - Before applying vis.ra, set all visibilities to hide.\n"
  "  -strict - only include tables that exist (and complain about missing html files).\n"
  "  -raName=trackDb.ra - Specify a file name to use other than trackDb.ra\n"
  "   for the ra files.\n"
  "  -release=alpha|beta - Include trackDb entries with this release only.\n"
  "  -settings - for trackDb scanning, output table name, type line,\n"
  "            -  and settings hash to stderr while loading everything\n"
  );
}

static struct optionSpec optionSpecs[] = {
    {"visibility", OPTION_STRING},
    {"priority", OPTION_STRING},
    {"raName", OPTION_STRING},
    {"strict", OPTION_BOOLEAN},
    {"hideFirst", OPTION_BOOLEAN},
    {"release", OPTION_STRING},
    {"settings", OPTION_BOOLEAN},
    {NULL,      0}
};

static char *raName = "trackDb.ra";
static char *release = "alpha";
static bool showSettings = FALSE;

static boolean hasNonAsciiChars(char *text)
/* check if text has any non-printing or non-ascii characters */
{
char *c;
for (c = text; *c != '\0'; c++)
    {
    if ((*c <= 7) || ((14 <= *c) && (*c <= 31)) || (*c >= 127))
        {
        return TRUE;
        }
    }
return FALSE;
}


static struct trackDb *trackDbListFromHash(struct hash *tdHash)
/* build a list of the tracks in hash.  List will be threaded through
 * the entries. */
{
struct trackDb *tdList = NULL;
struct hashCookie cookie = hashFirst(tdHash);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    slSafeAddHead(&tdList, (struct trackDb*)hel->val);
return tdList;
}

static void pruneStrict(struct trackDb **tdListPtr, char *database)
/* purge unused trackDb entries */
{
struct trackDb *td;
struct trackDb *strictList = NULL;
struct hash *compositeHash = hashNew(0);
struct trackDb *compositeList = NULL;
struct hash *superHash = hashNew(0);
struct trackDb *superList = NULL;
char *setting, *words[1];

while ((td = slPopHead(tdListPtr)) != NULL)
    {
    if (td->overrides != NULL)
        {
        // override entry, just keep it in the list for now
        slAddHead(&strictList, td);
        }
    else if (hTableOrSplitExists(database, td->tableName))
        {
        verbose(2, "%s table exists\n", td->tableName);
        slAddHead(&strictList, td);
        if ((setting = trackDbSetting(td, "subTrack")) != NULL)
            {
            /* note subtracks with tables so we can later add
             * the composite trackdb */
            chopLine(cloneString(setting), words);
            hashStore(compositeHash, words[0]);
            }
        else if ((setting = trackDbSetting(td, "superTrack")) != NULL)
            {
            /* note super track member tracks with tables so we can later add
             * the super track trackdb */
            chopLine(cloneString(setting), words);
            hashStore(superHash, words[0]);
            }
        }
    else
        {
        if (trackDbSetting(td, "compositeTrack"))
            {
            slAddHead(&compositeList, td);
            }
        else if (sameOk("on", trackDbSetting(td, "superTrack")))
            {
            slAddHead(&superList, td);
            }
        else
            {
            verbose(2, "%s missing\n", td->tableName);
            trackDbFree(&td);
            }
        }
    }
/* add all composite tracks that have a subtrack with a table */
while ((td = slPopHead(&compositeList)) != NULL)
    {
    if (hashLookup(compositeHash, td->tableName))
        {
        slAddHead(&strictList, td);
        if ((setting = trackDbSetting(td, "superTrack")) != NULL)
            {
            /* note that this is part of a super track so the
             * super track will be added to the strict list */
            chopLine(cloneString(setting), words);
            hashStore(superHash, words[0]);
            }
        }
    }
hashFree(&compositeHash);

/* add all super tracks that have a member (simple or composite) with a table */
while ((td = slPopHead(&superList)) != NULL)
    {
    if (hashLookup(superHash, td->tableName))
        {
        slAddHead(&strictList, td);
        }
    }
hashFree(&superHash);

/* No need to slReverse, it's sorted later. */
*tdListPtr = strictList;
}

/* keep track of prunedTracks so their subTracks can also be pruned */
static struct hash *prunedTracks = NULL;

static void pruneRelease(struct trackDb **tdListPtr, char *database)
/* prune out alternate track entries for another release */
{
struct trackDb *td;
struct trackDb *relList = NULL;

while ((td = slPopHead(tdListPtr)) != NULL)
    {
    char *rel = trackDbSetting(td, "release");
    if (rel != NULL)
        {
        if (sameString(rel, release))
            {
            /* remove release setting from trackDb entry -- there
             * should only be a single entry for the track, so
             * the release setting is no longer relevant (and it
             * confuses Q/A */
            hashRemove(td->settingsHash, "release");
            slSafeAddHead(&relList, td);
            }
	else
            {
	    if (prunedTracks == NULL)
		prunedTracks = newHash(8);
	    hashAddInt(prunedTracks, td->tableName, 1);
            }
        }
    else
        slSafeAddHead(&relList, td);
    }
*tdListPtr = relList;
}

static void applyOverride(struct hash *uniqHash,
                          struct trackDb *overTd)
/* apply a trackDb override to a track, if it exists */
{
struct trackDb *td = hashFindVal(uniqHash, overTd->tableName);
if (td != NULL)
    trackDbOverride(td, overTd);
}

static void addVersionRa(boolean strict, char *database, char *dirName, char *raName,
                         struct hash *uniqHash)
/* Read in tracks from raName and add them to table, pruning as required. Call
 * top-down so that track override will work. */
{
struct trackDb *tdList = trackDbFromRa(raName), *td;

if (strict)
    pruneStrict(&tdList, database);
pruneRelease(&tdList, database);

/* load tracks, replacing higher-level ones with lower-level and
 * applying overrides*/
while ((td = slPopHead(&tdList)) != NULL)
    {
    if (td->overrides != NULL)
        applyOverride(uniqHash, td);
    else
        hashStore(uniqHash, td->tableName)->val = td;
    }
}

void updateBigTextField(struct sqlConnection *conn, char *table,
     char *whereField, char *whereVal,
     char *textField, char *textVal)
/* Generate sql code to update a big text field that may include
 * newlines and stuff. */
{
struct dyString *dy = newDyString(4096);
dyStringPrintf(dy, "update %s set %s=", table, textField);
dyStringQuoteString(dy, '"', textVal);
dyStringPrintf(dy, " where %s = '%s'", whereField, whereVal);
sqlUpdate(conn, dy->string);
freeDyString(&dy);
}

char *subTrackName(char *create, char *tableName)
/* Substitute tableName for whatever is between CREATE TABLE  and  first '('
   freez()'s create passed in. */
{
char newCreate[strlen(create) + strlen(tableName) + 10];
char *front = NULL;
char *rear = NULL;
int length = (strlen(create) + strlen(tableName) + 10);
/* try to find "TABLE" or "table" in create */
front = strstr(create, "TABLE");
if(front == NULL)
    front = strstr(create, "table");
if(front == NULL)
    errAbort("hgTrackDb::subTrackName() - Can't find 'TABLE' in %s", create);

/* try to find first "(" in string */
rear = strstr(create, "(");
if(rear == NULL)
    errAbort("hgTrackDb::subTrackName() - Can't find '(' in %s", create);

/* set to NULL character after "TABLE" */
front += 5;
*front = '\0';

safef(newCreate, length , "%s %s %s", create, tableName, rear);
return cloneString(newCreate);
}

void layerOnRa(boolean strict, char *database, char *dir, struct hash *uniqHash,
               boolean raMustExist)
/* Read trackDb.ra from directory and layer them on top of whatever is in tdList.
 * Must call top-down (root->org->assembly) */
{
char raFile[512];
if (raName[0] != '/')
    safef(raFile, sizeof(raFile), "%s/%s", dir, raName);
else
    safef(raFile, sizeof(raFile), "%s", raName);
if (fileExists(raFile))
    {
    addVersionRa(strict, database, dir, raFile, uniqHash);
    }
else
    {
    if (raMustExist)
        errAbort("%s doesn't exist!", raFile);
    }
}

static void layerOnHtml(char *dirName, struct trackDb *tdList)
/* Read in track HTML call bottom-up. */
{
char fileName[512];
struct trackDb *td;
for (td = tdList; td != NULL; td = td->next)
    {
    if (isEmpty(td->html))
        {
        char *htmlName = trackDbSettingClosestToHome(td, "html");
        if (htmlName == NULL)
            htmlName = td->tableName;
	safef(fileName, sizeof(fileName), "%s/%s.html", dirName, htmlName);
	if (fileExists(fileName))
            {
	    readInGulp(fileName, &td->html, NULL);
            // Check for note ASCII characters at higher levels of verboseness.
            // Normally, these are acceptable ISO-8859-1 characters
            if  ((verboseLevel() >= 2) && hasNonAsciiChars(td->html))
                verbose(2, "Note: non-printing or non-ASCII characters in %s\n", fileName);
            }
        }
    }
}

static char *settingsFromHash(struct hash *hash)
/* Create settings string from settings hash. */
{
if (hash == NULL)
    return cloneString("");
else
    {
    struct dyString *dy = dyStringNew(1024);
    char *ret;
    struct hashEl *el, *list = hashElListHash(hash);
    slSort(&list, hashElCmp);
    for (el = list; el != NULL; el = el->next)
	dyStringPrintf(dy, "%s %s\n", el->name, (char *)el->val);
    slFreeList(&list);
    ret = cloneString(dy->string);
    dyStringFree(&dy);
    return ret;
    }
}

struct subGroupData
/* composite group definitions */
{
int numSubGroups;        /* count of subGroups */
struct hash *nameHash;   /* hash of subGroup names */
struct hash *values[10]; /* array of value hash pointers in order */
struct trackDb *compositeTdb;  /* tdb of composite parent */
};

static void checkSubGroups(struct trackDb *tdList)
/* check integrity of subGroup clauses */
{
struct hash *compositeHash = newHash(8);

/* process all composite tracks */
struct trackDb *td, *tdNext;
for (td = tdList; td != NULL; td = tdNext)
    {
    tdNext = td->next;
    if (trackDbSetting(td, "compositeTrack"))
	{
	int i = 0;
	struct subGroupData *sgd;
        char subGroupName[256];
        AllocVar(sgd);
	sgd->nameHash = newHash(3);
	sgd->compositeTdb = td;
	tdbMarkAsComposite(td);
	while(i<10)
	    {
	    safef(subGroupName, sizeof(subGroupName), "subGroup%d", i+1);
	    char *sgSetting = trackDbSetting(td, subGroupName);
	    if (!sgSetting)
		break;
            sgSetting = cloneString(sgSetting);
	    char *sgWord = sgSetting;
	    char *sgName = nextWord(&sgWord);
	    nextWord(&sgWord);  /* skip word not used */
	    hashAddInt(sgd->nameHash, sgName, i);
	    sgd->values[i] = newHash(3);
	    struct slPair *slPair, *slPairList = slPairFromString(sgWord);
            for (slPair = slPairList; slPair; slPair = slPair->next)
		{
		hashAdd(sgd->values[i], slPair->name, slPair->val);
		}
	    ++i;
	    if (i > 10)
		{
		verbose(1,"composite parent %s has more than 10 subGroup clauses, unexpected error\n", td->tableName);
		break;
		}
	    }
	sgd->numSubGroups = i;
	hashAdd(compositeHash, td->tableName, sgd);

	}
    }

struct hash *removeSubTracks = newHash(8);

/* now verify subtracks */
for (td = tdList; td != NULL; td = tdNext)
    {

    tdNext = td->next;
    if (!trackDbSetting(td, "compositeTrack")
     && !sameOk("on", trackDbSetting(td, "superTrack")))
	{
	char *trackName = trackDbSetting(td, "subTrack");
	if (trackName)
	    {
	    char *words[2];
	    chopLine(cloneString(trackName), words);
	    trackName = words[0];


	    struct subGroupData *sgd = hashFindVal(compositeHash, trackName);
	    if ( sgd )
		{
		td->parent = sgd->compositeTdb;
		tdbMarkAsCompositeChild(td);
		}
	    else
		{
		if ((prunedTracks != NULL) && hashFindVal(prunedTracks, trackName))
		    {	/* parent was pruned, get rid of subTrack too */
		    hashAdd(removeSubTracks, td->tableName, td);
		    }
		else
		    {
		    verbose(1,"parent %s missing for subtrack %s\n",
			trackName, td->tableName);
		    }
		continue;
		}
	    char *subGroups = trackDbSetting(td, "subGroups");
	    if (subGroups && (sgd->numSubGroups == 0))
		{
		verbose(1,"parent %s missing subGroups for subtrack %s subGroups=[%s]\n",
			trackName, td->tableName, subGroups);
		continue;
		}
	    if (!subGroups && (sgd->numSubGroups > 0))
		{
		verbose(1,"parent %s : subtrack %s is missing subGroups\n", trackName, td->tableName);
		continue;
		}
	    if (!subGroups && (sgd->numSubGroups == 0))
		{
		continue; /* nothing to do */
		}
	    int i = 0, lastI = -1, numSubGroups = 0;
	    boolean inOrder = TRUE;
	    struct slPair *slPair, *slPairList = slPairFromString(subGroups);
            for (slPair = slPairList; slPair; slPair = slPair->next)
		{
		i = hashIntValDefault(sgd->nameHash, slPair->name, -1);
		if (i == -1)
		    {
		    verbose(1,"%s: subGroup name not found: %s\n", td->tableName, (char *)slPair->name);
		    }
		else if (sgd->values[i] && !hashLookup(sgd->values[i], slPair->val))
		    {
		    verbose(1,"%s: value not found in parent composite : %s=%s\n", td->tableName, slPair->name, (char *)slPair->val);
		    }
		++numSubGroups;
		if (i < lastI)
		    inOrder = FALSE;
		lastI = i;
		}
	    if (numSubGroups != sgd->numSubGroups)
		{
		verbose(2,"%s: found %d values but parent composite has %d\n", td->tableName, numSubGroups, sgd->numSubGroups);
		}
	    if (!inOrder)
		{
		verbose(2,"%s: found groups in a different order than the parent composite\n", td->tableName);
		}

	    }
	}
    }
/* clean up subTracks that had parents disappear */
struct hashCookie cookie = hashFirst(removeSubTracks);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    slRemoveEl(&tdList, (struct trackDb*)hel->val);

}

static void prioritizeContainerItems(struct trackDb *tdbList)
/* set priorities in containers if they have no priorities already set
   priorities are based upon 'sortOrder' setting or else shortLabel */
{
int countOfSortedContainers = 0;

// Walk through tdbs looking for containers
struct trackDb *tdbContainer;
for (tdbContainer = tdbList; tdbContainer != NULL; tdbContainer = tdbContainer->next)
    {
    if (trackDbSetting(tdbContainer, "compositeTrack") == NULL) // TODO: Expand beyond composites
        continue;

    sortOrder_t *sortOrder = sortOrderGet(NULL,tdbContainer);   // TODO: Expand beyond composites
    boolean needsSorting = TRUE; // default
    float firstPriority = -1.0;
    sortableTdbItem *item,*itemsToSort = NULL;

    // Walk through tdbs looking for items contained
    struct trackDb *tdbItem;
    for (tdbItem = tdbList; tdbItem != NULL; tdbItem = tdbItem->next)
        {
        char *containerName = containerName = trackDbSetting(tdbItem, "subTrack");  // TODO: Expand beyond subtracks
        if (containerName == NULL)
            continue;

        containerName = strSwapChar(cloneString(containerName),' ',0);  // subTrack "compositeName off"
        if(sameString(containerName,tdbContainer->tableName))
            {
            if( needsSorting && sortOrder == NULL )  // do we?
            {
                if( firstPriority == -1.0)    // all 0 or all the same value
                    firstPriority = tdbItem->priority;
                if(firstPriority != tdbItem->priority && (int)(tdbItem->priority + 0.9) > 0)
                    {
                    needsSorting = FALSE;
                    break;
                    }
            }
            // create an Item
            item = sortableTdbItemCreate(tdbItem,sortOrder);
            if(item != NULL)
                slAddHead(&itemsToSort, item);
            else
                {
                verbose(1,"Error: '%s' missing shortLabels or sortOrder setting is inconsistent.\n",tdbContainer->tableName);
                needsSorting = FALSE;
                break;
                }
            }
        freeMem(containerName); // good accounting
        }

    // Does this container need to be sorted?
    if(needsSorting && slCount(itemsToSort))
        {
        verbose(2,"Sorting '%s' with %d items\n",tdbContainer->tableName,slCount(itemsToSort));
        sortTdbItemsAndUpdatePriorities(&itemsToSort);
        countOfSortedContainers++;
        }

    // cleanup
    sortOrderFree(&sortOrder);
    sortableTdbItemsFree(&itemsToSort);
    }
if(countOfSortedContainers > 0)
    verbose(1,"Sorted %d containers\n",countOfSortedContainers);
}

static struct trackDb *buildTrackDb(char *org, char *database, char *hgRoot,
                                    char *visibilityRa, char *priorityRa,
                                    boolean strict)
/* build trackDb objects from files. */
{
struct hash *uniqHash = newHash(8);
char rootDir[PATH_LEN], orgDir[PATH_LEN], asmDir[PATH_LEN];

/* Create track list from hgRoot and hgRoot/org and hgRoot/org/assembly
 * ra format database. */
safef(rootDir, sizeof(rootDir), "%s", hgRoot);
safef(orgDir, sizeof(orgDir), "%s/%s", hgRoot, org);
safef(asmDir, sizeof(asmDir), "%s/%s/%s", hgRoot, org, database);

// must call these top-down
layerOnRa(strict, database, rootDir, uniqHash, TRUE);
layerOnRa(strict, database, orgDir, uniqHash, FALSE);
layerOnRa(strict, database, asmDir, uniqHash, FALSE);

struct trackDb *tdList = trackDbListFromHash(uniqHash);

// must call these bottom-up
layerOnHtml(asmDir, tdList);
layerOnHtml(orgDir, tdList);
layerOnHtml(rootDir, tdList);

if (visibilityRa != NULL)
    trackDbOverrideVisbility(uniqHash, visibilityRa, optionExists("hideFirst"));
if (priorityRa != NULL)
    trackDbOverridePriority(uniqHash, priorityRa);
prioritizeContainerItems(tdList);
slSort(&tdList, trackDbCmp);
hashFree(&uniqHash);
return tdList;
}

void hgTrackDb(char *org, char *database, char *trackDbName, char *sqlFile, char *hgRoot,
               char *visibilityRa, char *priorityRa, boolean strict)
/* hgTrackDb - Create trackDb table from text files. */
{
struct trackDb *td;
char tab[PATH_LEN];
safef(tab, sizeof(tab), "%s.tab", trackDbName);

struct trackDb *tdList = buildTrackDb(org, database, hgRoot,
                                      visibilityRa, priorityRa, strict);
checkSubGroups(tdList);

verbose(1, "Loaded %d track descriptions total\n", slCount(tdList));

/* Write to tab-separated file; hold off on html, since it must be encoded */
    {
    FILE *f = mustOpen(tab, "w");
    for (td = tdList; td != NULL; td = td->next)
        {
        hVarSubstTrackDb(td, database);
        char *hold = td->html;
        td->html = "";
	trackDbTabOut(td, f);
        td->html = hold;
        }
    carefulClose(&f);
    }

/* Update database */
    {
    char *create, *end;
    char query[256];
    struct sqlConnection *conn = sqlConnect(database);

    /* Load in table definition. */
    readInGulp(sqlFile, &create, NULL);
    create = trimSpaces(create);
    create = subTrackName(create, trackDbName);
    end = create + strlen(create)-1;
    if (*end == ';') *end = 0;
    sqlRemakeTable(conn, trackDbName, create);

    /* Load in regular fields. */
    safef(query, sizeof(query), "load data local infile '%s' into table %s", tab, trackDbName);
    sqlUpdate(conn, query);

    /* Load in html and settings fields. */
    for (td = tdList; td != NULL; td = td->next)
	{
        if ( (! tdbIsCompositeChild(td)) && isEmpty(td->html))
	    {
	    if (strict && !trackDbSetting(td, "subTrack") && !sameString(td->tableName,"cytoBandIdeo"))
		{
		fprintf(stderr, "Warning: html missing for %s %s %s '%s'\n",org, database, td->tableName, td->shortLabel);
		}
	    }
	else
	    {
	    if (! tdbIsCompositeChild(td))
		updateBigTextField(conn,  trackDbName, "tableName",
		    td->tableName, "html", td->html);
	    }
	if (td->settingsHash != NULL)
	    {
	    char *settings = settingsFromHash(td->settingsHash);
	    updateBigTextField(conn, trackDbName, "tableName", td->tableName,
	    	"settings", settings);
	    if (showSettings)
		{
		verbose(1, "%s: type='%s';", td->tableName, td->type);
		if (isNotEmpty(settings))
		    {
		    char *oneLine = replaceChars(settings, "\n", "; ");
		    eraseTrailingSpaces(oneLine);
		    verbose(1, " %s", oneLine);
		    freeMem(oneLine);
		    }
		verbose(1, "\n");
		}
	    freeMem(settings);
	    }
	}

    sqlDisconnect(&conn);
    verbose(1, "Loaded database %s\n", database);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 6)
    usage();
raName = optionVal("raName", raName);
showSettings = optionExists("settings");
if (strchr(raName, '/') != NULL)
    errAbort("-raName value should be a file name without directories");
release = optionVal("release", release);

hgTrackDb(argv[1], argv[2], argv[3], argv[4], argv[5],
          optionVal("visibility", NULL), optionVal("priority", NULL),
          optionExists("strict"));
return 0;
}
