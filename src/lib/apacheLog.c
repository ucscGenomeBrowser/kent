/* apacheLog - stuff to parse out apache web server logs, currently
 * just the access log. */

#include "common.h"
#include "obscure.h"
#include "apacheLog.h"


void apacheAccessLogFree(struct apacheAccessLog **pLl)
/* Free up apacheAccessLog. */
{
struct apacheAccessLog *ll = *pLl;
if (ll != NULL)
    {
    freeMem(ll->buf);
    freez(pLl);
    }
}


static void badFormat(struct apacheAccessLog **pLl, char *line, char *fileName, 
	int lineIx, char *message)
/* Complain about format if verbose flag is on.  Free up
 * *pLl */
{
if (verboseLevel()  > 1)
    {
    if (fileName != NULL)
	warn("%s line %d: %s", fileName, lineIx, message);
    else
	warn("%s", message);
    }
}

static void unterminatedQuote(struct apacheAccessLog **pLl, char *line, 
	char *fileName, int lineIx)
/* Complain about unterminated quote. */
{
badFormat(pLl, line, fileName, lineIx, 
	"missing closing quote");
}

static void shortLine(struct apacheAccessLog **pLl, char *line, 
	char *fileName, int lineIx)
/* Complain about short line. */
{
badFormat(pLl, line, fileName, lineIx, 
	"short line");
}

static void badTimeStamp(struct apacheAccessLog **pLl, char *line, 
	char *fileName, int lineIx)
/* Complain about bad time stamp. */
{
badFormat(pLl, line, fileName, lineIx, 
	"bad time stamp");
}

time_t apacheAccessLogTimeToTick(char *timeStamp)
/* Convert something like 27/Aug/2009:09:25:32 to Unix timestamp (seconds since 1970).
 * On error returns zero. */

{
struct tm tm;
ZeroVar(&tm);
if (strptime(timeStamp, "%d/%b/%Y:%T", &tm) != NULL)
    return mktime(&tm);
else
    return 0;
}

struct apacheAccessLog *apacheAccessLogParse(char *line, 
	char *fileName, int lineIx)
/* Return a apacheAccessLog from line.  Return NULL if there's a parsing 
 * problem, but don't abort. */
{
struct apacheAccessLog *ll;
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

/* Parse out bracket enclosed timeStamp and time zone. */
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
ll->timeZone = nextWord(&s);

/* Convert time stamp to Unix tick. */
ll->tick = apacheAccessLogTimeToTick(ll->timeStamp);


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

/* Parse out elapsed time if it's there. */
ll->runTime = -1;		/* Marker for unset. */
char *runTime = nextWord(&e);
char *label = nextWord(&e);
if (label != NULL)
    {
    if (!isdigit(runTime[0]))
        {
	badFormat(&ll, line, fileName, lineIx, "non-numerical seconds");
	return NULL;
	}
    int x = atoi(runTime);
    if (sameString(label, "seconds"))
        ll->runTime = x*1000;
    else if (sameString(label, "microseconds"))
        ll->runTime = x/1000;
    }

return ll;
}

int apacheAccessLogCmpTick(const void *va, const void *vb)
/* Compare items to sort by tick (which tracks timestamp) */
{
const struct apacheAccessLog *a = *((struct apacheAccessLog **)va);
const struct apacheAccessLog *b = *((struct apacheAccessLog **)vb);
if (a->tick < b->tick)
    return -1;
else if (a->tick == b->tick)
    return 0;
else
    return 1;
}

