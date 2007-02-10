/* trashDir.c - temporary file creation and directory creation in /trash */

static char const rcsid[] = "$Id: trashDir.c,v 1.3 2007/02/10 00:04:14 hiram Exp $";

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
safef(prefix, sizeof(prefix), "%s/%s", dirName,base);
makeTempName(tn, prefix, suffix);
}
