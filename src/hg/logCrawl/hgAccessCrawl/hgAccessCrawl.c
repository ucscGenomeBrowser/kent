/* hgAccessCrawl - Go through Apache access log collecting stats on hgXXX programs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "cheapcgi.h"

static char const rcsid[] = "$Id: hgAccessCrawl.c,v 1.3 2003/12/24 04:52:02 kent Exp $";

int verbose = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgAccessCrawl - Go through Apache access log collecting stats on hgXXX programs\n"
  "usage:\n"
  "   hgAccessCrawl access_log(s)\n"
  "options:\n"
  "   -verbose=N  - Set verbosity level.  0 for silent, 1 for input data warnings, \n"
  "                 2 for status.\n"
  );
}

static struct optionSpec options[] = {
   {"verbose", OPTION_INT},
   {NULL, 0},
};

struct logLine
/* Parsed out apache access log line */
    {
    struct logLine *next;
    char *buf;		/* All memory for logLine fields is allocated at once here. */
    char *ip;		/* IP Address: dotted quad of numbers, or xxx.com. */
    char *dash1;	/* Unknown, usually a dash */
    char *dash2;	/* Unknown, usually a dash */
    char *timeStamp;	/* Time stamp like 23/Nov/2003:04:21:08 */
    char *tsExtra;	/* Extra number after timeStamp, usually -0800 */
    char *method;	/* GET/POST etc. */
    char *url;		/* Requested URL */
    char *httpVersion;  /* Something like HTTP/1.1 */
    int status;		/* Status code - 200 is good! */
    char *num1;		/* Some number, I'm not sure what it is. */
    char *referrer;	/* Referring URL, may be NULL. */
    char *program;	/* Requesting program,  often Mozilla 4.0 */
    };

void logLineFree(struct logLine **pLl)
/* Free up logLine. */
{
struct logLine *ll = *pLl;
if (ll != NULL)
    {
    freeMem(ll->buf);
    freez(pLl);
    }
}


static void badFormat(struct logLine **pLl, char *line, char *fileName, 
	int lineIx, char *message)
/* Complain about format if verbose flag is on.  Free up
 * *pLl */
{
if (verbose  > 0)
    {
    if (fileName != NULL)
	warn("%s line %d: %s", fileName, lineIx, message);
    else
	warn("%s", message);
    }
}

static void unterminatedQuote(struct logLine **pLl, char *line, 
	char *fileName, int lineIx)
/* Complain about unterminated quote. */
{
badFormat(pLl, line, fileName, lineIx, 
	"missing closing quote");
}

static void shortLine(struct logLine **pLl, char *line, 
	char *fileName, int lineIx)
/* Complain about short line. */
{
badFormat(pLl, line, fileName, lineIx, 
	"short line");
}

static void badTimeStamp(struct logLine **pLl, char *line, 
	char *fileName, int lineIx)
/* Complain about bad time stamp. */
{
badFormat(pLl, line, fileName, lineIx, 
	"bad time stamp");
}

