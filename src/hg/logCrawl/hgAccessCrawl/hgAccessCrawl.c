/* hgAccessCrawl - Go through Apache access log collecting stats on hgXXX programs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "apacheLog.h"


FILE *errLog = NULL;
int errCode = 0;
FILE *nonRoboLog = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgAccessCrawl - Go through Apache access log collecting stats on hgXXX programs\n"
  "usage:\n"
  "   hgAccessCrawl access_log(s)\n"
  "options:\n"
  "   -errLog=err.log - Put errors into err.log\n"
  "   -errCode=NNN - Only write out to errLog when status code matches errCode\n"
  "   -nonRobot=file - Write out non-robot CGI lines to file\n"
  "   -verbose=N  - Set verbosity level.  0 for silent, 1 for input data warnings, \n"
  "                 2 for status.\n"
  );
}

static struct optionSpec options[] = {
   {"errLog", OPTION_STRING},
   {"errCode", OPTION_INT},
   {"nonRobot", OPTION_STRING},
   {NULL, 0},
};

boolean cgiHashVal(struct hash *cgiHash, char *var, char *val)
/* Return TRUE if var exists in hash with given value. */
{
struct cgiVar *cv = hashFindVal(cgiHash, var);
return cv != NULL && sameString(cv->val, val);
}

struct nameCount
/* List of name/count pairs. */
    {
    struct nameCount *next;
    char *name;
    int count;
    };

boolean isRobot(char *ip, char *program)
/* Return TRUE if it appears to be a robot ip address. */
{
static struct hash *roboHash = NULL;
if (startsWith("Java", program))
    return TRUE;
else if (startsWith("Wget", program))
    return TRUE;
else if (startsWith("AgentName", program))
    return TRUE;
else if (startsWith("libwww-perl", program))
    return TRUE;
else if (startsWith("Googlebot", program))
    return TRUE;
else if (startsWith("ia_archiver", program))
    return TRUE;
else if (startsWith("Hatena Antenna", program))
    return TRUE;
else if (startsWith("webBlat", program))
    return TRUE;
else if (startsWith("HTTP::Lite", program))
    return TRUE;
else if (startsWith("Python-urllib", program))
    return TRUE;
else if (startsWith("LWP::Simple", program))
    return TRUE;
else if (startsWith("httpunit", program))
    return TRUE;
else if (startsWith("Teleport Pro", program))
    return TRUE;
else if (startsWith("WWW-Mechanize", program))
    return TRUE;
else if (startsWith("Bio::Das::", program))
    return TRUE;
else if (stringIn("Googlebot", program))
    return TRUE;
else if (sameString("-", program))
    return TRUE;
else if (startsWith("Microsoft Data Access", program))
    return TRUE;
if (roboHash == NULL)
    {
    roboHash = hashNew(0);
    hashAdd(roboHash, "joiner.stanford.edu", NULL);
    hashAdd(roboHash, "pc-glass-1.ucsd.edu", NULL);
    hashAdd(roboHash, "pc-glass-2.ucsd.edu", NULL);
    hashAdd(roboHash, "pc-glass-3.ucsd.edu", NULL);
    hashAdd(roboHash, "64-170-97-98.ded.pacbell.net", NULL);
    hashAdd(roboHash, "ce.hosts.jhmi.edu", NULL);
    hashAdd(roboHash, "technetium.hgsc.bcm.tmc.edu", NULL);
    hashAdd(roboHash, "62.232.24.178", NULL);
    }
return hashLookup(roboHash, ip) != NULL;
}

int nameCountCmp(const void *va, const void *vb)
/* Compare to sort based on count - biggest first. */
{
const struct nameCount *a = *((struct nameCount **)va);
const struct nameCount *b = *((struct nameCount **)vb);
return b->count - a->count;
}

struct visCount
/* Keep track of visibility settings observed. */
    {
    struct visCount *next;
    char *track;		/* Name of track.  Not allocated here. */
    int hideCount;
    int denseCount;
    int squishCount;
    int packCount;
    int fullCount;
    int visCount;	/* Sum of all but hide. */
    };

