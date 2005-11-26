/* gensatFixFull - Fix a problem that arose from image magic convert not handling .full in filename 
 * during tiling.  Should be a one-time fix.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"

static char const rcsid[] = "$Id: gensatFixFull.c,v 1.2 2005/11/26 17:21:50 kent Exp $";

char *database = "visiGene";
char *thumbDir = "/cluster/store11/visiGene/gbdb/200/inSitu/Mouse/gensat";
char *fullDir = "/cluster/store11/visiGene/gbdb/full/inSitu/Mouse/gensat";
char *downloadDir = "/cluster/store11/visiGene/offline/gensat/Institutions";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gensatFixFull - Fix a problem that arose from image magic convert not handling .full "
  "in filename during tiling.  Should be a one-time fix.\n"
  "usage:\n"
  "   gensatFixFull now\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void fixDir(char *dir)
/* Fix file names in directory and any subdirectories. */
{
struct fileInfo *el, *list = listDirX(dir, "*", TRUE);

/* Recurse on dirs (better to do this first before they are
 * renamed). */
for (el = list; el != NULL; el = el->next)
    {
    if (el->isDir)
        fixDir(el->name);
    }

/* Do the linking. */
for (el = list; el != NULL; el = el->next)
    {
    char *commonTail;
    char linkName[PATH_LEN];
    if (endsWith(el->name, ".jpg"))
        {
	commonTail = el->name + strlen(downloadDir);
	safef(linkName, sizeof(linkName), "%s%s", fullDir, commonTail);
	uglyf("ln -s %s %s\n", el->name, linkName);
	}
    }
slFreeList(&list);
}

void gensatFixFull(char *logFile)
/* gensatFixFull - Fix a problem that arose from image magic convert not 
 * handling .full in filename during tiling.  Should be a one-time fix.. */
{
fixDir(downloadDir);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
gensatFixFull(argv[1]);
return 0;
}
