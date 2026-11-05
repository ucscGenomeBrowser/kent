/* trashDirTester - check which paths the cart will accept in a variable that names a file.
 *
 * A handful of cart variables hold the name of a file the server made for the user, and a CGI
 * reads one back out of the cart and opens it.  hg/lib/cart.c screens those on the way in with
 * the functions here, so this is the check that decides whether a saved session still works and
 * whether a hand-written path is refused.  refs #37623.
 *
 * Both halves of it are invisible.  A path wrongly refused does not produce an error page; the
 * session simply comes back without its custom track or its region list.  That is #38303: a
 * check shipped in v503 discarded the saved region list from 583 sessions on the RR and 66 on
 * euro, and neither hgwdev, nor code review, nor hgwbeta saw it, because the corpus that shows
 * it is only on the production central.  A path wrongly accepted is worse and quieter still.
 *
 * The rules that fix pins, each of which cost a bug or a review round to arrive at:
 *
 *   - Only the configured DIRECTORY is symlink-resolved, never the path from the cart.  A trash
 *     file is deliberately a symlink into session storage, so resolving the value would defeat
 *     the check entirely.
 *   - The resolved spelling of the directory is accepted as well as the configured one, because
 *     /userdata on the RR is a symlink to /shared/userdata and sessionData.c stores whichever
 *     spelling it saw.  Saved sessions hold both.
 *   - Only an ABSOLUTE directory is resolved.  A relative one would be resolved against the
 *     process's working directory, so the answer would move with the caller, and the trash
 *     directory is exactly such a relative path ("../trash").
 *   - One direction only: a directory configured as the already-resolved spelling does not
 *     accept a path written through the symlink.  That is a deliberate limit, not an oversight,
 *     and it is here so a change to it is a decision rather than an accident.
 *
 * One config per run, because hgConfig caches what it read, so the makefile runs this three
 * times with three spellings of sessionDataDir and diffs the three together.
 *
 * refs #38303, refs #37623 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hgConfig.h"
#include "trashDir.h"

static void usage()
/* Explain usage and exit. */
{
errAbort(
  "trashDirTester - check which file paths the cart accepts\n"
  "usage:\n"
  "   trashDirTester label linkDir realDir\n"
  "label     what the hg.conf in HGDB_CONF sets sessionDataDir to, for the report\n"
  "linkDir   a directory reached through a symlink\n"
  "realDir   the directory that symlink points at\n");
}

static void say(char *what, boolean got)
{
printf("  %-58s %s\n", what, got ? "accept" : "refuse");
}

static void pure()
/* pathIsUnderDir on its own, with no configuration in the way.  These are the cases that
 * decide whether a sibling directory or a climb out of one is mistaken for a file inside it. */
{
printf("pathIsUnderDir\n");
say("/a/b        /a/b/c.bed", pathIsUnderDir("/a/b", "/a/b/c.bed"));
say("/a/b/       /a/b/c.bed  (trailing slash on the dir)",
    pathIsUnderDir("/a/b/", "/a/b/c.bed"));
say("/a/b        /a/bb/c.bed (sibling that starts the same)",
    pathIsUnderDir("/a/b", "/a/bb/c.bed"));
say("/a/b        /a/b        (the directory itself, no file)",
    pathIsUnderDir("/a/b", "/a/b"));
say("/a/b        /a/b/       (nothing after the slash)",
    pathIsUnderDir("/a/b", "/a/b/"));
say("/a/b        /a/b/../../etc/passwd",
    pathIsUnderDir("/a/b", "/a/b/../../etc/passwd"));
say("/a/b        /a/b/x/../y.bed (a .. that stays inside)",
    pathIsUnderDir("/a/b", "/a/b/x/../y.bed"));
say("/a/b        /a/b/..x/y.bed  (a name that begins with ..)",
    pathIsUnderDir("/a/b", "/a/b/..x/y.bed"));
say("(empty dir) /a/b/c.bed", pathIsUnderDir("", "/a/b/c.bed"));
say("/a/b        (empty path)", pathIsUnderDir("/a/b", ""));
say("/           /etc/passwd  (a dir of just a slash)",
    pathIsUnderDir("/", "/etc/passwd"));
}

static void urls()
/* isRemoteUrl exists because hasProtocol() in net.c is a bare search for "://" and would
 * hand file:///etc/passwd to code that then reads it. */
{
printf("\nisRemoteUrl\n");
say("http://example.org/a.bb", isRemoteUrl("http://example.org/a.bb"));
say("https://example.org/a.bb", isRemoteUrl("https://example.org/a.bb"));
say("ftp://example.org/a.bb", isRemoteUrl("ftp://example.org/a.bb"));
say("file:///etc/passwd", isRemoteUrl("file:///etc/passwd"));
say("gopher://example.org/a.bb", isRemoteUrl("gopher://example.org/a.bb"));
say("/etc/passwd", isRemoteUrl("/etc/passwd"));
}

static void configured(char *label, char *linkDir, char *realDir)
/* The session-data directory as this run's hg.conf spells it.  Which of these two spellings
 * is accepted is the whole of #38303. */
{
/* The paths are built from the fixture, so they carry this machine's build directory.  Only
 * the symbolic name is printed: an expected/ file with an absolute path in it would pass
 * here and fail in every other checkout. */
char path[PATH_LEN];
printf("\nisTrashOrSessionDataPath, sessionDataDir is %s\n", label);

safef(path, sizeof path, "%s/hgt/user.bed", linkDir);
say("<link>/hgt/user.bed   through the symlink",
    isTrashOrSessionDataPath(path));

safef(path, sizeof path, "%s/hgt/user.bed", realDir);
say("<real>/hgt/user.bed   the resolved spelling",
    isTrashOrSessionDataPath(path));

say("/etc/passwd", isTrashOrSessionDataPath("/etc/passwd"));
safef(path, sizeof path, "%s/../../../etc/passwd", linkDir);
say("<link>/../../../etc/passwd", isTrashOrSessionDataPath(path));
say("http://example.org/a.bb  (a URL is not a file path)",
    isTrashOrSessionDataPath("http://example.org/a.bb"));

/* The wider door, which adds the per-feature data directories and then URLs.  A URL has to
 * come back accepted from the last one only, since the variables that use it hold either. */
safef(path, sizeof path, "%s/hgt/user.bed", linkDir);
printf("\nthe other two doors, for <link>/hgt/user.bed\n");
say("isServerUserFilePath", isServerUserFilePath(path));
say("isServerUserFileOrUrl", isServerUserFileOrUrl(path));
say("isServerUserFilePath   http://example.org/a.bb",
    isServerUserFilePath("http://example.org/a.bb"));
say("isServerUserFileOrUrl  http://example.org/a.bb",
    isServerUserFileOrUrl("http://example.org/a.bb"));
}

int main(int argc, char *argv[])
{
if (argc != 4)
    usage();
char *label = argv[1], *linkDir = argv[2], *realDir = argv[3];
printf("======== sessionDataDir spelled %s\n", label);
pure();
urls();
configured(label, linkDir, realDir);
printf("\n");
return 0;
}
