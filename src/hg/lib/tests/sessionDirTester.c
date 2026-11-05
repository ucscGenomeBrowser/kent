/* sessionDirTester - check how a saved session's data directory is named.
 *
 * A saved session's custom tracks and region files are moved to a durable directory named
 * from the user name and the session name.  #10138 widened the session half of that name from
 * 8 hex characters to 10, because 8 characters of md5 is 4 billion values and the birthday
 * arithmetic over hundreds of thousands of sessions is not comfortable: two sessions landing
 * in one directory means one user's files sitting in another user's session.
 *
 * Widening a name that is already on disk is the risky half.  Directories written before the
 * change are named with the old width and their files are still in use, so the cleanup code
 * has to be able to name both, which is why the width is a parameter and not a constant in
 * the middle of the function.
 *
 * All of it is invisible.  A session whose directory is named differently does not report an
 * error, it comes back without its custom track.
 *
 * The property worth pinning is not the exact hash, it is that the short name is a PREFIX of
 * the long one.  That is what lets code holding the new name find the directory written under
 * the old one, and it is true only because both come from the same md5 truncated to different
 * lengths.  A change to either width, or to the hash, breaks it silently.
 *
 * refs #10138 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "errCatch.h"
#include "cart.h"
#include "sessionData.h"

#define DATA_DIR "/userdata/sessions"

static int errCount = 0;

static char *lastPart(char *path)
/* The session-hash component, which is the part the width applies to. */
{
char *slash = strrchr(path, '/');
return (slash == NULL) ? path : slash + 1;
}

static void widths()
/* The current width, the legacy width, and the relationship between them. */
{
char *user = "someUser";
char *session = "a session with a rather long name";

char *now = sessionDirFromNames(DATA_DIR, user, session);
char *legacy = sessionDirFromNamesHashLen(DATA_DIR, user, session,
                                          sessionDirHashLenLegacy);
printf("current: %s\n", now);
printf("legacy:  %s\n", legacy);
printf("  current width %d, legacy width %d\n",
       (int)strlen(lastPart(now)), (int)strlen(lastPart(legacy)));

if (strlen(lastPart(now)) != sessionDirHashLen
    || strlen(lastPart(legacy)) != sessionDirHashLenLegacy)
    {
    printf("  FAIL: a width does not match the header\n");
    ++errCount;
    }
if (!startsWith(lastPart(legacy), lastPart(now)))
    {
    printf("  FAIL: the old name is not a prefix of the new one, so cleanup "
           "cannot find an old directory\n");
    ++errCount;
    }
else
    printf("  the legacy name is a prefix of the current one\n");
}

static void shape()
/* The user name appears in the path as given, with a two character spreading directory in
 * front of it.  A mirror reading these by hand depends on both. */
{
char *dir = sessionDirFromNames(DATA_DIR, "someUser", "s");
printf("\nshape\n  %s\n", dir);
char *rest = dir + strlen(DATA_DIR) + 1;
char *slash = strchr(rest, '/');
printf("  spreading directory is %d characters\n", (int)(slash - rest));
if ((slash - rest) != 2)
    {
    printf("  FAIL: expected two\n");
    ++errCount;
    }
if (strstr(dir, "/someUser/") == NULL)
    {
    printf("  FAIL: the user name is not in the path as given\n");
    ++errCount;
    }

/* Two user names differing in one character must not share a directory. */
char *a = sessionDirFromNames(DATA_DIR, "userA", "s");
char *b = sessionDirFromNames(DATA_DIR, "userB", "s");
printf("  two users, same session name, same directory? %s\n",
       sameString(a, b) ? "YES -- FAIL" : "no");
if (sameString(a, b))
    ++errCount;
}

static void refused(char *what, char *dataDir, char *user, char *session, int hashLen)
/* Calls that must abort rather than invent a path. */
{
struct errCatch *errCatch = errCatchNew();
char *dir = NULL;
if (errCatchStart(errCatch))
    dir = sessionDirFromNamesHashLen(dataDir, user, session, hashLen);
errCatchEnd(errCatch);
if (errCatch->gotError)
    printf("  %-40s aborted\n", what);
else
    {
    printf("  %-40s returned %s -- FAIL\n", what, dir == NULL ? "NULL" : dir);
    ++errCount;
    }
errCatchFree(&errCatch);
}

static void edges()
{
printf("\nno directory configured\n");
char *none = sessionDirFromNames("", "someUser", "s");
printf("  empty sessionDataDir -> %s\n", none == NULL ? "NULL" : none);
if (none != NULL)
    ++errCount;

printf("\nrefused\n");
refused("a relative sessionDataDir", "userdata/sessions", "u", "s",
        sessionDirHashLen);
refused("hashLen 0", DATA_DIR, "u", "s", 0);
refused("hashLen 33, past the end of an md5", DATA_DIR, "u", "s", 33);
}

int main(int argc, char *argv[])
{
widths();
shape();
edges();
printf("\n%d failures\n", errCount);
return errCount == 0 ? 0 : 1;
}
