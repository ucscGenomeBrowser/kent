/* patchToAgp - Convert Elia's patch file that describe NT clone position to AGP file. */
#include "common.h"
#include "hash.h"
#include "linefile.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "patchToAgp - Convert Elia's patch file that describe NT clone position to AGP file\n"
  "usage:\n"
  "   patchToAgp ~gs/ffa/sequence.inf in.patch out.agp\n");
}

struct hash *readVersionInfo(char *fileName)
/* Read sequence.inf file to figure out version number of
 * each clone.  Save it as a hash keyed by accession with
 * values accession.version. */
{
char acc[128];
char *row[1];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);

while (lineFileRow(lf, row))
    {
    strcpy(acc, row[0]);
    chopSuffix(acc);
    hashAdd(hash, acc, cloneString(row[0]));
    }
lineFileClose(&lf);
return hash;
}

char *fixVersion(char *accVer, struct hash *versionHash)
/* See if accVer ends in .0.  If so try to fix it via
 * hash lookup. */
{
static char fixed[128];
char cloneName[128];
char *s;

strcpy(cloneName, accVer);
chopSuffix(cloneName);
return hashMustFindVal(versionHash, cloneName);
}

void patchToAgp(char *infName, char *inName, char *outName)
/* patchToAgp - Convert Elia's patch file that describe NT clone position to AGP file. */
{
char *row[9];
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
int ix = 0;
struct hash *versionHash = readVersionInfo(infName);
int ntStart, ntEnd, cStart, cEnd;
int diff;

while (lineFileRow(lf, row))
    {
    ntStart = atoi(row[2]);
    ntEnd = atoi(row[3]);
    cStart = atoi(row[7]);
    cEnd = atoi(row[8]);
    if (cStart <= 0)
        {
	warn("Fixing negative clone start line %d of %s", lf->lineIx, lf->fileName);
	diff = 1 - cStart;
	cStart += diff;
	cEnd += diff;
	if (cEnd - cStart != ntEnd - ntStart)
	     errAbort("Couldn't fix properly");
	}
    fprintf(f, "%s\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t%c\n",
	row[0], ntStart, ntEnd, ++ix,
	row[5], fixVersion(row[6], versionHash),
	cStart, cEnd,
	row[4][0]);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
patchToAgp(argv[1], argv[2], argv[3]);
return 0;
}
