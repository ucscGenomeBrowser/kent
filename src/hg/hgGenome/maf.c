/* maf - stuff to process maf tracks.  */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "portable.h"
#include "obscure.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "cart.h"
#include "web.h"
#include "trackDb.h"
#include "maf.h"
#include "hgMaf.h"
#include "hgGenome.h"


boolean isMafTable(char *database, struct trackDb *track, char *table)
/* Return TRUE if table is maf. */
{
char setting[128], *p = setting;

if (track == NULL)
    return FALSE;
safecpy(setting, sizeof setting, track->type);
char *type = nextWord(&p);

if (sameString(type, "maf") || sameString(type, "wigMaf"))
    if (sameString(track->table, table))
        return TRUE;
return FALSE;
}


