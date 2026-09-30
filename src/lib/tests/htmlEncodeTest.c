/* htmlEncodeTest - check the html encoders and tag strippers on edge-case input.
 *
 * htmlEncode and attributeEncode are called on item names and tooltips, and some items
 * have no name, so they must return an empty string for a NULL input.  refs #38226
 *
 * htmlTextStripTags and htmlTextStripJavascriptCssAndTags must return a terminated string.
 * They relied on zeroed memory past the text and allocated no byte for it, so a label with
 * no tags in it read whatever followed the allocation.  The careful allocator puts a marker
 * straight after every block, so under it that mistake shows up on every run instead of
 * only sometimes.  refs #37617 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "memalloc.h"
#include "htmshell.h"

static void showEncode(char *label, char *(*encode)(char *s), char *s)
/* Print what one encoder makes of s. */
{
char *out = encode(s);
printf("  %-16s %-12s -> ", label, (s == NULL) ? "NULL" : ((*s == 0) ? "(empty)" : s));
if (out == NULL)
    printf("NULL\n");
else
    printf("\"%s\" (%d chars)\n", out, (int)strlen(out));
freeMem(out);
}

static void showStrip(char *label, char *(*strip)(char *s), char *s)
/* Print what one tag stripper makes of s, with its length, so trailing bytes show. */
{
char *out = strip(s);
printf("  %-12s %-40s -> ", label, (s == NULL) ? "NULL" : ((*s == 0) ? "(empty)" : s));
if (out == NULL)
    printf("NULL\n");
else
    printf("\"%s\" (%d chars)\n", out, (int)strlen(out));
freeMem(out);
}

int main(int argc, char *argv[])
{
pushCarefulMemHandler(100000000);

printf("encoders\n");
showEncode("htmlEncode", htmlEncode, NULL);
showEncode("htmlEncode", htmlEncode, "");
showEncode("htmlEncode", htmlEncode, "a<b>&'\"");
showEncode("attributeEncode", attributeEncode, NULL);
showEncode("attributeEncode", attributeEncode, "");
showEncode("attributeEncode", attributeEncode, "a b'c");

printf("\ntag strippers\n");
char *inputs[] = {"plain label", "x", "<b>bold</b> label", "<i>all tag</i>",
                  "unclosed <b", "a<script>x()</script>b", "a<style>p{}</style>b"};
int i;
for (i = 0; i < ArraySize(inputs); i++)
    showStrip("tags", htmlTextStripTags, inputs[i]);
for (i = 0; i < ArraySize(inputs); i++)
    showStrip("js+css+tags", htmlTextStripJavascriptCssAndTags, inputs[i]);
showStrip("tags", htmlTextStripTags, "");
showStrip("js+css+tags", htmlTextStripJavascriptCssAndTags, "");
showStrip("tags", htmlTextStripTags, NULL);
showStrip("js+css+tags", htmlTextStripJavascriptCssAndTags, NULL);

carefulCheckHeap();
printf("\nheap markers intact\n");
return 0;
}
