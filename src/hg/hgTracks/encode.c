/* encode.c - hgTracks routines that are specific to the ENCODE project */

#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "hgTracks.h"


char *encodeErgeName(struct track *tg, void *item)
/* return the actual data name, in form xx/yyyy cut off xx/ return yyyy */
{
char *name;
struct linkedFeatures *lf = item;
name = strstr(lf->name, "/");
if (name != NULL)
    name ++;
if (name != NULL)
    return name;
return "unknown";
}
  
void encodeErgeMethods(struct track *tg)
/* setup special methods for ENCODE dbERGE II tracks */
{
tg->itemName = encodeErgeName;
}

