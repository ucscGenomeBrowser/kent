/* encode2Md5UpdateManifest - Update md5sum, size, validation key in an 
 * encode2 manifest.tab file.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "encode3/encode2Manifest.h"
#include "encode3/encode3Valid.h"
#include "md5.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
"encode2Md5UpdateManifest - Update md5sum, size, validation key in an encode2 manifest.tab file.\n"
"usage:\n"
"   encode2Md5UpdateManifest md5sumFile rootDir oldManifest newManifest\n"
"options:\n"
"   -xxx=XXX\n");
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void updateSumAndAll(struct encode2Manifest *mi, char *md5, char *rootDir)
/* Update mi to reflect new md5.  Since this means file has changed we'll go
 * look for file and get new size and update time.  We also update validation key. */
{
char path[PATH_LEN];
safef(path, sizeof(path), "%s/%s", rootDir, mi->fileName);
off_t size = fileSize(path);
if (size == -1)
    errAbort("%s doesn't exist", path);
mi->size = size;
mi->modifiedTime = fileModTime(path);
mi->md5sum = md5;
mi->validKey = encode3CalcValidationKey(md5, size);
}

void encode2Md5UpdateManifest(char *md5File, char *rootDir, char *oldManifest, char *newManifest)
/* encode2Md5UpdateManifest - Update md5sum, size, validation key in an encode2 
 * manifest.tab file. */
{
struct encode2Manifest *mi, *miList = encode2ManifestLoadAll(oldManifest);
struct hash *md5Hash = md5FileHash(md5File);
verbose(2, "Got %d items in miList, %d in md5Hash\n", slCount(miList), md5Hash->elCount);
FILE *f = mustOpen(newManifest, "w");
int updateCount = 0;
for  (mi = miList; mi != NULL; mi = mi->next)
    {
    char *newMd5 = hashFindVal(md5Hash, mi->fileName);
    if (newMd5 != NULL)
        {
	++updateCount;
	updateSumAndAll(mi, newMd5, rootDir);
	}
    encode2ManifestTabOut(mi, f);
    }
verbose(1, "Found %d of %d in patch.\n", updateCount, md5Hash->elCount);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
encode2Md5UpdateManifest(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
