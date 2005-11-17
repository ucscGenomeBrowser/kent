/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "dnaseq.h"
#include "fa.h"
#include "portable.h"

static char const rcsid[] = "$Id: freen.c,v 1.60 2005/11/17 16:51:55 kent Exp $";

void usage()
{
errAbort("freen - rearrange messup into directories.\n"
         "usage:  freen lsFile inDir outDir\n");
}

void freen(char *lsFile, char *inDir, char *outDir)
/* Test some hair-brained thing. */
{
char oldName[PATH_LEN], newName[PATH_LEN];
struct lineFile *lf = lineFileOpen(lsFile, TRUE);
char *row[9], *part1, *part2;
struct hash *ncHash = hashNew(0);

makeDir(outDir);
while (lineFileRow(lf, row))
    {
    char *oldFile = row[8];
    safef(oldName, sizeof(oldName), "%s/%s", inDir, oldFile);

    /* Split NT_004612_135 into NT_004612 and 135 */
    part1 = oldFile;
    part2 = strchr(oldFile, '_');
    if (part2 == NULL)
        errAbort("Couldn't find first _ in %s", oldFile);
    part2 = strchr(part2+1, '_');
    if (part2 == NULL)
        errAbort("Couldn't find second _ in %s", oldFile);
    *part2++ = 0;

    /* If need be make new subdirectory */
    if (!hashLookup(ncHash, part1))
        {
	hashAdd(ncHash, part1, NULL);
	safef(newName, sizeof(newName), "%s/%s", outDir, part1);
	makeDir(newName);
	}

    safef(newName, sizeof(newName), "%s/%s/%s", outDir, part1, part2);
    if (rename(oldName, newName) != 0)
        errnoAbort("Couldn't rename %s to %s", oldName, newName);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
   usage();
freen(argv[1], argv[2], argv[3]);
return 0;
}