int visCountCmp(const void *va, const void *vb)
/* Compare to sort based on visCount - biggest first. */
{
const struct visCount *a = *((struct visCount **)va);
const struct visCount *b = *((struct visCount **)vb);
return b->visCount - a->visCount;
}

boolean isTrackVal(char *val)
/* Return TRUE if this is a value we expect in a track name */
{
return sameString(val, "hide") || sameString(val, "dense")
	|| sameString(val, "squish") || sameString(val, "pack")
	|| sameString(val, "full");
}

void recordTrackVis(struct cgiVar *cv, struct hash *trackHash, 
	struct visCount **pVcList)
/* If var looks like it is a track, update trackHash with visibility
 * value. */
{
char *val = cv->val;
if (isTrackVal(val))
    {
    struct visCount *vc = hashFindVal(trackHash, cv->name);
    if (vc == NULL)
	{
	AllocVar(vc);
	hashAddSaveName(trackHash, cv->name, vc, &vc->track);
	slAddHead(pVcList, vc);
	}
    if (sameString(val, "hide"))
        vc->hideCount += 1;
    else if (sameString(val, "dense"))
	{
        vc->denseCount += 1;
	vc->visCount += 1;
	}
    else if (sameString(val, "squish"))
	{
        vc->squishCount += 1;
	vc->visCount += 1;
	}
    else if (sameString(val, "pack"))
	{
        vc->packCount += 1;
	vc->visCount += 1;
	}
    else if (sameString(val, "full"))
	{
        vc->fullCount += 1;
	vc->visCount += 1;
	}
    }
}

struct cgiProgram
/* Data on one cgi program. */
    {
    struct cgiProgram *next;
    char *name;	/* Name of program. */
    int totalHits;	/* Total hit count. */
    int roboHits;	/* Robot hit count. */
    };

int cgiProgramCmp(const void *va, const void *vb)
/* Compare to sort based on count - biggest first. */
{
const struct cgiProgram *a = *((struct cgiProgram **)va);
const struct cgiProgram *b = *((struct cgiProgram **)vb);
return b->totalHits - a->totalHits;
}


