/* config - put up track and display configuration page. */

#include "common.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hCommon.h"
#include "cart.h"
#include "web.h"
#include "customTrack.h"
#include "hgTracks.h"

void textSizeDropDown()
/* Create drop down for font size. */
{
static char *sizes[] = {"tiny", "small", "medium", "large", "huge"};
cartUsualString(cart, textSizeVar, "small");
hDropList(textSizeVar, sizes, ArraySize(sizes), tl.textSize);
}


void trackConfig(struct track *trackList, struct group *groupList,
	char *groupTarget,  int changeVis)
/* Put up track configurations. If groupTarget is 
 * NULL then set visibility for tracks in all groups.  Otherwise,
 * just set it for the given group.  If vis is -2, then visibility is
 * unchanged.  If -1 then set visibility to default, otherwise it should 
 * be tvHide, tvDense, etc. */
{
char pname[512];
struct group *group;
boolean showedRuler = FALSE;

setRulerMode();
changeTrackVis(groupList, groupTarget, changeVis, FALSE);

/* Set up ruler mode according to changeVis. */
#ifdef BOB_DOESNT_LIKE
if (changeVis != -2)
    {
    if (groupTarget == NULL || 
    	(groupList != NULL && sameString(groupTarget, groupList->name)))
	{
	if (changeVis == -1)
	    rulerMode = tvFull;
	else
	    rulerMode = changeVis;
	}
    }
#endif /* BOB_DOESNT_LIKE */

cgiMakeHiddenVar(configGroupTarget, "none");
for (group = groupList; group != NULL; group = group->next)
    {
    struct trackRef *tr;

    if (group->trackList == NULL)
	continue;

    hTableStart();
    hPrintf("<TR>");
    hPrintf("<TH align=LEFT colspan=3 BGCOLOR=#536ED3>");
    hPrintf("<B>&nbsp;%s</B> ", wrapWhiteFont(group->label));
    hPrintf("&nbsp;&nbsp;&nbsp;");
    hPrintf("<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"%s\" "
	   "onClick=\"document.mainForm.%s.value='%s';\">", 
	   configHideAll, "hide all", configGroupTarget, group->name);
    hPrintf(" ");
    hPrintf("<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"%s\" "
	   "onClick=\"document.mainForm.%s.value='%s';\">", 
	   configShowAll, "show all", configGroupTarget, group->name);
    hPrintf(" ");
    hPrintf("<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"%s\" "
	   "onClick=\"document.mainForm.%s.value='%s';\">", 
	   configDefaultAll, "default", configGroupTarget, group->name);
    hPrintf(" ");
    cgiMakeButton("submit", "submit");
    hPrintf("</TH>\n");
    if (withPriorityOverride)
        {
        hPrintf("<TH>\n");
        safef(pname, sizeof(pname), "%s.priority",group->name);
        hDoubleVar(pname, (double)group->priority, 4);
        hPrintf("</TH>\n");
        }
    hPrintf("</TR>\n");

    /* First group gets ruler. */
    if (!showedRuler)
	{
	showedRuler = TRUE;
	hPrintf("<TR>");
	hPrintf("<TD>");
	hPrintf("%s", RULER_TRACK_LABEL);
	hPrintf("</TD>");
	hPrintf("<TD>");
	hTvDropDownClass("ruler", rulerMode, FALSE, rulerMode ? "normalText" : "hiddenText");
	hPrintf("</TD>");
	hPrintf("<TD>");
	hPrintf("Chromosome position in bases.  (Clicks here zoom in 3x)");
	hPrintf("</TD>");
        if (withPriorityOverride)
            {
            hPrintf("<TD>");
            hPrintf("");
            hPrintf("</TD>");
            }
	hPrintf("</TR>\n");
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
        if (withPriorityOverride)
            {
            hPrintf("<TD>");
            safef(pname, sizeof(pname), "%s.priority",track->mapName);
            hDoubleVar(pname, (double)track->priority, 4);
            hPrintf("</TD>");
            }
	hPrintf("</TR>\n");
	}
    hTableEnd();
    hPrintf("<BR>");
    }
}

