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
/* an entity encoded scheme is still that scheme, however it is spelled */
"<a href=\"&#106;avascript:alert(1)\">one</a> <a href=\"java&Tab;script:alert(1)\">two</a>",
"<a href=\"&#0000000106;avascript:alert(1)\">three</a> <a href=\"&#106avascript:alert(1)\">four</a>",
"<a href=\"jav&#9;ascript:alert(1)\">five</a> <a href=\"java&colon;script:alert(1)\">six</a>",
/* an address that only looks encoded is left working */
"<a href=\"&#109;ailto:help&#64;riken.jp?subject=x\">write to us</a>",
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
/* tags left open are closed for us, and a slash does not close one of these */
"<div><b>bold<p>para",
"<div style=\"color:red\"/>the rest of the page is not inside that div",
/* a page built of tags that are never closed is not a way to make us work all day */
"<svg><svg><svg><svg>text",
/* a stray quote inside an unquoted value does not swallow the page */
"<a href=https://example.com/x\\\">link text</a> and more text",
/* a form and everything in it goes */
"<form action=\"/x\"><input name=\"password\"><button>Log in</button></form><p>after</p>",
/* comments and doctypes go */
"<!DOCTYPE html><!-- <p>hidden</p> --><p>shown</p>",
/* an id, and a name on an anchor, are renamed, and a link to one of them is renamed too */
"<h2 id=\"methods\">M</h2><a name=\"top\">t</a>"
    "<a href=\"#methods\">same page</a> <a href=\"other.html#methods\">other page</a>"
    "<a href=\"#\">to the top</a><div id=\"\">no name at all</div>",
/* a table keeps its shape */
"<table border=\"1\"><tr><td colspan=\"2\" bgcolor=\"#eee\">cell</td></tr></table>",
/* an entity in a style value cannot spell a property value we would not print */
"<p style=\"list-style:u&#114l(http://example.com/x.png);color:re&#100\">a</p>",
/* an ampersand the author meant still reads as one */
"<p style=\"font-family:'AT&T Sans'\">a</p>",
/* the same attribute twice is written once, the way a browser reads it */
"<img src=\"first.png\" src=\"second.png\" alt=\"a\" alt=\"b\">"
    "<a href=\"javascript:alert(1)\" href=\"https://example.com\">x</a>",
/* a quote that is never closed takes the rest of the page, and we say so */
"<p>real</p><p title=\"oops>never printed</p>",
/* a less than sign that starts no tag is text, and cannot eat a tag of ours */
"<div>a &lt; b</ <b>bold</b></div>",
/* a name we already renamed is not renamed again, on either side of the link */
"<h2 id=\"descPage-methods\">M</h2><a href=\"#descPage-methods\">jump</a>",
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
/* Running the filter over its own output has to leave it alone.  hgCustom hands the text
 * we returned back to us when a custom track is edited and saved again, so anything that
 * changes on a second pass changes a little more on every save.  Any case that is not
 * settled prints here, and the expected output is the record of which ones those are. */
for (i = 0;  i < ArraySize(cases);  ++i)
    {
    char *once = htmlSanitize(cases[i]);
    char *twice = htmlSanitize(once);
    if (differentString(once, twice))
        printf("not settled:\n once : %s\n twice: %s\n\n", once, twice);
    freeMem(once);
    freeMem(twice);
    }
if (htmlSanitize(NULL) != NULL)
    errAbort("htmlSanitize(NULL) should be NULL");
return 0;
}
