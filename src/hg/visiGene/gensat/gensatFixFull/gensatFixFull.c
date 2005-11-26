/* gensatFixFull - Fix a problem that arose from image magic convert not handling .full in filename during tiling.  Should be a one-time fix.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"

static char const rcsid[] = "$Id: gensatFixFull.c,v 1.1 2005/11/26 17:05:49 kent Exp $";

char *database = "visiGene";
char *thumbDir = "/cluster/store11/visiGene/gbdb/200/inSitu/Mouse/gensat";
char *fullDir = "/cluster/store11/visiGene/gbdb/full/inSitu/Mouse/gensat";
char *downloadDir = "/cluster/store11/visiGene/offline/gensat/Institutions";
char *stripper = ".full";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gensatFixFull - Fix a problem that arose from image magic convert not handling .full in filename during tiling.  Should be a one-time fix.\n"
  "usage:\n"
  "   gensatFixFull out.sh out.sql\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void fixDir(FILE *f, char *dir)
/* Fix file names in directory and any subdirectories. */
{
struct fileInfo *el, *list = listDirX(dir, "*", TRUE);
int dotEvery = 10;
int dot = 0;
int lineEvery = 50;
int line = 0;

/* Recurse on dirs (better to do this first before they are
 * renamed). */
for (el = list; el != NULL; el = el->next)
    {
    if (el->isDir)
        fixDir(f, el->name);
    }

/* Do the renaming. */
for (el = list; el != NULL; el = el->next)
    {
    char *source = el->name;
    char dir[PATH_LEN], file[PATH_LEN], ext[PATH_LEN];
    splitPath(source, dir, file,ext);
    if (stringIn(stripper, file) || stringIn(stripper, ext))
        {
	char dest[PATH_LEN];
	stripString(file, stripper);
	stripString(ext, stripper);
	safef(dest, sizeof(dest), "%s%s%s", dir, file, ext);
	rename(source, dest);
	fprintf(f, "mv %s %s\n", source, dest);
	}
    else
        fprintf(f, "not moving %s\n", source);
    }
slFreeList(&list);
}

void fixFiles(char *shellScript)
/* Get rid of .full in file names in both thumb and full dirs. */
{
FILE *f = mustOpen(shellScript, "w");
printf("#Fixing thumbnails\n");
fixDir(f, thumbDir);
printf("\n");
printf("#Fixing full\n");
fixDir(f, fullDir);
printf("\n");
printf("#Fixing download\n");
fixDir(f, downloadDir);
printf("\n");
carefulClose(&f);
}

void fixDb(char *sqlScript)
/* Get rid of .full in image file names. */
{
FILE *f = mustOpen(sqlScript, "w");

fprintf(f, "update imageFile set fileName=REPLACE(fileName, '.full', '') where submitId=707;\n");

carefulClose(&f);
}

void gensatFixFull(char *outSh, char *outSql)
/* gensatFixFull - Fix a problem that arose from image magic convert not 
 * handling .full in filename during tiling.  Should be a one-time fix.. */
{
fixFiles(outSh);
fixDb(outSql);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
gensatFixFull(argv[1], argv[2]);
return 0;
}