struct logLine *logLineParse(char *line, char *fileName, int lineIx)
/* Return a logLine from line.  Return NULL if there's a parsing problem, but
 * don't abort. */
{
struct logLine *ll;
char *buf, *s, *e;
AllocVar(ll);
ll->buf = buf = cloneString(line);
ll->ip = nextWord(&buf);
ll->dash1 = nextWord(&buf);
ll->dash2 = nextWord(&buf);
if (buf == NULL)
    {
    shortLine(&ll, line, fileName, lineIx);
    return NULL;
    }
s = strchr(buf, '[');
if (s == NULL)
    {
    badTimeStamp(&ll, line, fileName, lineIx);
    return NULL;
    }
s += 1;
e = strchr(s, ']');
if (e == NULL)
    {
    badTimeStamp(&ll, line, fileName, lineIx);
    return NULL;
    }
*e = 0;
ll->timeStamp = nextWord(&s);
if (!isdigit(ll->timeStamp[0]))
    {
    badTimeStamp(&ll, line, fileName, lineIx);
    return NULL;
    }
ll->tsExtra = nextWord(&s);
buf = e+2;
if (buf[0] != '"')
    {
    badFormat(&ll, line, fileName, lineIx, "Missing quote after time stamp");
    return NULL;
    }
if (!parseQuotedString(buf, buf, &e))
    {
    unterminatedQuote(&ll, line, fileName, lineIx);
    return NULL;
    }
ll->method = nextWord(&buf);
ll->url = nextWord(&buf);
ll->httpVersion = nextWord(&buf);
if (ll->url == NULL)
    {
    badFormat(&ll, line, fileName, lineIx, "Missing URL");
    return NULL;
    }
buf = e;
s = nextWord(&buf);
if (!isdigit(s[0]))
    {
    badFormat(&ll, line, fileName, lineIx, "Non-numerical status code");
    return NULL;
    }
ll->status = atoi(s);
ll->num1 = nextWord(&buf);
if (buf == NULL)
    {
    shortLine(&ll, line, fileName, lineIx);
    return NULL;
    }
if (buf[0] != '"')
    {
    badFormat(&ll, line, fileName, lineIx, "Missing quote after request");
    return NULL;
    }
if (!parseQuotedString(buf, buf, &e))
    {
    unterminatedQuote(&ll, line, fileName, lineIx);
    return NULL;
    }
if (!sameString(buf, "-"))
    ll->referrer = buf;
buf = e + 1;
if (buf[0] != '"')
    {
    badFormat(&ll, line, fileName, lineIx, "Missing quote after referrer");
    return NULL;
    }
if (!parseQuotedString(buf, buf, &e))
    {
    unterminatedQuote(&ll, line, fileName, lineIx);
    return NULL;
    }
ll->program = buf;
return ll;
}

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
if (roboHash == NULL)
    {
    roboHash = hashNew(0);
    hashAdd(roboHash, "joiner.stanford.edu", NULL);
    hashAdd(roboHash, "pc-glass-1.ucsd.edu", NULL);
    hashAdd(roboHash, "pc-glass-2.ucsd.edu", NULL);
    hashAdd(roboHash, "pc-glass-3.ucsd.edu", NULL);
    hashAdd(roboHash, "64-170-97-98.ded.pacbell.net", NULL);
    hashAdd(roboHash, "ce.hosts.jhmi.edu", NULL);
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

void hgAccessCrawl(int logCount, char *logFiles[])
/* hgAccessCrawl - Go through Apache access log collecting stats on hgXXX programs. */
{
int i;
int hgTracksTotal = 0;
int hgTracksPosted = 0;
int hgNearTotal = 0;
int hgGeneTotal = 0;
int hgTextTotal = 0;
int hgBlatTotal = 0;
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
int hgTextRobot = 0;
int hgGeneRobot = 0;
int hgNearRobot = 0;
int hgBlatRobot = 0;
int undisclosedOutsideSimple = 0;
int undisclosedOutsideWithCustom = 0;
int resetAll = 0;
int hideAll = 0;
int postScriptOutput = 0;
int addYourOwn = 0;
struct hash *gHash = hashNew(0);
struct nameCount *gList = NULL, *gEl, *gNone, *gPost, *gRobot;
struct hash *dbHash = hashNew(8);
struct nameCount *dbList = NULL, *dbEl;

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
	struct logLine *ll = logLineParse(line, lf->fileName, lf->lineIx);
	if (ll != NULL)
	    {
	    if (sameString(ll->method, "GET") && startsWith("/cgi-bin/", ll->url))
		{
		struct hash *cgiHash;
		struct cgiVar *cgiList;
		char *cgiString = strchr(ll->url, '?');
		int cgiCount;
		if (cgiString == NULL)
		    cgiString = "";
		else
		    cgiString += 1;
		cgiString = cloneString(cgiString);
		if (!cgiParseInput(cgiString, &cgiHash, &cgiList))
		    {
		    if (verbose > 0)
		        printf("%s\n", ll->url);
		    continue;
	            }
		cgiCount = slCount(cgiList);
		if (startsWith("/cgi-bin/hgTracks", ll->url))
		    {
		    ++hgTracksTotal;
		    if (ll->referrer != NULL && stringIn("hgGateway", ll->referrer))
		        {
			struct cgiVar *cv = hashFindVal(cgiHash, "db");
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
		    if (isRobot(ll->ip, ll->program))
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
			if (verbose >= 2)
			    printf("%s\n", line);
			}
		    }
		else if (startsWith("/cgi-bin/hgc", ll->url))
		    {
		    struct cgiVar *cv = hashFindVal(cgiHash, "g");
		    struct nameCount *gEl;
		    ++hgcTotal;
		    if (isRobot(ll->ip, ll->program))
			gRobot->count += 1;
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
		else if (startsWith("/cgi-bin/hgGene", ll->url))
		    {
		    hgGeneTotal += 1;
		    if (isRobot(ll->ip, ll->program))
		        hgGeneRobot += 1;
		    }
		else if (startsWith("/cgi-bin/hgNear", ll->url))
		    {
		    hgNearTotal += 1;
		    if (isRobot(ll->ip, ll->program))
		        hgNearRobot += 1;
		    }
		else if (startsWith("/cgi-bin/hgText", ll->url))
		    {
		    hgTextTotal += 1;
		    if (isRobot(ll->ip, ll->program))
		        hgTextRobot += 1;
		    }
		else if (startsWith("/cgi-bin/hgBlat", ll->url))
		    {
		    hgBlatTotal += 1;
		    if (isRobot(ll->ip, ll->program))
		        hgBlatRobot += 1;
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
		else if (startsWith("/cgi-bin/hgGene", ll->url))
		    {
		    hgGeneTotal += 1;
		    if (isRobot(ll->ip, ll->program))
		        hgGeneRobot += 1;
		    }
		else if (startsWith("/cgi-bin/hgNear", ll->url))
		    {
		    hgNearTotal += 1;
		    if (isRobot(ll->ip, ll->program))
		        hgNearRobot += 1;
		    }
		else if (startsWith("/cgi-bin/hgText", ll->url))
		    {
		    hgTextTotal += 1;
		    if (isRobot(ll->ip, ll->program))
		        hgTextRobot += 1;
		    }
		else if (startsWith("/cgi-bin/hgBlat", ll->url))
		    {
		    hgBlatTotal += 1;
		    if (isRobot(ll->ip, ll->program))
		        hgBlatRobot += 1;
		    }
		}
	    logLineFree(&ll);
	    }
	}
    }
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

printf("hgBlat total %d, robot %d (%4.2f%%)\n", 
	hgBlatTotal, hgBlatRobot, 100.0*hgBlatRobot/hgBlatTotal);
printf("hgText total %d, robot %d (%4.2f%%)\n", 
	hgTextTotal, hgTextRobot, 100.0*hgTextRobot/hgTextTotal);
printf("hgGene total %d, robot %d (%4.2f%%)\n", 
	hgGeneTotal, hgGeneRobot, 100.0*hgGeneRobot/hgGeneTotal);
printf("hgNear total %d, robot %d (%4.2f%%)\n", 
	hgNearTotal, hgGeneRobot, 100.0*hgNearRobot/hgNearTotal);

printf("\n");
slSort(&gList, nameCountCmp);
printf("total hgc clicks: %d\n", hgcTotal);
for (gEl = gList; gEl != NULL; gEl = gEl->next)
    printf("%4.2f%% hgc %s: %d\n", 100.0 * gEl->count/hgcTotal, 
    	gEl->name, gEl->count);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
verbose = optionInt("verbose", verbose);
if (argc < 2)
    usage();
hgAccessCrawl(argc-1, argv+1);
return 0;
}
