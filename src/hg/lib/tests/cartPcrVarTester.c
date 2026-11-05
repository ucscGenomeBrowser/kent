/* cartPcrVarTester - check which hgPcrResult_ cart variables survive a cart load.
 *
 * hgPcr saves a result in hgPcrResult_<db> as two trash file names and an optional target
 * name, and cart.c drops a value of that name that is not a pair of file names the server
 * made.  The variable is matched by its prefix, because <db> is any assembly or hub name.
 *
 * But hgPcrResult is also the name of the track that shows the result, so the track's own
 * settings begin with the same prefix.  When a user drags the track, hgTracks saves its place
 * in the image as hgPcrResult_imgOrd, and the cart threw that away on the next load.  The
 * track then went back to the bottom of the image on every zoom or scroll, however often it
 * was dragged up again (#38442).  hgPcrResult_targetStyle, a display setting, is the other
 * name with the prefix that is not a result.
 *
 * Nothing on the page says a value was dropped; the only trace is a line in the error log.
 * So the cases here are fed through cartParseOverHash(), the same parse a saved cart or
 * session goes through, and the output says which ones are kept.  No database is needed:
 * with no merge, the parse only touches cart->hash.
 *
 * refs #38442, refs #37623 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "cheapcgi.h"
#include "cart.h"
#include "portable.h"

static void check(char *var, char *val, char *what)
/* Load one variable into an empty cart and say whether it is still there. */
{
struct cart *cart;
AllocVar(cart);
cart->hash = newHash(0);
char contents[1024];
safef(contents, sizeof(contents), "%s=%s", var, cgiEncodeFull(val));
cartParseOverHash(cart, contents);
char *got = hashFindVal(cart->hash, var);
printf("  %-26s %-40s %s\n", var, what, (got != NULL) ? "kept" : "dropped");
hashFree(&cart->hash);
freez(&cart);
}

int main(int argc, char *argv[])
{
/* The trash directory is "../trash" in a CGI and something else from the command line, so
 * the result files are named under whatever this process calls the trash. */
char psl[512], primers[512], pair[1100], pairTarget[1200], badFirst[600], badSecond[600];
safef(psl, sizeof(psl), "%s/hgPcr/hgPcr_a.psl", trashDir());
safef(primers, sizeof(primers), "%s/hgPcr/hgPcr_a.txt", trashDir());
safef(pair, sizeof(pair), "%s %s", psl, primers);
safef(pairTarget, sizeof(pairTarget), "%s %s gencode", psl, primers);
safef(badFirst, sizeof(badFirst), "/etc/passwd %s", primers);
safef(badSecond, sizeof(badSecond), "%s /etc/passwd", psl);

printf("track settings that share the prefix are kept\n");
check("hgPcrResult_imgOrd", "3", "3");
check("hgPcrResult_targetStyle", "box", "box");

printf("a result pair of trash files is kept\n");
check("hgPcrResult_hg38", pair, "two trash files");
check("hgPcrResult_hg38", pairTarget, "two trash files and a target");

printf("a result that does not name two trash files is dropped\n");
check("hgPcrResult_hg38", "3", "3");
check("hgPcrResult_hg38", badFirst, "a psl file outside the trash");
check("hgPcrResult_hg38", badSecond, "a primer file outside the trash");

printf("the exclusions are exact names, not suffixes\n");
check("hgPcrResult_hg38_imgOrd", "/etc/passwd /etc/passwd", "two files outside the trash");
check("hgPcrResult_imgOrdx", "/etc/passwd /etc/passwd", "two files outside the trash");
return 0;
}
