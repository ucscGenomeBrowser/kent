/* bedItemRgbTester - check which stanza wins when a track says both "itemRgb" and "color".
 *
 * bedItemRgb() decides whether a BED track draws its items in the colors the file carries or
 * in the one color the stanza names.  It lives in hg/cgilib/bedCart.c, so the test lives
 * here beside it.
 *
 * The rule has four steps and the order of the first three is the whole of #36212.  An
 * explicit "itemRgb off" wins, then an explicit "itemRgb on" wins, and only then does the
 * presence of a "color" setting turn item colors off by default.  Commit 88d620e6c82 folded
 * the first two tests together with the third, so a stanza saying both "itemRgb on" and
 * "color" -- which means "items from the file, labels from color" -- lost its item colors.
 *
 * Nothing about that is visible to a test that only looks at one setting at a time, which is
 * why the pairs below matter more than the singles: every single-setting case passed while
 * the bug was live.
 *
 * The last step reads hg.conf's alwaysItemRgb, so the makefile runs this twice, once with
 * that knob left alone and once with it off, and both answers are diffed.  A mirror that
 * turns it off must still get item colors from a stanza that explicitly asks for them.
 *
 * refs #36212 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "hgConfig.h"
#include "trackDb.h"
#include "bedCart.h"

static struct trackDb *tdbWith(char *settings)
/* A track with the given settings, as they would be spelled in a trackDb stanza. */
{
struct trackDb *tdb;
AllocVar(tdb);
tdb->track = cloneString("testTrack");
tdb->table = cloneString("testTrack");
tdb->type = cloneString("bed 9 .");
tdb->settings = cloneString(settings);
tdb->settingsHash = trackDbSettingsFromString(tdb, tdb->settings);
return tdb;
}

static void say(char *what, boolean got)
{
printf("  %-52s %s\n", what, got ? "item colors" : "one color");
}

static void singles()
/* One setting at a time.  Every one of these passed while #36212 was live. */
{
printf("one setting\n");
say("(nothing)", bedItemRgb(tdbWith("")));
say("itemRgb on", bedItemRgb(tdbWith("itemRgb on\n")));
say("itemRgb off", bedItemRgb(tdbWith("itemRgb off\n")));
say("color 255,0,0", bedItemRgb(tdbWith("color 255,0,0\n")));
}

static void pairs()
/* Both settings in one stanza, which is what #36212 is about. */
{
printf("\nboth settings\n");
say("itemRgb on  + color 255,0,0",
    bedItemRgb(tdbWith("itemRgb on\ncolor 255,0,0\n")));
say("itemRgb off + color 255,0,0",
    bedItemRgb(tdbWith("itemRgb off\ncolor 255,0,0\n")));
say("color 255,0,0 + itemRgb on  (other order)",
    bedItemRgb(tdbWith("color 255,0,0\nitemRgb on\n")));
}

static void inherited()
/* trackDbSettingClosestToHome walks up to the parent, so a child can be ruled by a color it
 * does not carry itself, and can overrule it with its own itemRgb. */
{
printf("\nfrom a parent\n");
struct trackDb *parent = tdbWith("color 255,0,0\n");
struct trackDb *child = tdbWith("");
child->parent = parent;
say("child of a stanza with color", bedItemRgb(child));

struct trackDb *saysOn = tdbWith("itemRgb on\n");
saysOn->parent = parent;
say("child says itemRgb on, parent says color", bedItemRgb(saysOn));
}

static void noTrack()
/* Callers reach this with no track at all, and the answer has to be the default rather than
 * a crash. */
{
printf("\nno track\n");
say("NULL", bedItemRgb(NULL));
}

int main(int argc, char *argv[])
{
printf("======== hg.conf alwaysItemRgb=%s\n",
       cfgOptionDefault("alwaysItemRgb", "(not set, defaults on)"));
singles();
pairs();
inherited();
noTrack();
printf("\n");
return 0;
}
