/* htmlSanitizeTest - check that htmlSanitize keeps what it should and drops the rest. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "htmlSanitize.h"

static char *cases[] = {
/* a whole pasted document comes out as the article it was meant to be */
"<html><head><title>T</title><meta charset=\"utf-8\"></head><body class=\"x\">"
    "<h2>Head</h2><p>Text</p></body></html>",
/* script and style go, with their contents */
"<p>before</p><script>alert(1)</script><style>body{visibility:hidden}</style><p>after</p>",
/* a script that is never closed takes the rest with it */
"<p>before</p><script>if (a < b) alert(1)",
/* event handlers and class go, id and title stay */
"<div id=\"top\" class=\"warn\" title=\"t\" onclick=\"alert(1)\">text</div>",
/* an entity encoded scheme is still that scheme */
"<a href=\"&#106;avascript:alert(1)\">one</a> <a href=\"java&Tab;script:alert(1)\">two</a>",
/* ordinary links are left alone, and a new window does not get a handle on ours */
"<a href=\"https://genome.ucsc.edu\">u</a> <a href=\"#anchor\" target=\"_blank\">a</a>",
/* an image keeps its source, not its onerror */
"<img src=\"pic.png\" alt=\"a\" onerror=\"alert(1)\" width=\"20\">",
/* a frame survives only when it plays a video from a host we know */
"<iframe width=\"560\" src=\"https://www.youtube.com/embed/abc\"></iframe>"
    "<iframe src=\"https://example.com/x\">fallback</iframe>",
/* the style attribute is filtered a property at a time */
"<p style=\"color:red;behavior:url(#default#VML);text-align:center;position:fixed\">p</p>",
/* unknown elements lose their tag and keep their text */
"<o:p>word</o:p><vertebrates>more</vertebrates>",
/* tags left open are closed for us */
"<div><b>bold<p>para",
/* a stray quote inside an unquoted value does not swallow the page */
"<a href=https://example.com/x\\\">link text</a> and more text",
/* a form and everything in it goes */
"<form action=\"/x\"><input name=\"password\"><button>Log in</button></form><p>after</p>",
/* comments and doctypes go */
"<!DOCTYPE html><!-- <p>hidden</p> --><p>shown</p>",
/* a table keeps its shape */
"<table border=\"1\"><tr><td colspan=\"2\" bgcolor=\"#eee\">cell</td></tr></table>",
};

int main(int argc, char *argv[])
{
int i;
for (i = 0;  i < ArraySize(cases);  ++i)
    {
    char *clean = htmlSanitize(cases[i]);
    printf("in : %s\nout: %s\n", cases[i], clean);
    struct slName *removed = NULL, *el;
    freeMem(clean);
    clean = htmlSanitizeReport(cases[i], &removed);
    for (el = removed;  el != NULL;  el = el->next)
        printf("     (%s)\n", el->name);
    printf("\n");
    freeMem(clean);
    slFreeList(&removed);
    }
if (htmlSanitize(NULL) != NULL)
    errAbort("htmlSanitize(NULL) should be NULL");
return 0;
}
