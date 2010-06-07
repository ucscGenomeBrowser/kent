/* trashDir.c - temporary file creation and directory creation in /trash */

static char const rcsid[] = "$Id: trashDir.c,v 1.6 2008/07/09 22:55:35 angie Exp $";

#include "common.h"
#include "hash.h"
#include "portable.h"
#include "trashDir.h"

void trashDirFile(struct tempName *tn, char *dirName, char *base, char *suffix)
/*	obtain a trash file name trash/dirName/base*.suffix */
{
static struct hash *dirHash = NULL;
char prefix[64];

if (! dirHash)
	dirHash = newHash(0);

/* already created this directory ? */
if (! hashLookup(dirHash,dirName))
    {
    hashAddInt(dirHash, dirName, 1);	/* remember, been here, done that */
    mkdirTrashDirectory(dirName);
    }
/* no need to duplicate the _ at the end of base, makeTempName is going
 *	to add _ to the given base, some CGIs pass "base_"
 */
if (endsWith(base,"_"))
    {
    char *t = cloneString(base);
    int len = strlen(t);
    t[len-1] = '\0';	/* remove ending _ */
    safef(prefix, sizeof(prefix), "%s/%s", dirName,t);
    freeMem(t);
    }
else
    safef(prefix, sizeof(prefix), "%s/%s", dirName,base);
makeTempName(tn, prefix, suffix);
}

void copyFileToTrash(char **pFileName, char *dirName, char *base, char *suffix)
/* If *pFileName is not NULL and exists, then create a new file in the
 * given dirName of trash/ with the given base and suffix, copy *pFileName's
 * contents to it, and set *pFileName to the new filename. */
{
if (pFileName != NULL && *pFileName != NULL)
    {
    if (fileExists(*pFileName))
	{
	FILE *fIn = mustOpen(*pFileName, "r");
        struct tempName tn;
	trashDirFile(&tn, dirName, base, suffix);
        char *newFileName = tn.forCgi;
	FILE *fOut = mustOpen(newFileName, "w");
	unsigned char buf[16 * 1024];
	size_t sz;
	while ((sz = fread(buf, sizeof(buf[0]), ArraySize(buf), fIn)) > 0)
	    fwrite(buf, sizeof(buf[0]), sz, fOut);
	fclose(fOut);
	fclose(fIn);
	*pFileName = cloneString(newFileName);
	}
    }
}

