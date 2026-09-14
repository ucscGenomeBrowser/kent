/* cgiParseTest - check how the two cgi query string parsers treat a malformed pair. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "cheapcgi.h"
#include "errCatch.h"
#include "hash.h"

static char *cases[] = {
/* the ordinary shape, and the same thing with semicolons, which DAS clients send */
"db=hg38&position=chr1:1-1000",
"a=1;b=2;c=3",
/* mixed separators: the ampersand still ends the pair, so the semicolon stays in the value */
"a=1;b=2&c=3",
/* an empty pair, at the front, in the middle and at the end.  The middle one used to
 * name the next variable "&position", so nothing looked it up, and the last one used
 * to abort the whole request.  refs #38185 */
"&db=hg38&position=chr1:1-1000",
"db=hg38&&position=chr1:1-1000",
"db=hg38&position=chr1:1-1000&&",
"&&;&db=hg38",
/* a pair with a name but no =value, in the same three places.  The trailing one used to
 * abort, and the other two used to run into the pair after them and take its value,
 * losing that variable without a word.  refs #38335 */
"g-catV2&db=hg38",
"db=hg38&i&position=chr1:1-1000",
"db=hg38&g-catV2",
"g-catV2",
/* an empty value is a value, and is kept */
"a=1&b=&c=3",
"=v",
"a=1&=&b=2",
/* nothing at all */
"",
"&",
/* an equals sign in a value belongs to the value */
"hgt.customText=track name=one",
/* the escapes a value can carry, including an ampersand that is not a separator */
"position=chr1%3A1-1000&name=a+b&other=x%26y",
};

static void showParseNext(char *in)
/* Print what cgiParseNext makes of in. */
{
printf("next: ");
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    char *s = cloneString(in);
    char *pt = s, *var, *val;
    while (cgiParseNext(&pt, &var, &val))
        printf("[%s=%s]", var, val);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    printf("aborted: %s", trimSpaces(errCatch->message->string));
errCatchFree(&errCatch);
printf("\n");
}

static void showParseInput(char *in)
/* Print what cgiParseInputAbort makes of in. */
{
printf("hash: ");
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    char *s = cloneString(in);
    struct hash *hash = NULL;
    struct cgiVar *list = NULL, *var;
    cgiParseInputAbort(s, &hash, &list);
    for (var = list; var != NULL; var = var->next)
        printf("[%s=%s]", var->name, var->val);
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
for (i = 0;  i < ArraySize(cases);  ++i)
    {
    printf("in  : %s\n", cases[i]);
    showParseNext(cases[i]);
    showParseInput(cases[i]);
    printf("\n");
    }
return 0;
}
