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

void trackConfig(char *groupToChange,  int changeVis)
/* Put up track configurations. If groupToChange is 
 * NULL then set visibility for tracks in all groups.  Otherwise,
 * just set it for the given group.  If vis is -2, then visibility is
 * unchanged.  If -1 then set visibility to default, otherwise it should 
 * be tvHide, tvDense, etc. */
{
struct track *trackList = getTrackList();
struct group *group, *groupList = NULL;
boolean showedRuler = FALSE;

groupTracks(&trackList, &groupList);
setRulerMode();

/* Set up ruler mode according to changeVis. */
if (changeVis != -2)
    {
    if (groupToChange == NULL || 
    	(groupList != NULL && sameString(groupToChange, groupList->name)))
	{
	if (changeVis == -1)
	    rulerMode = tvFull;
	else
	    rulerMode = changeVis;
	}
    }

hPrintf(
"<SCRIPT LANGUAGE=\"JavaScript\">\n"
"function testResults() {\n"
"   alert('I was there');\n"
"}\n"
"</SCRIPT>\n"
);

cgiMakeHiddenVar(configGroupTarget, "none");
hTableStart();
for (group = groupList; group != NULL; group = group->next)
    {
    struct trackRef *tr;

    if (group->trackList == NULL)
	continue;

    if (changeVis != -2 && (groupToChange == NULL || sameString(group->name,groupToChange)))
        {
	for (tr = group->trackList; tr != NULL; tr = tr->next)
	    {
	    struct track *track = tr->track;
	    if (changeVis == -1)
	        track->visibility = track->tdb->visibility;
	    else
	        track->visibility = changeVis;
	    }
	}

    hPrintf("<TR>");
    hPrintf("<TH align=LEFT colspan=3 BGCOLOR=#536ED3>");
    hPrintf("<B>&nbsp;%s</B> ", wrapWhiteFont(group->label));
    hPrintf("<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"%s\" "
	   "onClick=\"document.mainForm.%s.value='%s';\">", 
	   configHideAll, "Hide All", configGroupTarget, group->name);
    hPrintf(" ");
    hPrintf("<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"%s\" "
	   "onClick=\"document.mainForm.%s.value='%s';\">", 
	   configShowAll, "Show All", configGroupTarget, group->name);
    hPrintf(" ");
    hPrintf("<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"%s\" "
	   "onClick=\"document.mainForm.%s.value='%s';\">", 
	   configDefaultAll, "Default", configGroupTarget, group->name);
    hPrintf(" ");
    cgiMakeButton("submit", "Submit");
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
	    hPrintf("<A HREF=\"%s?%s=%u&g=%s\">", hgTrackUiName(),
		cartSessionVarName(), cartSessionId(cart),
		track->mapName);
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

void configPageSetTrackVis(int vis)
/* Do config page after setting track visibility. If vis is -2, then visibility 
 * is unchanged.  If -1 then set visibility to default, otherwise it should 
 * be tvHide, tvDense, etc. */
{
struct dyString *title = dyStringNew(0);
char *group = NULL;

/* Fetch group if any from CGI, and remove var so it doesn't get used again. */
group = cloneString(cartUsualString(cart, configGroupTarget, ""));
cartRemove(cart, configGroupTarget);
if (sameString(group, "none"))
    freez(&group);

dyStringPrintf(title, "Configure Image",
	       hOrganism(database), hFreezeFromDb(database), database);
webStartWrapper(cart, title->string, NULL, FALSE, FALSE);
hPrintf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=POST>\n", hgTracksName());
cartSaveSession(cart);

hPrintf(" image width: ");
hIntVar("pix", tl.picWidth, 4);
hPrintf(" text size: ");
textSizeDropDown();
hPrintf(" ");
cgiMakeButton("Submit", "Submit");
hPrintf("<P>");
hTableStart();
hPrintf("<TR><TD>");
hCheckBox("ideogram", cartUsualBoolean(cart, "ideogram", TRUE));
hPrintf("</TD><TD>");
hPrintf("Display chromosome ideogram above main graphic.");
hPrintf("</TD></TR>");
hPrintf("<TR><TD>");
hCheckBox("guidelines", cartUsualBoolean(cart, "guidelines", TRUE));
hPrintf("</TD><TD>");
hPrintf("Show light blue vertical guidelines.");
hPrintf("</TD></TR>");
hPrintf("<TR><TD>");
hCheckBox("leftLabels", cartUsualBoolean(cart, "leftLabels", TRUE));
hPrintf("</TD><TD>");
hPrintf("Display labels to the left of items tracks.");
hPrintf("</TD></TR>");
hPrintf("<TR><TD>");
hCheckBox("centerLabels", cartUsualBoolean(cart, "centerLabels", TRUE));
hPrintf("</TD><TD>");
hPrintf("Display track description above each track.");
hPrintf("</TD></TR>");
hTableEnd();

webNewSection("Configure Tracks");
hPrintf("Control tracks in all groups here ");
cgiMakeButton(configHideAll, "Hide All");
hPrintf(" ");
cgiMakeButton(configShowAll, "Show All");
hPrintf(" ");
cgiMakeButton(configDefaultAll, "Default");
hPrintf(" ");
hPrintf("or control tracks visibility more selectively below.<BR>");
trackConfig(group, vis);

hPrintf("</FORM>");
dyStringFree(&title);
freez(&group);
}


void configPage()
/* Put up configuration page. */
{
configPageSetTrackVis(-2);
}
