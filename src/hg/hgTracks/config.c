/* config - put up track and display configuration page. */

#include "common.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hCommon.h"
#include "cart.h"
#include "web.h"
#include "hgTracks.h"

void textSizeDropDown()
/* Create drop down for font size. */
{
static char *sizes[] = {"tiny", "small", "medium", "large", "huge"};
cartUsualString(cart, textSizeVar, "small");
hDropList(textSizeVar, sizes, ArraySize(sizes), tl.textSize);
}

void trackConfig()
/* Put up track configurations. */
{
struct track *trackList = getTrackList();
struct group *group, *groupList = NULL;
boolean showedRuler = FALSE;

groupTracks(&trackList, &groupList);
hTableStart();
for (group = groupList; group != NULL; group = group->next)
    {
    struct trackRef *tr;

    if (group->trackList == NULL)
	continue;

    hPrintf("<TR>");
    hPrintf("<TH colspan=3 BGCOLOR=#536ED3>");
    hPrintf("<B>%s</B>", wrapWhiteFont(group->label));
    hPrintf("</TH>\n");
    hPrintf("</TR>");

    /* First group gets ruler. */
    if (!showedRuler)
	{
	showedRuler = TRUE;
	hPrintf("<TR>");
	hPrintf("<TD>");
	hPrintf("Base Position");
	hPrintf("</TD>");
	hPrintf("<TD>");
	hTvDropDownClass("ruler", rulerMode, FALSE, rulerMode ? "normalText" : "hiddenText");
	hPrintf("</TD>");
	hPrintf("<TD>");
	hPrintf("Chromosome position in bases.  (Clicks here zoom in 3x)");
	hPrintf("</TD>");
	hPrintf("</TR>");
	}

    /* Loop through this group. */
    for (tr = group->trackList; tr != NULL; tr = tr->next)
	{
	struct track *track = tr->track;
	hPrintf("<TR>");
	hPrintf("<TD>");
	if (track->hasUi)
	    hPrintf("<A HREF=\"%s?%s=%u&c=%s&g=%s\">", hgTrackUiName(),
		cartSessionVarName(), cartSessionId(cart),
		chromName, track->mapName);
	hPrintf(" %s", track->shortLabel);
	if (track->hasUi)
	    hPrintf("</A>");
	hPrintf("</TD>");
	hPrintf("<TD>");
	/* If track is not on this chrom print an informational
	   message for the user. */
	if(hTrackOnChrom(track->tdb, chromName)) 
	    hTvDropDownClass(track->mapName, track->visibility, track->canPack,
			     (track->visibility == tvHide) ? 
			     "hiddenText" : "normalText" );
	else 
	    hPrintf("[No data-%s]", chromName);
	hPrintf("</TD>");
	hPrintf("<TD>");
	hPrintf("%s", track->longLabel);
	hPrintf("</TD>");
	hPrintf("</TR>");
	}
    }
hTableEnd();
}

void configPage()
/* Put up configuration page. */
{
char *position = cartUsualString(cart, "position", hDefaultPos(database));
struct dyString *title = dyStringNew(0);

dyStringPrintf(title, "Configure %s %s (%s) Browser Display",
	       hOrganism(database), hFreezeFromDb(database), database);
webStartWrapper(cart, title->string, NULL, FALSE, FALSE);
hPrintf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=POST>\n", hgTracksName());
cartSaveSession(cart);

hPrintf("position: ");
hTextVar("position", addCommasToPos(position), 30);
hPrintf(" image width: ");
hIntVar("pix", tl.picWidth, 4);
hPrintf(" text size: ");
textSizeDropDown();
hPrintf(" ");
cgiMakeButton("Submit", "Submit");

webNewSection("Track Controls");
trackConfig();

hPrintf("</FORM>");
dyStringFree(&title);
}

