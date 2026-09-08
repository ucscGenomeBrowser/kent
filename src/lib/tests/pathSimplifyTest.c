/* pathSimplifyTest - exercise path canonicalization in osunix.
 *
 * Two parts.  The first prints the answer for a list of named paths, which
 * the makefile diffs against expected/ so a change in behavior shows up as a
 * diff.  The second enumerates every short path over a small alphabet and
 * compares eatExcessDotsInPath against a reference written a different way,
 * which catches a wrong answer rather than merely a changed one.  refs #37263
 */
#include "common.h"
#include "portable.h"

static char *namedCases[] = {
/* Paths that should come back unchanged. */
    "", "a", "a/b", "/", "/a/b",
/* Trailing slash. */
    "a/", "a/b/", "/a/",
/* Single dots. */
    ".", "./", "./a", "a/.", "a/./b", "/a/./b",
/* Double dots consuming the component before them. */
    "a/..", "a/../", "a/../b", "/a/..", "/a/../b",
    "a/b/..", "a/b/../c", "a/../b/../c", "a/../b/../c/..",
    "/a/../b/../c/..", "x/./../y",
/* A double dot that climbs out of a relative path has to survive, or a
 * caller cannot tell "still inside" from "escaped". */
    "..", "../", "../..", "../a", "../../a", "a/../../b",
    "h/../../../etc/passwd",
/* A double dot at the root of an absolute path is dropped, as realpath(3)
 * does, so it can never climb out. */
    "/..", "/../a", "/a/../../b", "/../../a",
/* Repeated slashes. */
    "//", "//../", "a//b///c", "a/b///",
    };

static void reportNamed()
/* Print the canonical form of each named path. */
{
int i;
for (i=0; i<ArraySize(namedCases); ++i)
    {
    char buf[PATH_LEN];
    safecpy(buf, sizeof(buf), namedCases[i]);
    eatSlashSlashInPath(buf);
    eatExcessDotsInPath(buf);
    printf("%-24s -> %s\n", namedCases[i], buf);
    }
}

static void refSimplify(char *path, char *out, int outSize)
/* Reference version of eatSlashSlashInPath plus eatExcessDotsInPath, written
 * as a stack of components rather than as two pointers walking one buffer, so
 * that a shared mistake is unlikely.  Not fast, and not meant to be. */
{
boolean absolute = (path[0] == '/');
boolean wasEmpty = (path[0] == 0);
char *comp[PATH_LEN];
int compCount = 0;

/* Split on '/', which drops empty components and so also collapses "//". */
char work[PATH_LEN];
safecpy(work, sizeof(work), path);
char *s = work;
while (s != NULL && *s != 0)
    {
    char *e = strchr(s, '/');
    if (e != NULL)
        *e++ = 0;
    if (s[0] == 0 || sameString(s, "."))
        ;                               /* empty or single dot: drop it */
    else if (sameString(s, ".."))
        {
        if (compCount > 0 && !sameString(comp[compCount-1], ".."))
            --compCount;                /* consume the component before it */
        else if (!absolute)
            comp[compCount++] = s;      /* relative: it has to survive */
        /* absolute at the root: drop it, since /.. is / */
        }
    else
        comp[compCount++] = s;
    s = e;
    }

/* Join back up. */
out[0] = 0;
if (absolute)
    safecat(out, outSize, "/");
int i;
for (i=0; i<compCount; ++i)
    {
    if (i > 0)
        safecat(out, outSize, "/");
    safecat(out, outSize, comp[i]);
    }
if (!absolute && compCount == 0 && !wasEmpty)
    safecat(out, outSize, ".");
}

static void enumerate(char *alphabet, int maxLen, int *retCases, int *retBad)
/* Compare the two versions on every string over alphabet up to maxLen. */
{
int alphaSize = strlen(alphabet);
int len;
for (len=0; len <= maxLen; ++len)
    {
    /* Odometer over alphaSize^len strings. */
    int digit[16];
    int i;
    for (i=0; i<len; ++i)
        digit[i] = 0;
    for (;;)
        {
        char in[32], got[32], want[32];
        for (i=0; i<len; ++i)
            in[i] = alphabet[digit[i]];
        in[len] = 0;
        safecpy(got, sizeof(got), in);
        eatSlashSlashInPath(got);
        eatExcessDotsInPath(got);
        refSimplify(in, want, sizeof(want));
        *retCases += 1;
        if (!sameString(got, want))
            {
            *retBad += 1;
            if (*retBad <= 10)
                printf("MISMATCH in=%s got=%s want=%s\n", in, got, want);
            }
        /* Next odometer reading. */
        for (i = len-1; i >= 0; --i)
            {
            if (++digit[i] < alphaSize)
                break;
            digit[i] = 0;
            }
        if (i < 0)
            break;
        }
    }
}

int main(int argc, char *argv[])
{
reportNamed();

int cases = 0, bad = 0;
enumerate("/.a", 9, &cases, &bad);
enumerate("/.ab", 7, &cases, &bad);
printf("enumerated %d paths, %d disagree with the reference\n", cases, bad);
return 0;
}