void configPageSetTrackVis(int vis)
/* Do config page after setting track visibility. If vis is -2, then visibility 
 * is unchanged.  If -1 then set visibility to default, otherwise it should 
 * be tvHide, tvDense, etc. */
{
struct dyString *title = dyStringNew(0);
char *groupTarget = NULL;
struct track *trackList =  NULL;
struct track *ideoTrack = NULL;
struct group *groupList = NULL;

/* Get track list and group them. */
ctList = customTracksParseCart(cart, &browserLines, &ctFileName);
trackList = getTrackList(&groupList);

/* The ideogram for some reason is considered a track.
 * We don't really want to process it as one though, so
 * we see if it's there, and if necessary remove it. */
ideoTrack = chromIdeoTrack(trackList);
if (ideoTrack != NULL)
    removeTrackFromGroup(ideoTrack);

/* Fetch group to change on if any from CGI, 
 * and remove var so it doesn't get used again. */
groupTarget = cloneString(cartUsualString(cart, configGroupTarget, ""));
cartRemove(cart, configGroupTarget);
if (sameString(groupTarget, "none"))
    freez(&groupTarget);

dyStringPrintf(title, "Configure Image");
hPrintf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=POST>\n", hgTracksName());
webStartWrapperDetailed(cart, "", title->string, NULL, FALSE, FALSE, FALSE, FALSE);
cartSaveSession(cart);

hPrintf(" image width: ");
hIntVar("pix", tl.picWidth, 4);
hPrintf(" text size: ");
textSizeDropDown();
hPrintf("&nbsp;");
if (trackLayoutInclFontExtras())
    {
    char *defaultStyle = cartUsualString(cart, "fontType", "medium");
    cartMakeRadioButton(cart, "fontType", "medium", defaultStyle);
    hPrintf("&nbsp;medium&nbsp;");
    cartMakeRadioButton(cart, "fontType", "fixed", defaultStyle);
    hPrintf("&nbsp;fixed&nbsp;");
    cartMakeRadioButton(cart, "fontType", "bold", defaultStyle);
    hPrintf("&nbsp;bold&nbsp;");
    hPrintf("&nbsp;");
    }
cgiMakeButton("Submit", "submit");
hPrintf("<P>");
hTableStart();
if (ideoTrack != NULL)
    {
    hPrintf("<TR><TD>");
    hCheckBox("ideogram", cartUsualBoolean(cart, "ideogram", TRUE));
    hPrintf("</TD><TD>");
    hPrintf("Display chromosome ideogram above main graphic");
    hPrintf("</TD></TR>\n");
    }
hPrintf("<TR><TD>");
hCheckBox("guidelines", cartUsualBoolean(cart, "guidelines", TRUE));
hPrintf("</TD><TD>");
hPrintf("Show light blue vertical guidelines");
hPrintf("</TD></TR>\n");
hPrintf("<TR><TD>");
hCheckBox("leftLabels", cartUsualBoolean(cart, "leftLabels", TRUE));
hPrintf("</TD><TD>");
hPrintf("Display labels to the left of items in tracks");
hPrintf("</TD></TR>\n");
hPrintf("<TR><TD>");
hCheckBox("centerLabels", cartUsualBoolean(cart, "centerLabels", TRUE));
hPrintf("</TD><TD>");
hPrintf("Display description above each track");
hPrintf("</TD></TR>\n");
hPrintf("<TR><TD>");
hCheckBox("trackControlsOnMain", cartUsualBoolean(cart, "trackControlsOnMain", TRUE));
hPrintf("</TD><TD>");
hPrintf("Show track controls under main graphic");
hPrintf("</TD></TR>\n");
hPrintf("<TR><TD>");
hCheckBox("nextItemArrows", cartUsualBoolean(cart, "nextItemArrows", FALSE));
hPrintf("</TD><TD>");
hPrintf("Next/previous item navigation");
hPrintf("</TD></TR>\n");
hPrintf("<TR><TD>");
hCheckBox("priorityOverride", cartUsualBoolean(cart, "priorityOverride", FALSE));
hPrintf("</TD><TD>");
hPrintf("Enable Track re-ordering");
hPrintf("</TD></TR>\n");
hTableEnd();

webNewSection("Configure Tracks");
hPrintf("Control tracks in all groups here: ");
cgiMakeButton(configHideAll, "hide all");
hPrintf(" ");
cgiMakeButton(configShowAll, "show all");
hPrintf(" ");
cgiMakeButton(configDefaultAll, "default");
hPrintf(" ");
hPrintf("&nbsp;&nbsp;Control track visibility more selectively below.<P>");
trackConfig(trackList, groupList, groupTarget, vis);

dyStringFree(&title);
freez(&groupTarget);
webEndSectionTables();
hPrintf("</FORM>");
}


void configPage()
/* Put up configuration page. */
{
withPriorityOverride = cartUsualBoolean(cart, "priorityOverride", FALSE);
configPageSetTrackVis(-2);
}
