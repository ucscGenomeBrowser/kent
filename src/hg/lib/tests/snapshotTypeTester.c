/* snapshotTypeTester - check the fast reader of a session's snapshotType against raFromString.
 *
 * A saved session is a snapshot when its settings carry a "snapshotType" line, and the My
 * Sessions listings hide those rows.  The obvious way to read one tag is raFromString(), which
 * parses the whole thing into a hash; #38313 replaced that with a walk over the lines, because
 * the question is asked once per row in a listing and the hash costs about 600ns and three
 * allocations per session that has any settings at all, against about 30ns and none.
 *
 * A hand written parser that has to agree with a general one is the shape of bug that is found
 * a year later, so this is a differential test: every case below is read both ways and the two
 * answers are printed side by side.  The claim in the code is "same line semantics as
 * raFromString", and that claim is what is being checked, not a list of answers somebody wrote
 * down once.  When the two disagree the line says DIFFER and the run fails.
 *
 * Nothing here is visible either.  Reading the tag wrongly does not produce an error: a
 * snapshot is listed as though it were an ordinary saved session, or an ordinary session
 * vanishes from the listing, and both look like something the user did.
 *
 * One case does NOT agree, and it is recorded here rather than hidden.  Given two
 * snapshotType lines in one settings string, the walk returns the first and raFromString
 * returns the last, because a hash overwrites.  Real settings carry the tag once, so nothing
 * depends on it today, but it is a difference in the semantics the code says it copies, and
 * the day somebody writes the tag twice the two readers will disagree about what kind of
 * session it is.  It is listed below as a known difference, not as a failure.
 *
 * The prefix case is the one to keep.  "snapshotTypeExtra view" begins with the whole tag, so a
 * reader that only checks startsWith matches it and returns "Extra view", while raFromString
 * takes the first word as the tag and never matches at all.  The delimiter test in the code is
 * the only thing standing between those two answers.
 *
 * refs #38313 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "ra.h"
#include "cart.h"
#include "snapshotSession.h"

static int errCount = 0;

static char *orNone(char *s)
{
return (s == NULL) ? "(none)" : s;
}

static void bothMaybeDiffer(char *what, char *settings, boolean knownDiff)
/* Read the tag the fast way and the raFromString way, and print both.  knownDiff marks the
 * one case where they are known to differ, so it is visible without being a failure. */
{
char *fast = snapshotTypeFromSettings(settings);

struct hash *raHash = raFromString(settings);
char *slow = (raHash == NULL) ? NULL : hashFindVal(raHash, snapshotTypeSetting);
/* raFromString keeps an empty value as an empty string; the walk returns NULL for one, and
 * that difference is deliberate, since an empty type names no feature. */
if (slow != NULL && slow[0] == '\0')
    slow = NULL;

boolean agree = sameOk(fast, slow);
printf("  %-38s fast=%-12s ra=%-12s %s\n", what, orNone(fast), orNone(slow),
       agree ? "" : (knownDiff ? "differ, known" : "DIFFER"));
if (!agree && !knownDiff)
    ++errCount;
if (agree && knownDiff)
    {
    printf("    this case used to differ and no longer does; update the header\n");
    ++errCount;
    }
hashFree(&raHash);
}

static void both(char *what, char *settings)
/* The two readers must agree. */
{
bothMaybeDiffer(what, settings, FALSE);
}

int main(int argc, char *argv[])
{
printf("the ordinary cases\n");
both("the tag alone", "snapshotType blat\n");
both("with other tags around it",
     "firstUse 2026-01-01\nsnapshotType blat\nuseCount 3\n");
both("no trailing newline", "snapshotType blat");
both("a tab after the tag", "snapshotType\tblat\n");
both("extra blanks after the tag", "snapshotType    blat\n");

printf("\nthe cases that must NOT match\n");
both("a longer tag that starts the same", "snapshotTypeExtra view\n");
both("the tag as a value", "someTag snapshotType blat\n");
both("no such tag", "firstUse 2026-01-01\nuseCount 3\n");
both("the tag with nothing after it", "snapshotType\n");

printf("\nthe edges\n");
both("empty settings", "");
both("only a newline", "\n");
both("leading blank lines", "\n\nsnapshotType blat\n");
both("leading spaces on the line", "   snapshotType blat\n");
bothMaybeDiffer("two of them: walk takes the first, ra the last",
                "snapshotType blat\nsnapshotType view\n", TRUE);

printf("\nNULL settings: fast=%s\n", orNone(snapshotTypeFromSettings(NULL)));
printf("\n%d disagreements\n", errCount);
return errCount == 0 ? 0 : 1;
}
