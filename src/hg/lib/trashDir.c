/* trashDir.c - temporary file creation and directory creation in /trash */

static char const rcsid[] = "$Id: trashDir.c,v 1.5 2007/03/31 16:48:10 markd Exp $";

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
