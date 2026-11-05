/* archiveTrackLineTest - check the priority the custom track archive writes on each track
 * line, which is the only thing that keeps the session's track order in the archive. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "cart.h"
#include "cheapcgi.h"

char *archiveTrackLine(struct cart *cart, char *track, double tdbPriority, char *trackLine);

/* Three symbols backup.c wants from hgSession.c, which this test does not link. */
struct cart *cart = NULL;
char *database = NULL;

char *cgiDecodeClone(char *encStr)
/* Same as hgSession.c's. */
{
char *s = cloneString(encStr);
cgiDecode(encStr, s, strlen(encStr));
return s;
}

struct testCase
/* One track line, the tdb priority, and the cart variables in force. */
    {
    char *trackLine;
    double tdbPriority;
    char *cartVars;	/* var=value pairs separated by spaces, or "" */
    };

static struct testCase cases[] = {
/* nothing says where the track goes, so the line is written as it was */
{"track name=ct1", 0, ""},
/* the priority the browser gave the track when it loaded it */
{"track name=ct1", 5.5, ""},
/* a priority set on the track's configuration page beats that one */
{"track name=ct1", 5.5, "ct1.priority=12"},
/* a drag-and-drop reorder sets only _imgOrd, and hgTracks draws by it ahead of the
 * priority, so the archive has to write it.  It used to write 12 here.  refs #38497 */
{"track name=ct1", 5.5, "ct1.priority=12 ct1_imgOrd=3"},
{"track name=ct1", 5.5, "ct1_imgOrd=3"},
/* hgTracks ignores an _imgOrd from 10000 (IMG_ORDERTOP) on, and so does the archive */
{"track name=ct1", 5.5, "ct1.priority=12 ct1_imgOrd=10000"},
{"track name=ct1", 5.5, "ct1_imgOrd=0"},
/* another track's position is not this one's */
{"track name=ct1", 5.5, "ct2_imgOrd=3"},
/* a priority the user wrote on the track line is left alone */
{"track name=ct1 priority=7", 5.5, "ct1_imgOrd=3"},
/* a bare track line */
{"track", 0, "ct1_imgOrd=3"},
};

static struct cart *cartFromVars(char *vars)
/* Make a cart holding only these var=value pairs.  Nothing here reads more of a cart
 * than its hash. */
{
struct cart *c;
AllocVar(c);
c->hash = hashNew(0);
char *copy = cloneString(vars);
char *word, *s = copy;
while ((word = nextWord(&s)) != NULL)
    {
    char *eq = strchr(word, '=');
    *eq = 0;
    hashAdd(c->hash, word, cloneString(eq + 1));
    }
freeMem(copy);
return c;
}

int main(int argc, char *argv[])
{
int i;
for (i = 0;  i < ArraySize(cases);  ++i)
    {
    struct testCase *tc = &cases[i];
    struct cart *c = cartFromVars(tc->cartVars);
    char *line = archiveTrackLine(c, "ct1", tc->tdbPriority, tc->trackLine);
    printf("in  : %s | tdb priority %g | cart: %s\n", tc->trackLine, tc->tdbPriority,
           tc->cartVars);
    printf("out : %s\n\n", line);
    freeMem(line);
    }
return 0;
}
