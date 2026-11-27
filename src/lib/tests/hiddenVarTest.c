/* hiddenVarTest - check that values written by cgiMakeHiddenVar come back unchanged when the
 * page is parsed again by htmlPage, the parser the test robots use. refs #38798 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "cheapcgi.h"
#include "htmlPage.h"
#include "obscure.h"

static char *values[] = {
    "plain",
    "with space",
    "a&b",
    "x<y>z",
    "it's",
    "say \"hi\"",
    "a/b/c",
    "hgTracks?db=hg38&position=chr1:1,000-2,000",
    "ENST00000456328.2",
    "100%20off",
    "tab\there",
    "caf\xc3\xa9",
    "",
};

/* A UTF-8 value must reach the page as the same bytes, not as one entity per byte. The
 * parser decodes entities a byte at a time, so the comparison alone cannot see that. */
static char *utf8Value = "caf\xc3\xa9";

void usage()
/* Explain usage and exit */
{
errAbort(
"hiddenVarTest - write hidden form values, or read them back and compare\n"
"usage:\n"
"   hiddenVarTest write > page.html\n"
"   hiddenVarTest read page.html\n");
}

static void writePage()
/* Print a page with one hidden input per test value, plus one for a NULL value. */
{
int i;
printf("<HTML><BODY><FORM ACTION=\"hiddenVarTest\" METHOD=GET>\n");
for (i = 0; i < ArraySize(values); ++i)
    {
    char name[16];
    safef(name, sizeof name, "v%d", i);
    cgiMakeHiddenVar(name, values[i]);
    }
cgiMakeHiddenVar("vNull", NULL);
printf("</FORM></BODY></HTML>\n");
}

static void checkVar(struct htmlForm *form, char *name, char *expected)
/* Print what the parser read for name, and whether it is what was written. */
{
struct htmlFormVar *var = htmlFormVarGet(form, name);
char *got = (var == NULL) ? "(missing)" : var->curVal;
if (got == NULL)
    got = "";
printf("%s\t[%s]\t%s\n", name, got, sameString(got, expected) ? "same" : "DIFFERENT");
}

static void readPage(char *fileName)
/* Parse the page and compare each hidden value with the one that was written. */
{
char *html;
readInGulp(fileName, &html, NULL);
struct htmlPage *page = htmlPageParseNoHead(fileName, html);
if (page->forms == NULL)
    errAbort("no form found in %s", fileName);
int i;
for (i = 0; i < ArraySize(values); ++i)
    {
    char name[16];
    safef(name, sizeof name, "v%d", i);
    checkVar(page->forms, name, values[i]);
    }
checkVar(page->forms, "vNull", "");
printf("utf8Raw\t[%s]\t%s\n", utf8Value, strstr(html, utf8Value) ? "same" : "DIFFERENT");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc == 2 && sameString(argv[1], "write"))
    writePage();
else if (argc == 3 && sameString(argv[1], "read"))
    readPage(argv[2]);
else
    usage();
return 0;
}
