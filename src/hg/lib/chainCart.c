/*	chainCart.c - take care of parsing and combining values from the
 *	chain trackDb optional settings and the same values that may be
 *	in the cart.
 */
#include "common.h"
#include "jksql.h"
#include "trackDb.h"
#include "cart.h"
#include "dystring.h"
#include "hui.h"
#include "chainCart.h"

static char const rcsid[] = "$Id: chainCart.c,v 1.1 2004/07/19 22:45:54 hiram Exp $";

extern struct cart *cart;      /* defined in hgTracks.c or hgTrackUi */

/******	ColorOption - Chrom colors by default **************************/
enum chainColorEnum chainFetchColorOption(struct trackDb *tdb,
	char **optString)
{
char *Default = chainColorEnumToString(chainColorChromColors);
char option[MAX_OPT_STRLEN]; /* .color  */
char *chainColor = NULL;
enum chainColorEnum ret;

snprintf( option, sizeof(option), "%s.%s", tdb->tableName, OPT_CHROM_COLORS );
chainColor = cloneString(cartOptionalString(cart, option));

/*	If chainColor is a string, it came from the cart, otherwise
 *	see if it is specified in the trackDb option, finally
 *	return the default.
 */
if (!chainColor)
    {
    char * tdbDefault = 
	trackDbSettingOrDefault(tdb, OPT_CHROM_COLORS, Default);

    freeMem(chainColor);
    if (differentWord(Default,tdbDefault))
	chainColor = cloneString(tdbDefault);
    else
	{
	struct hashEl *hel;
	/*	no chainColor from trackDb, maybe it is in tdb->settings
	 *	(custom tracks keep settings here)
	 */
	chainColor = cloneString(Default);
	if ((tdb->settings != (char *)NULL) &&
	    (tdb->settingsHash != (struct hash *)NULL))
	    {
	    if ((hel =hashLookup(tdb->settingsHash, OPT_CHROM_COLORS)) !=NULL)
		if (differentWord(Default,(char *)hel->val))
		    {
		    freeMem(chainColor);
		    chainColor = cloneString((char *)hel->val);
		    }
	    }
	}
    }

if (optString)
    *optString = cloneString(chainColor);

ret = chainColorStringToEnum(chainColor);
freeMem(chainColor);
return(ret);
}	/*	enum chainColorEnum chainFetchColorOption()	*/
