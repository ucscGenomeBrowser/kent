/* hgTrackDb - Create trackDb table from text files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlList.h"
#include "jksql.h"
#include "trackDb.h"
#include "hdb.h"
#include "obscure.h"
#include "portable.h"
#include "dystring.h"

static char const rcsid[] = "$Id: hgTrackDb.c,v 1.40 2008/08/08 00:29:55 kate Exp $";

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
  "  -local - connect to local host, instead of default host, using localDb.XXX variables defined in .hg.conf.\n"
  "  -raName=trackDb.ra - Specify a file name to use other than trackDb.ra\n"
  "   for the ra files.\n" 
  "  -release=alpha|beta - Include trackDb entries with this release only.\n"
  );
}

static struct optionSpec optionSpecs[] = {
    {"visibility", OPTION_STRING},
    {"priority", OPTION_STRING},
    {"raName", OPTION_STRING},
    {"strict", OPTION_BOOLEAN},
    {"local", OPTION_BOOLEAN},
    {"hideFirst", OPTION_BOOLEAN},
    {"release", OPTION_STRING},
    {NULL,      0}
};

static char *raName = "trackDb.ra";
static char *release = "alpha";
boolean localDb=FALSE;               /* Connect to local host, instead of default host, using localDb.XXX variables defined in .hg.conf.\n"*/ 

void addVersion(boolean strict, char *database, char *dirName, char *raName, 
    struct hash *uniqHash,
    struct hash *htmlHash,
    struct trackDb **pTrackList)
/* Read in tracks from raName and add them to list/database if new. */
{
struct trackDb *tdList = NULL, *td, *tdNext;
char fileName[512];
char *words[1];

tdList = trackDbFromRa(raName);

if (strict) 
    {
    struct trackDb *strictList = NULL;
    struct hash *compositeHash = hashNew(0);
    struct trackDb *compositeList = NULL;
    struct hash *superHash = hashNew(0);
    struct trackDb *superList = NULL;
    char *setting;
    for (td = tdList; td != NULL; td = tdNext)
        {
        tdNext = td->next;
        if (hTableOrSplitExists(td->tableName))
            {
            if (verboseLevel() > 2)
                printf("%s table exists\n", td->tableName);
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
                if (verboseLevel() > 1)
                    printf("%s missing\n", td->tableName);
                trackDbFree(&td);
                }
	    }
        }
    /* add all composite tracks that have a subtrack with a table */
    for (td = compositeList; td != NULL; td = tdNext)
        {
        tdNext = td->next;
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
    for (td = superList; td != NULL; td = tdNext)
        {
        tdNext = td->next;
        if (hashLookup(superHash, td->tableName))
            {
	    slAddHead(&strictList, td);
            }
        }
    hashFree(&superHash);

    /* No need to slReverse, it's sorted later. */
    tdList = strictList;
    }

/* prune out alternate track entries for another release */
for (td = tdList; td != NULL; td = tdNext)
    {
    char *rel;
    tdNext = td->next;
    if ((rel = trackDbSetting(td, "release")) != NULL)
        {
        if (differentString(rel, release))
           slRemoveEl(&tdList, td);
        else
            /* remove release setting from trackDb entry -- there
             * should only be a single entry for the track, so 
             * the release setting is no longer relevant (and it
             * confuses Q/A */
        hashRemove(td->settingsHash, "release");
        }
    }

for (td = tdList; td != NULL; td = tdNext)
    {
    tdNext = td->next;
    if (!hashLookup(uniqHash, td->tableName))
        {
	hashAdd(uniqHash, td->tableName, td);
        slAddHead(pTrackList, td);
	}
    }
for (td = *pTrackList; td != NULL; td = td->next)
    {
    char *htmlName;
    if ((htmlName = trackDbSetting(td, "html")) == NULL)
        htmlName = td->tableName;
    if (!hashLookup(htmlHash, td->tableName))
	{
	sprintf(fileName, "%s/%s.html", dirName, htmlName);
	if (fileExists(fileName))
	    {
	    char *s;
	    readInGulp(fileName, &s, NULL);
	    hashAdd(htmlHash, td->tableName, s);
	    }
	}
    }
}

void dyStringQuoteString(struct dyString *dy, char quotChar, char *text)
/* Stringize text onto end of dy. */
{
char c;

dyStringAppendC(dy, quotChar);
while ((c = *text++) != 0)
    {
    if (c == quotChar)
        dyStringAppendC(dy, '\\');
    dyStringAppendC(dy, c);
    }
dyStringAppendC(dy, quotChar);
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

snprintf(newCreate, length , "%s %s %s", create, tableName, rear);
return cloneString(newCreate);
}


