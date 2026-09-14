/* cgiCookieTest - check how the cookie parser treats a malformed cookie. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "cheapcgi.h"
#include "errCatch.h"

static char *names[] = {"first", "second", "third"};

static char *cases[] = {
/* the ordinary shape, with and without the space a browser usually sends */
"first=1; second=2; third=3",
"first=1;second=2;third=3",
/* an empty pair, at the front, in the middle and at the end.  The middle one
 * names the next cookie "; second", so nothing looks it up and that cookie is
 * silently lost, which is the cookie twin of #38185 */
";first=1;second=2",
"first=1;;second=2;third=3",
"first=1;second=2;;",
/* a cookie with a name and no =value.  This aborts the CGI, and because the
 * browser sends the same cookie again on every request, the reader cannot get
 * a page back until they clear it by hand.  The cookie twin of #38335 */
"first=1; broken; second=2",
"first=1; second=2; broken",
"broken",
/* an empty value is a value, and is kept */
"first=; second=2",
/* nothing at all */
"",
";",
};

static void readAll(char *cookie, boolean skip)
/* Print what the three names read out of this cookie string. */
{
int j;
setenv("HTTP_COOKIE", cookie, 1);
cgiResetState();
cgiSkipMalformedPairs(skip);
printf("%-4s: ", skip ? "on" : "off");
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    for (j = 0;  j < ArraySize(names);  ++j)
	{
	char *val = findCookieData(names[j]);
	printf("[%s=%s]", names[j], val ? val : "(not found)");
	}
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    printf("aborted: %s", trimSpaces(errCatch->message->string));
errCatchFree(&errCatch);
printf("\n");
}

int main(int argc, char *argv[])
{
int i;
/* Each case is read twice, with hg.conf skipMalformedCgiPairs off and on, so
 * the test also pins that the gate leaves the old behavior alone. */
for (i = 0;  i < ArraySize(cases);  ++i)
    {
    printf("cookie: %s\n", cases[i]);
    readAll(cases[i], FALSE);
    readAll(cases[i], TRUE);
    printf("\n");
    }
return 0;
}
