/* hgAccessCrawl - Go through Apache access log collecting stats on hgXXX programs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

static char const rcsid[] = "$Id: hgAccessCrawl.c,v 1.1 2003/12/22 13:24:55 kent Exp $";

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

void hgAccessCrawl(int logCount, char *logFiles[])
/* hgAccessCrawl - Go through Apache access log collecting stats on hgXXX programs. */
{
int i;
int total = 0;
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
int robot = 0;
int undisclosedOutsideSimple = 0;
int undisclosedOutsideWithCustom = 0;
int resetAll = 0;
int hideAll = 0;
int postScriptOutput = 0;
int addYourOwn = 0;

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
	    if (sameString(ll->method, "GET") && startsWith("/cgi-bin/hgTracks", ll->url))
	        {
		++total;
		if (stringIn("Submit=Submit", ll->url))
		    ++fromGateway;
		else if (stringIn("submit=jump", ll->url))
		    ++jump;
		else if (stringIn("submit=refresh", ll->url))
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
		    (stringIn("ss=../trash", ll->url) || 
		    stringIn("ss=http://genome.ucsc.edu/trash", ll->url)))
		    ++fromOtherBlat;
		else if (ll->referrer != NULL 
			&& !startsWith("http://genome.ucsc.edu", ll->referrer)
			&& !startsWith("http://genome.cse.ucsc.edu", ll->referrer))
		    ++fromOutside;
		else if (countChars(ll->url, '=') == 2 && stringIn("position=", ll->url) 
		    && stringIn("hgsid=", ll->url))
		    ++zoomInRuler;
		else if (countChars(ll->url, '=') == 3 && stringIn("position=", ll->url) 
		    && stringIn("hgsid=", ll->url))
		    ++gatewayMultiple;
		else if (stringIn("dummyEnterButton", ll->url))
		    {
		    if (stringIn("guideline", ll->url))
			++jump;
		    else
		        ++fromGateway;
		    }
		else if (startsWith("Java", ll->program))
		    ++robot;
		else if (sameString("joiner.stanford.edu", ll->ip))
		    ++robot;
		else if (ll->referrer == NULL && countChars(ll->url, '=') == 2 
		    && stringIn("db=", ll->url) && stringIn("position=", ll->url))
		    ++undisclosedOutsideSimple;
		else if (ll->referrer == NULL && countChars(ll->url, '=') == 2 
		    && stringIn("org=", ll->url) && stringIn("position=", ll->url))
		    ++undisclosedOutsideSimple;
		else if (ll->referrer == NULL && countChars(ll->url, '=') == 3 
		    && stringIn("db=", ll->url) && stringIn("position=", ll->url)
		    && stringIn("org=", ll->url))
		    ++undisclosedOutsideSimple;
		else if (ll->referrer == NULL && stringIn("hgt.customText=", ll->url))
		    ++undisclosedOutsideWithCustom;
		else if (stringIn("hgt.reset=", ll->url))
		    ++resetAll;
		else if (stringIn("hgt.hideAll=", ll->url))
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
		else if (stringIn("hgt.psOutput=on", ll->url))
		    ++postScriptOutput;
		else if (stringIn("Add+Your+Own", ll->url))
		    ++addYourOwn;
		else
		    {
		    ++other;
		    uglyf("%s\n", line);
		    }
		}
	    logLineFree(&ll);
	    }
	}
    }
printf("total: %d\n", total);
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
printf("robot: %d\n", robot);
printf("resetAll: %d\n", resetAll);
printf("hideAll: %d\n", hideAll);
printf("fromEncode: %d\n", fromEncode);
printf("postScriptOutput: %d\n", postScriptOutput);
printf("addYourOwn: %d\n", addYourOwn);
printf("other: %d\n", other);
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