void layerOn(boolean strict, char *database, char *dir, struct hash *uniqHash, 
	struct hash *htmlHash,  boolean mustExist, struct trackDb **tdList)
/* Read trackDb.ra from directory and any associated .html files,
 * and layer them on top of whatever is in tdList. */
{
char raFile[512];
if (raName[0] != '/') 
    safef(raFile, sizeof(raFile), "%s/%s", dir, raName);
else
    safef(raFile, sizeof(raFile), "%s", raName);
if (fileExists(raFile))
    {
    addVersion(strict, database, dir, raFile, uniqHash, htmlHash, tdList);
    }
else 
    {
    if (mustExist)
        errAbort("%s doesn't exist!", raFile);
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
	    if (!sgd)
		{
		verbose(1,"parent %s missing for subtrack %s\n", trackName, td->tableName);
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


}


void hgTrackDb(char *org, char *database, char *trackDbName, char *sqlFile, char *hgRoot,
               char *visibilityRa, char *priorityRa, boolean strict)
/* hgTrackDb - Create trackDb table from text files. */
{
struct hash *uniqHash = newHash(8);
struct hash *htmlHash = newHash(8);
struct trackDb *tdList = NULL, *td;
char rootDir[512], orgDir[512], asmDir[512];
char tab[512];
snprintf(tab, sizeof(tab), "%s.tab", trackDbName);

/* Create track list from hgRoot and hgRoot/org and hgRoot/org/assembly 
 * ra format database. */
sprintf(rootDir, "%s", hgRoot);
sprintf(orgDir, "%s/%s", hgRoot, org);
sprintf(asmDir, "%s/%s/%s", hgRoot, org, database);
hSetDb(database);
layerOn(strict, database, asmDir, uniqHash, htmlHash, FALSE, &tdList);
layerOn(strict, database, orgDir, uniqHash, htmlHash, FALSE, &tdList);
layerOn(strict, database, rootDir, uniqHash, htmlHash, TRUE, &tdList);
if (visibilityRa != NULL)
    trackDbOverrideVisbility(uniqHash, visibilityRa,
			     optionExists("hideFirst"));
if (priorityRa != NULL)
    trackDbOverridePriority(uniqHash, priorityRa);
slSort(&tdList, trackDbCmp);

checkSubGroups(tdList); 

printf("Loaded %d track descriptions total\n", slCount(tdList));

/* Write to tab-separated file. */
    {
    FILE *f = mustOpen(tab, "w");
    for (td = tdList; td != NULL; td = td->next)
	trackDbTabOut(td, f);
    carefulClose(&f);
    }

/* Update database */
    {
    char *create, *end;
    char query[256];
    struct sqlConnection *conn = NULL;
    if (!localDb)
	conn = sqlConnect(database);
    else
	conn = hConnectLocalDb(database);

    /* Load in table definition. */
    readInGulp(sqlFile, &create, NULL);
    create = trimSpaces(create);
    create = subTrackName(create, trackDbName);
    end = create + strlen(create)-1;
    if (*end == ';') *end = 0;
    sqlRemakeTable(conn, trackDbName, create);

    /* Load in regular fields. */
    sprintf(query, "load data local infile '%s' into table %s", tab, trackDbName);
    sqlUpdate(conn, query);

    /* Load in html and settings fields. */
    for (td = tdList; td != NULL; td = td->next)
	{
	char *html = hashFindVal(htmlHash, td->tableName);
        if (html == NULL)
	    {
	    if (strict && !trackDbSetting(td, "subTrack") && !sameString(td->tableName,"cytoBandIdeo"))
		{
		printf("html missing for %s %s %s '%s'\n",org, database, td->tableName, td->shortLabel);
		}
	    }
	else
	    {
	    updateBigTextField(conn,  trackDbName, "tableName", td->tableName, 
	    	"html", html);
	    }
	if (td->settingsHash != NULL)
	    {
	    char *settings = settingsFromHash(td->settingsHash);
	    updateBigTextField(conn, trackDbName, "tableName", td->tableName, 
	    	"settings", settings);
	    freeMem(settings);
	    }
	}

    sqlDisconnect(&conn);
    printf("Loaded database %s\n", database);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 6)
    usage();
raName = optionVal("raName", raName);
if (strchr(raName, '/') != NULL)
    errAbort("-raName value should be a file name without directories");
release = optionVal("release", release);
localDb = optionExists("local");

hgTrackDb(argv[1], argv[2], argv[3], argv[4], argv[5],
          optionVal("visibility", NULL), optionVal("priority", NULL), 
          optionExists("strict"));
return 0;
}
