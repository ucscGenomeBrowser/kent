/* apacheLog - stuff to parse out apache web server logs, currently
 * just the access log. */

#include "common.h"
#include "obscure.h"
#include "apacheLog.h"

static char const rcsid[] = "$Id: apacheLog.c,v 1.1 2005/09/03 02:07:40 kent Exp $";

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

