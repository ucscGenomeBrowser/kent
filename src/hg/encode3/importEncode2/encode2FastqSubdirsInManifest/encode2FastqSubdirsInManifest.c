/* encode2FastqSubdirsInManifest - Transform manifest file to include contents of fastq tarballs.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "encode3/encode2Manifest.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encode2FastqSubdirsInManifest - Transform manifest file to include contents of fastq tarballs.\n"
  "usage:\n"
  "   encode2FastqSubdirsInManifest inputManifest rootDir outputManifest\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void unpackDir(struct encode2Manifest *mi, char *rootDir, FILE *f)
/* List directory - call it all fastq in new manifest. */
{
char dirPath[PATH_LEN];
safef(dirPath, sizeof(dirPath), "%s/%s", rootDir, mi->fileName);
uglyf("Theoretically unpacking %s\n", dirPath);
}

void encode2FastqSubdirsInManifest(char *inputManifest, char *rootDir, char *outputManifest)
/* encode2FastqSubdirsInManifest - Transform manifest file to include contents of fastq tarballs. */
{
struct encode2Manifest *mi, *miList= encode2ManifestLoadAll(inputManifest);
FILE *f = mustOpen(outputManifest, "w");
uglyf("Loaded %d items from %s\n", slCount(miList), inputManifest);
for (mi = miList; mi != NULL; mi = mi->next)
    {
    if (endsWith(mi->fileName, ".tgz.dir"))
        {
	unpackDir(mi, rootDir, f);
	}
    else
        {
	encode2ManifestTabOut(mi, f);
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
encode2FastqSubdirsInManifest(argv[1], argv[2], argv[3]);
return 0;
}
