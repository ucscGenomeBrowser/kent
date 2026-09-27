/* udcDotsTest - check how udc turns a remote URL with "." or ".." in it into a cache path.
 *
 * A hub may reach up a level to share a file between assemblies, so a ".." in a bigDataUrl
 * is legal and must fetch the file the remote server would serve.  The cache path has to name
 * that same file, and a ".." that climbs above the host must abort instead of leaving the
 * cache directory.  A URL with no dot components must map to exactly the path it always did,
 * or every cached file on every node would be orphaned.
 *
 * udcFileCacheFiles() builds the cache names without touching the network, so this test
 * prints the directory each URL maps to.  refs #38120 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "errCatch.h"
#include "udc.h"

static char *cacheDir = "/udcCache";

static char *cases[] = {
/* no dot components: the path must come back exactly as given */
"https://hgdownload.soe.ucsc.edu/hubs/GCF/000/001/405/hub.txt",
"http://example.com/a//b/file.bb",
"http://example.com/dir/",
/* names that only start with dots are ordinary names */
"http://example.com/..foo/.bar/file...bb",
/* "." is dropped */
"http://example.com/a/./b/./file.bb",
/* ".." removes the component before it, as a server does */
"http://example.com/hubs/hg38/../shared/file.bb",
"http://example.com/a/b/c/../../file.bb",
"http://example.com/a/b/../../c/./file.bb",
"ftp://example.com/pub/x/../y/file.bw",
/* a ".." that climbs to or above the host must abort */
"http://example.com/../file.bb",
"http://example.com/a/../../file.bb",
"http://example.com/a/../../../../etc/passwd",
"https://example.com/..",
};

static void showCase(char *url)
/* Print the cache directory url maps to, or the error it aborts with. */
{
printf("in  : %s\n", url);
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    struct slName *files = udcFileCacheFiles(url, cacheDir);
    /* The first name is the bitmap file, inside the directory the URL maps to. */
    char *dir = cloneString(files->name);
    chopSuffixAt(dir, '/');
    printf("dir : %s\n", dir);
    freeMem(dir);
    slFreeList(&files);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    printf("abort: %s\n", trimSpaces(errCatch->message->string));
errCatchFree(&errCatch);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int i;
for (i = 0; i < ArraySize(cases); i++)
    showCase(cases[i]);
return 0;
}
