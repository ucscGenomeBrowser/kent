/* transMapStuff - common definitions and functions for supporting transMap
 * tracks in the browser CGIs */
#include "common.h"
#include "transMapStuff.h"
#include "trackDb.h"
#include "hdb.h"

char *transMapIdToAcc(char *id)
/* remove all unique suffixes (starting with last `-') from any TransMap 
 * id.  WARNING: static return */
{
static char acc[128];
safecpy(acc, sizeof(acc), id);
char *dash = strrchr(acc, '-');
if (dash != NULL)
    *dash = '\0';
return acc;
}