void hgAccessCrawl(int logCount, char *logFiles[])
/* hgAccessCrawl - Go through Apache access log collecting stats on hgXXX programs. */
{
struct cgiProgram *progList = NULL, *prog;
struct hash *progHash = hashNew(0);
int i;
int hgTracksTotal = 0;
int hgTracksPosted = 0;
/* int hgNearTotal = 0;
int hgGeneTotal = 0;
int hgTextTotal = 0;
int hgBlatTotal = 0;
int hgTablesTotal = 0; */
int hgcTotal = 0;
int dbTotal = 0;
int other = 0;
int fromGateway = 0;
int fromHgBlat = 0;
int fromOtherBlat = 0;
int fromHgGene = 0;
int fromHgc = 0;
int fromHgNear = 0;
int fromEncode = 0;
int fromOutside = 0;
int zoomIn = 0;
int zoomOut = 0;
int dink = 0;
int left = 0;
int right = 0;
int jump = 0;
int refresh = 0;
int gatewayMultiple = 0;
int zoomInRuler = 0;
int hgTracksRobot = 0;
/* int hgTablesRobot = 0;
int hgTextRobot = 0;
int hgGeneRobot = 0;
int hgNearRobot = 0;
int hgBlatRobot = 0; */
int undisclosedOutsideSimple = 0;
int undisclosedOutsideWithCustom = 0;
int resetAll = 0;
int hideAll = 0;
int postScriptOutput = 0;
int addYourOwn = 0;
struct hash *gHash = hashNew(0);	/* g (track) var for hgc */
struct nameCount *gList = NULL, *gEl, *gNone, *gPost, *gRobot;
struct hash *dbHash = hashNew(8);       /* db var in hgTracks after hgGateway */
struct nameCount *dbList = NULL, *dbEl;
struct hash *trackHash = hashNew(10);
struct visCount *vcList = NULL;
/* struct visCount *vcList = NULL, *vc; */

/* Allocate dummy group for POSTed htc's. */
AllocVar(gPost);
gPost->name = "Posted (no CGI vars available)";
slAddHead(&gList, gPost);

/* Allocate dummy group for htc's with no 'g' variable. */
AllocVar(gNone);
gNone->name = "no 'g'";
slAddHead(&gList, gNone);

/* Allocate dummy group for htc robots. */
AllocVar(gRobot);
gRobot->name = "robot";
slAddHead(&gList, gRobot);

for (i=0; i<logCount; ++i)
    {
    char *fileName = logFiles[i];
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    char *line;
    while (lineFileNext(lf, &line, NULL))
        {
	struct apacheAccessLog *ll = apacheAccessLogParse(line, lf->fileName, lf->lineIx);
	if (ll != NULL)
	    {
	    if (errLog != NULL 
	    	&& ll->status != 200 && ll->status != 304 
		&& ll->status != 206 && ll->status != 301)
	       {
	       if (errCode == 0 || errCode == ll->status)
		   if (!isRobot(ll->ip, ll->program))
		       fprintf(errLog, "%s\n", line);
	       }
	    if (startsWith("/cgi-bin/", ll->url))
		{
		boolean thisIsRobot = isRobot(ll->ip, ll->program);
		char *progNameStart = ll->url + strlen("/cgi-bin/");
		char *progNameEnd = strchr(progNameStart, '?');
		char *progName;
		if (!thisIsRobot && nonRoboLog != NULL)
		   fprintf(nonRoboLog, "%s\n", line);
		if (progNameEnd == NULL)
		    progName = cloneString(progNameStart);
		else
		    progName = cloneStringZ(progNameStart, progNameEnd-progNameStart);
		progNameEnd = strchr(progName, '/');
		if (progNameEnd != NULL)
		     *progNameEnd = 0;
		prog = hashFindVal(progHash, progName);
		if (prog == NULL)
		    {
		    AllocVar(prog);
		    hashAddSaveName(progHash, progName, prog, &prog->name);
		    slAddHead(&progList, prog);
		    }
		prog->totalHits += 1;
		if (thisIsRobot)
		    prog->roboHits += 1;
		if (sameString(ll->method, "GET"))
		    {
		    struct hash *cgiHash;
		    struct cgiVar *cgiList, *cv;
		    char *cgiString = strchr(ll->url, '?');
		    int cgiCount;
		    if (cgiString == NULL)
			cgiString = "";
		    else
			cgiString += 1;
		    cgiString = cloneString(cgiString);
		    if (!cgiParseInput(cgiString, &cgiHash, &cgiList))
			{
			if (verboseLevel() > 1)
			    printf("%s\n", ll->url);
			continue;
			}
		    cgiCount = slCount(cgiList);
		    if (startsWith("/cgi-bin/hgTracks", ll->url))
			{
			++hgTracksTotal;

			/* Here we try to determine the popularity of each 
			 * database (organism+assembly) by looking at
			 * initial entries from hgGateway into hgTracks. */
			if (ll->referrer != NULL && !thisIsRobot && stringIn("hgGateway", ll->referrer))
			    {
			    cv = hashFindVal(cgiHash, "db");
			    if (cv != NULL)
				{
				char *db = cv->val;
				dbEl = hashFindVal(dbHash, db);
				if (dbEl == NULL)
				    {
				    AllocVar(dbEl);
				    hashAddSaveName(dbHash, db, dbEl, &dbEl->name);
				    slAddHead(&dbList, dbEl);
				    }
				dbEl->count += 1;
				dbTotal += 1;
				}
			    }

			/* Count up dense/squished/packed/full track usage */
			if (!thisIsRobot)
			    {
			    for (cv = cgiList; cv != NULL; cv = cv->next)
				recordTrackVis(cv, trackHash, &vcList);
			    }

			/* Count up hits in a bunch of mutually exclusive
			 * categories. */
			if (thisIsRobot)
			    ++hgTracksRobot;
			else if (cgiHashVal(cgiHash, "Submit", "Submit"))
			    ++fromGateway;
			else if (cgiHashVal(cgiHash, "submit", "jump"))
			    ++jump;
			else if (cgiHashVal(cgiHash, "submit", "refresh"))
			    ++refresh;
			else if (stringIn("hgt.out", ll->url))
			    ++zoomOut;
			else if (stringIn("hgt.in", ll->url))
			    ++zoomIn;
			else if (stringIn("hgt.left", ll->url))
			    ++left;
			else if (stringIn("hgt.right", ll->url))
			    ++right;
			else if (stringIn("hgt.dink", ll->url))
			    ++dink;
			else if (ll->referrer != NULL && stringIn("cgi-bin/hgBlat", ll->referrer))
			    ++fromHgBlat;
			else if (ll->referrer == NULL && 
			    hashLookup(cgiHash, "ss"))
			    ++fromOtherBlat;
			else if (ll->referrer != NULL 
				&& !startsWith("http://genome.ucsc.edu", ll->referrer)
				&& !startsWith("http://genome.cse.ucsc.edu", ll->referrer))
			    ++fromOutside;
			else if (cgiCount == 2 
			    && hashLookup(cgiHash, "position") 
			    && hashLookup(cgiHash, "hgsid"))
			    {
			    ++zoomInRuler;
			    }
			else if (cgiCount == 3 
			    && hashLookup(cgiHash, "position") 
			    && hashLookup(cgiHash, "hgsid"))
			    {
			    ++gatewayMultiple;
			    }
			else if (stringIn("dummyEnterButton", ll->url))
			    {
			    if (stringIn("guideline", ll->url))
				++jump;
			    else
				++fromGateway;
			    }
			else if (ll->referrer == NULL && cgiCount == 2  &&
			    hashLookup(cgiHash, "db") && hashLookup(cgiHash, "position"))
			    ++undisclosedOutsideSimple;
			else if (ll->referrer == NULL && cgiCount == 2  &&
			    hashLookup(cgiHash, "org") && hashLookup(cgiHash, "position"))
			    ++undisclosedOutsideSimple;
			else if (ll->referrer == NULL && cgiCount == 3  &&
			    hashLookup(cgiHash, "db") 
			    && hashLookup(cgiHash, "org")
			    && hashLookup(cgiHash, "position"))
			    ++undisclosedOutsideSimple;
			else if (ll->referrer == NULL && hashLookup(cgiHash, "hgt.customText"))
			    ++undisclosedOutsideWithCustom;
			else if (hashLookup(cgiHash, "hgt.reset"))
			    ++resetAll;
			else if (hashLookup(cgiHash, "hgt.hideAll"))
			    ++hideAll;
			else if (ll->referrer != NULL && stringIn("cgi-bin/hgc", ll->referrer))
			    ++fromHgc;
			else if (ll->referrer != NULL && stringIn("cgi-bin/hgNear", ll->referrer))
			    ++fromHgNear;
			else if (ll->referrer != NULL && stringIn("cgi-bin/hgGene", ll->referrer))
			    ++fromHgGene;
			else if (ll->referrer != NULL && 
			     (stringIn("ENCODE", ll->referrer) || stringIn("Encode", ll->referrer)) )
			    ++fromEncode;
			else if (hashLookup(cgiHash, "hgt.psOutput"))
			    ++postScriptOutput;
			else if (stringIn("Add+Your+Own", ll->url))
			    ++addYourOwn;
			else
			    {
			    ++other;
			    if (verboseLevel() >= 3)
				printf("%s\n", line);
			    }
			}
		    else if (startsWith("/cgi-bin/hgc", ll->url))
			{
			struct cgiVar *cv = hashFindVal(cgiHash, "g");
			struct nameCount *gEl;
			++hgcTotal;
			if (thisIsRobot)
			    {
			    gRobot->count += 1;
			    }
			else if (cv == NULL)
			    {
			    gNone->count += 1;
			    }
			else
			    {
			    gEl = hashFindVal(gHash, cv->val);
			    if (gEl == NULL)
				{
				AllocVar(gEl);
				hashAddSaveName(gHash, cv->val, gEl, &gEl->name);
				slAddHead(&gList, gEl);
				}
			    gEl->count += 1;
			    }
			}
		    hashFree(&cgiHash);
		    slFreeList(&cgiList);
		    freez(&cgiString);
		    }
		else if (sameString(ll->method, "POST"))
		    {
		    if (startsWith("/cgi-bin/hgc", ll->url))
			{
			hgcTotal += 1;
			if (isRobot(ll->ip, ll->program))
			    gRobot->count += 1;
			else
			    gPost->count += 1;
			}
		    else if (startsWith("/cgi-bin/hgTracks", ll->url))
			{
			hgTracksTotal += 1;
			if (isRobot(ll->ip, ll->program))
			    hgTracksRobot += 1;
			else
			    hgTracksPosted += 1;
			}
		    }
		freez(&progName);
		}
	    apacheAccessLogFree(&ll);
	    }
	}
    }

printf("CGI Programs:\n");
slSort(&progList, cgiProgramCmp);
for (prog = progList; prog != NULL; prog = prog->next)
    {
    char *name = prog->name;
    if (strchr(name, '%') == NULL && !endsWith(name, "_files"))
	printf("%s total %d, robot %d (%3.2f%%)\n", prog->name, prog->totalHits,
	    prog->roboHits, 100.0 * prog->roboHits/prog->totalHits);
    }
printf("\n");


slSort(&dbList, nameCountCmp);
printf("Total entries from hgGateway with db set: %d\n", dbTotal);
for (dbEl = dbList; dbEl != NULL; dbEl = dbEl->next)
    {
    printf("%4.2f%% db %s: %d\n", 100.0 * dbEl->count/dbTotal, 
    	dbEl->name, dbEl->count);
    }

printf("hgTracksTotal: %d\n", hgTracksTotal);
printf("hgTracksPosted: %d\n", hgTracksPosted);
printf("fromGateway: %d\n", fromGateway);
printf("gatewayMultiple: %d\n", gatewayMultiple);
printf("fromHgBlat: %d\n", fromHgBlat);
printf("fromOtherBlat: %d\n", fromOtherBlat);
printf("fromHgNear: %d\n", fromHgNear);
printf("fromHgc: %d\n", fromHgc);
printf("fromHgGene: %d\n", fromHgGene);
printf("zoomIn: %d\n", zoomIn);
printf("zoomOut: %d\n", zoomOut);
printf("dink: %d\n", dink);
printf("left: %d\n", left);
printf("right: %d\n", right);
printf("jump: %d\n", jump);
printf("refresh: %d\n", refresh);
printf("zoomInRuler: %d\n", zoomInRuler);
printf("fromOutside: %d\n", fromOutside);
printf("undisclosedOutsideSimple: %d\n", undisclosedOutsideSimple);
printf("undisclosedOutsideWithCustom: %d\n", undisclosedOutsideWithCustom);
printf("robot: %d\n", hgTracksRobot);
printf("resetAll: %d\n", resetAll);
printf("hideAll: %d\n", hideAll);
printf("fromEncode: %d\n", fromEncode);
printf("postScriptOutput: %d\n", postScriptOutput);
printf("addYourOwn: %d\n", addYourOwn);
printf("other: %d\n", other);
printf("\n");

#ifdef OLD /* Sadly track visibilities are now posted now so we don't know. */
printf("\n");
slSort(&vcList, visCountCmp);
printf("Count of track visibility\n");
for (vc = vcList; vc != NULL; vc = vc->next)
    {
    int total = vc->visCount + vc->hideCount;
    double scale = 100.0/total;
    printf("%s: %d visible, %4.2f%% hidden, %4.2f%% dense, "
    	   "%4.2f%% squish, %4.2f%% pack, %4.2f%% full\n",
	   vc->track, vc->visCount, scale*vc->hideCount,
	   scale*vc->denseCount, scale*vc->squishCount,
	   scale*vc->packCount, scale*vc->fullCount);
    }
#endif /* OLD */

slSort(&gList, nameCountCmp);
printf("total hgc clicks: %d\n", hgcTotal);
for (gEl = gList; gEl != NULL; gEl = gEl->next)
    {
    printf("%4.2f%% hgc %s: %d\n", 100.0 * gEl->count/hgcTotal, 
    	gEl->name, gEl->count);
    }
printf("\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (optionExists("errLog"))
    errLog = mustOpen(optionVal("errLog", NULL), "w");
errCode = optionInt("errCode", 0);
if (optionExists("nonRobot"))
    nonRoboLog = mustOpen(optionVal("nonRobot", NULL), "w");
if (argc < 2)
    usage();
hgAccessCrawl(argc-1, argv+1);
return 0;
}
