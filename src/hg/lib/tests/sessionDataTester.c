/* sessionDataTester - check that sessionDataSaveTrashFile hands back memory its callers
 * can release with freeMem.
 *
 * Three places release that path through kent's own handler stack: two in sessionData.c
 * itself and freez(&durable) in snapshotSession.c.  All three are reached only from
 * hgSession, which installs no memory handler, so nothing fails today.  (hgPhyloPlace calls
 * the function too, but drops what it returns.)  hgc, hgTables, hgLogin and hgLinkIn install
 * the careful handler in main(), and hgVai does on a private host, so a caller added in any
 * of those would die on a pointer that came from the system malloc.  This test installs the
 * careful handler itself, so a return to that arrangement fails here instead of in a CGI.
 * refs #38318 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include <unistd.h>
#include "common.h"
#include "memalloc.h"
#include "cart.h"   /* for struct cart, named in a prototype in sessionData.h */
#include "portable.h"
#include "sessionData.h"

/* Large enough that the probe allocation below never reaches it, so the only way to exceed it
 * is the underflow this test is watching for: carefulAlloced is a size_t, and freeing memory
 * the careful handler never allocated subtracts a garbage size from it and wraps it around. */
#define carefulAllocLimit (100*1024*1024)

/* Unused by the code paths under test, which all return before a session directory is needed. */
#define unusedSessionDir "/unused/sessionDataDir"

static void usage()
/* Explain usage and exit. */
{
errAbort(
  "sessionDataTester - test the memory contract of sessionDataSaveTrashFile\n"
  "usage:\n"
  "   sessionDataTester workDir\n"
  "workDir is created if it does not exist, and the test fixture is written there.\n");
}

static void makeFixture(char *workDir, char *relLinkPath, char *absLinkPath, int pathSize)
/* Write workDir/real/target.txt, then two symlinks to it in workDir/link: one spelled
 * relatively, the way the trashCleaner scripts leave them, and one spelled absolutely. */
{
char realDir[PATH_LEN], linkDir[PATH_LEN], targetPath[PATH_LEN];
/* The absolute link has to be spelled absolutely whatever the caller passed in, since that
 * spelling is what sends it down the other branch. */
if (workDir[0] == '/')
    {
    safef(realDir, sizeof realDir, "%s/real", workDir);
    safef(linkDir, sizeof linkDir, "%s/link", workDir);
    }
else
    {
    safef(realDir, sizeof realDir, "%s/%s/real", getCurrentDir(), workDir);
    safef(linkDir, sizeof linkDir, "%s/%s/link", getCurrentDir(), workDir);
    }
makeDirsOnPath(realDir);
makeDirsOnPath(linkDir);

safef(targetPath, sizeof targetPath, "%s/target.txt", realDir);
FILE *f = mustOpen(targetPath, "w");
char *contents = "session data\n";
mustWrite(f, contents, strlen(contents));
carefulClose(&f);

/* A leftover fixture from a failed run would make symlink fail with EEXIST. */
safef(relLinkPath, pathSize, "%s/relative.txt", linkDir);
unlink(relLinkPath);
if (symlink("../real/target.txt", relLinkPath) != 0)
    errnoAbort("makeFixture: symlink to '%s' failed", relLinkPath);

safef(absLinkPath, pathSize, "%s/absolute.txt", linkDir);
unlink(absLinkPath);
if (symlink(targetPath, absLinkPath) != 0)
    errnoAbort("makeFixture: symlink to '%s' failed", absLinkPath);
}

static void checkResult(char *what, char *newPath, char *expectedSuffix, int blocksBefore)
/* Check one return value: it names the file we expect, freeMem takes it, the heap survives a
 * later allocation, and the function kept nothing of its own. */
{
if (newPath == NULL)
    errAbort("%s: expected a path, got NULL", what);
if (!endsWith(newPath, expectedSuffix))
    errAbort("%s: expected a path ending in '%s', got '%s'", what, expectedSuffix, newPath);

/* In the failing version this hands the careful handler a pointer from the system malloc.
 * It reads a block header that was never written and subtracts a garbage size. */
freeMem(newPath);
carefulCheckHeap();

/* This is where the damage above shows itself: the running total has wrapped around, so the
 * next allocation of any size looks larger than the limit and the process exits. */
char *probe = needMem(4096);
freeMem(probe);
carefulCheckHeap();

int blocksAfter = carefulCountBlocksAllocated();
if (blocksAfter != blocksBefore)
    errAbort("%s: %d block(s) were allocated and not released", what, blocksAfter - blocksBefore);
printf("%s: freed cleanly, nothing left behind\n", what);
}

int main(int argc, char *argv[])
/* Build the fixture, then run each branch under the careful allocator. */
{
if (argc != 2)
    usage();
char *workDir = argv[1];
char relLinkPath[PATH_LEN], absLinkPath[PATH_LEN];
makeFixture(workDir, relLinkPath, absLinkPath, PATH_LEN);

/* Everything above here used the default handler.  Nothing allocated before this point may be
 * released after it, since the careful handler would not recognize it either. */
pushCarefulMemHandler(carefulAllocLimit);
int blocksBefore = carefulCountBlocksAllocated();

/* The branch the fix is about: a relative symlink, resolved with realpath. */
checkResult("relative symlink", sessionDataSaveTrashFile(relLinkPath, unusedSessionDir),
            "/real/target.txt", blocksBefore);

/* Its sibling, which returns the link target as read. */
checkResult("absolute symlink", sessionDataSaveTrashFile(absLinkPath, unusedSessionDir),
            "/real/target.txt", blocksBefore);

/* An expired file is not an error; the callers expect NULL. */
char missingPath[PATH_LEN];
splitPath(relLinkPath, missingPath, NULL, NULL);
safecat(missingPath, sizeof missingPath, "gone.txt");
if (sessionDataSaveTrashFile(missingPath, unusedSessionDir) != NULL)
    errAbort("missing file: expected NULL");
if (carefulCountBlocksAllocated() != blocksBefore)
    errAbort("missing file: returned NULL but left memory allocated");
printf("missing file: returned NULL\n");

carefulCheckHeap();
printf("passed\n");
return 0;
}
