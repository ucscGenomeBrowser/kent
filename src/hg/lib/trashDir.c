/* trashDir.c - temporary file creation and directory creation in /trash */

static char const rcsid[] = "$Id: trashDir.c,v 1.1 2007/02/09 22:25:08 hiram Exp $";

#include "common.h"
#include "hash.h"
#include "portable.h"
#include "trashDir.h"

void trashDirFile(struct tempName *tn, char *suffix, char *dirName)
/*	obtain a trash file name in the specified trash dirName */
{
static struct hash *dirHash = NULL;
char prefix[64];

if (! dirHash)
	dirHash = newHash(0);

/* already created this directory ? */
if (hashLookup(dirHash,dirName))
    {
    hashAddInt(dirHash, dirName, 1);	/* remember, been here, done that */
    mkdirTrashDirectory(dirName);
    }
safef(prefix, sizeof(prefix), "%s/%s", dirName,dirName);
makeTempName(tn, prefix, suffix);
}
