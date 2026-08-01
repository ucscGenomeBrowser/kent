#!/usr/bin/env python3
"""urlCommandCatalog.py - the registry of Genome Browser URL parameters.

Refs #37923.  Sibling of cartTrackVarCatalog.py (#37838), which does the same
job for track-scoped cart variables.  This one covers what can go on a CGI URL.

The distinction the tree does not currently record anywhere, and the reason
this file exists:

  action    a one-shot command.  It asks the CGI to do something, is consumed,
            and is gone on the next request.  hideTracks=1 is an action.
  setting   a cart variable that happens to be settable from the URL.  It is
            written to the user's session and stays there, for months, until
            something overwrites it.  pix=300 is a setting.

and a third state that falls out of the first two:

  leak      semantically a one-shot command, but nothing excludes or removes
            it, so it lands in the user's session anyway.  cartExclude() at
            hg/lib/cart.c:1800 is the only thing that keeps a CGI variable out
            of the saved cart, so any command not named in its CGI's
            excludeVars[] and not cartRemove()d is silently persisted.
            startTutorial=true is one: it turns the tutorial on, then follows
            the user around.  These are defects rather than design, and naming
            them is half the point of this catalog.  --reconcile audits the
            claim mechanically, so the list cannot rot.

The public help page (goldenPath/help/customTrackText.html, the optParams
anchor) lists both kinds in one undifferentiated bullet list, which is why
users cannot tell that hideTracks evaporates and pix does not.  Of everything
documented there, only hideTracks and ignoreCookie are actions.  Everything
else persists.

A command reaches a CGI by one of three routes, all harvested by
harvestUrlCommands.py next door:

  1. named in that CGI's char *excludeVars[], so cartNew() will not persist it
  2. read directly with cgiOptionalString()/cgiVarExists(), bypassing the cart
  3. read from the cart and then cartRemove()d after use

Route 2 is the one that causes bugs.  A value read straight from CGI while the
surrounding code reads the cart gives two different answers for the same
question; see the hideTracks note below.

Verification status.  Every row carries verified=True only if its kind and
persistence were confirmed by reading the code at the cited file:line during
curation.  Rows inferred from excludeVars membership alone are verified=False.
--check reports how many public rows are still unverified, because the
generated page must not go live until that number is zero: a wrong persistence
column is worse than the ambiguous page we have now.

Usage:
    urlCommandCatalog.py --json out.json
    urlCommandCatalog.py --html out.html
    urlCommandCatalog.py --check         # counts and consistency checks
    urlCommandCatalog.py --reconcile     # diff the catalog against the tree
    urlCommandCatalog.py --reconcile --verbose        # ... with the full diff
    urlCommandCatalog.py --update-baseline            # accept new tree names

--reconcile is the mode meant for a nightly cron: it prints nothing and exits 0
when nothing has changed, and exits 1 with a list when somebody has added a CGI
parameter nobody has classified, or when the leak audit's answer has moved.

The catalog covers the parameters a user can put on a browser URL, but the
harvester sees every cgiOptionalString() in the tree, which is form fields,
debug switches and the arguments of small utility CGIs as well.  The names in
that gap are recorded in BASELINE_FILE next to this script rather than argued
with one at a time: reconcile complains only about a name in neither the catalog
nor the baseline, so what it reports is what arrived since the baseline was
last accepted.  When the new names really are out of scope, add them with
--update-baseline and commit the file, which puts the decision in the git log
where it can be read later.
"""

import argparse
import html
import json
import os
import sys

# Tree names deliberately outside the catalog's scope.  See the docstring.
BASELINE_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "urlNamesNotCataloged.txt")

# Floor on how many names a working scan finds; well under the real count.  See
# the check in reconcile().
MIN_TREE_NAMES = 300

# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def c(name, kind, src, value="1", persists=False, public=False, note=None,
      verified=False, alias=None, deprecated=False, members=None,
      nocart=False, leaks=False, droppedBy=None):
    """One catalog entry.

    name       the CGI parameter name as typed on the URL
    kind       "action" (one-shot) or "setting" (persists in the cart)
    src        file:line where the tree reads it
    value      value syntax, for the docs
    persists   True if it is written to the cart and kept
    public     True if it is meant for users and belongs on the help page
    verified   True if kind/persists were confirmed by reading src
    alias      another name that does the same thing
    members    for a prefix family, the individual names under it
    nocart     the owning CGI keeps no cart at all, so nothing can persist and
               the persistence audit should not expect an excludeVars entry
    leaks      semantically one-shot, but nothing excludes or removes it, so it
               is written into the user's session anyway.  This is a defect,
               not a design: see the leak discussion in the module docstring
    droppedBy  file:line of an indirect cartRemove, where the name is passed
               into a helper rather than removed literally, so the audit
               cannot see it
    """
    d = {"name": name, "kind": kind, "src": src, "value": value,
         "persists": persists, "public": public, "verified": verified}
    if nocart:
        d["nocart"] = True
    if leaks:
        d["leaks"] = True
    if droppedBy:
        d["droppedBy"] = droppedBy
    if note:
        d["note"] = note
    if alias:
        d["alias"] = alias
    if deprecated:
        d["deprecated"] = True
    if members:
        d["members"] = members
    return d


# ---------------------------------------------------------------------------
# how the URL is read at all
# ---------------------------------------------------------------------------

MECHANISMS = {
    "excludeVars": {
        "what": "Named in the CGI's char *excludeVars[] array, so cartNew() "
                "loads it for this request and refuses to write it back.",
        "src": "hg/lib/cart.c:cartNew",
    },
    "directCgi": {
        "what": "Read with cgiOptionalString() or cgiVarExists(), which never "
                "consults the cart at all.  Invisible to anyone reading the "
                "cart machinery, and the source of the read-two-ways bugs.",
        "src": "src/lib/cheapcgi.c",
    },
    "cartRemove": {
        "what": "Arrives through the cart like ordinary state, then is deleted "
                "after use.  Transient in effect, indistinguishable from real "
                "cart state until you spot the removal.",
        "src": "hg/lib/cart.c:cartRemove",
    },
}

BOUNDARY = (
    "Anything not listed here that you put on a URL is an ordinary cart "
    "variable: it is written to the user's session and kept. That includes "
    "every per-track variable, which has its own catalog under #37838. The "
    "settings section below covers only the ones the help page advertises as "
    "URL parameters, because those are the ones users mistake for actions."
)


# ---------------------------------------------------------------------------
# GLOBAL: handled by the cart itself, so they work on any CGI
# ---------------------------------------------------------------------------

GLOBAL = {
    "what": "Handled in hg/lib/cart.c and hg/lib/web.c rather than in any one "
            "CGI, so they work on every CGI in the tree.",
    "cmds": [
        c("hgsid", "setting", "hg/lib/cart.c:1520", value="<n>",
          persists=True, public=True, verified=True,
          note="Attach to an existing session id instead of the cookie's. It "
               "is written back at cart.c:1289 because it is the session's own "
               "identity, not a user preference. Best left out of a shared "
               "link: it points at one person's session."),
        c("ignoreCookie", "action", "hg/lib/cart.c:1707",
          public=True, verified=True,
          note="Start from browser defaults, ignoring the user's cookie cart. "
               "Changes made afterwards still stick to their session, so this "
               "gives a clean first view, not a sandbox."),
        c("redirect", "action", "hg/lib/cart.c:2636", value="<host>",
          verified=True, leaks=True,
          note="Geo-mirror override, written back out as a cookie. Only "
               "meaningful when geoMirrorEnabled()."),
        c("_dumpCart", "action", "hg/lib/cart.c:2756", value="<ms>",
          verified=True, leaks=True,
          note="Write the whole cart to a trash file named by elapsed page "
               "time. Debugging aid that stays in the session once used."),
        c("_dumpToLog", "action", "hg/lib/cart.c:2744", value="<msg>",
          verified=True, leaks=True,
          note="Drop a marker line into the Apache error log."),
        c("verbose", "action", "hg/lib/cart.c:2763", value="<n>",
          verified=True, leaks=True,
          note="Verbosity level for verbose() output."),
        c("ajax", "action", "hg/lib/cart.c:2852",
          verified=True,
          note="Suppress page framing and one-time init so the response can be "
               "spliced into an existing page."),
        c("apiKey", "action", "hg/lib/cart.c:1592", value="<key>",
          verified=True, leaks=True,
          note="API authentication. Worth noting that it lands in the stored "
               "cart, unlike token below, which is explicitly cleaned out at "
               "cart.c:1805."),
        c("token", "action", "hg/lib/cart.c:1613", value="<tok>",
          verified=True, note="Login token. Removed from the cart deliberately."),
        c("captcha", "action", "hg/lib/cart.c:1607", value="<s>",
          verified=True, leaks=True,
          note="Suppresses the bot check on user id lookup."),
        c("jsh_pageVertPos", "action", "hg/lib/jsHelper.c:45", value="<n>",
          verified=True, leaks=True,
          note="Restore vertical scroll position after a form round trip."),
        c("action", "action", "hg/lib/web.c:170", value="<verb>", leaks=True,
          note="Generic dispatch used by a few of the older pages."),
    ],
}


# ---------------------------------------------------------------------------
# SESSIONS: also cart-level, so also available on any CGI
# ---------------------------------------------------------------------------

SESSIONS = {
    "what": "Session loading is handled inside cartNew(), which means these "
            "work on any CGI and not just hgSession. This is how a shared "
            "session link works.",
    "cmds": [
        c("hgS_doOtherUser", "action", "hg/lib/cart.c:1741", value="submit",
          public=True, verified=True, droppedBy="hg/lib/cart.c:713",
          note="Load a named session belonging to another user. Pair with "
               "hgS_otherUserName and hgS_otherUserSessionName."),
        c("hgS_otherUserName", "action", "hg/lib/cart.c:1743", value="<user>",
          public=True, verified=True, leaks=True, note="Session owner."),
        c("hgS_otherUserSessionName", "action", "hg/lib/cart.c:1744",
          value="<name>", public=True, verified=True, leaks=True,
          note="Session name. Only the action variable is removed after the "
               "load, so this and hgS_otherUserName stay in the session."),
        c("hgS_merge", "action", "hg/lib/cart.c:1745",
          verified=True,
          note="Merge the loaded session into the current cart instead of "
               "replacing it."),
        c("hgS_doLoadUrl", "action", "hg/lib/cart.c:1753", value="submit",
          public=True, verified=True, droppedBy="hg/lib/cart.c:1335",
          note="Load session settings from a URL. Pair with hgS_loadUrlName."),
        c("hgS_loadUrlName", "action", "hg/lib/cart.c:1756", value="<url>",
          public=True, verified=True, leaks=True,
          note="URL of a saved session settings file. Stays in the session "
               "after the load."),
        c("hgS_*", "action", "hg/hgSession/hgSession.h:19", value="<varies>",
          note="hgSession's own command family. All transient.",
          members=["hgS_doNewSession", "hgS_doSaveLocal", "hgS_doLoadLocal",
                   "hgS_doMainPage", "hgS_doSessionDetail",
                   "hgS_doSessionChange", "hgS_doReSaveSession",
                   "hgS_doSaveSessionJson", "hgS_doRenameSessionJson",
                   "hgS_load_*", "hgS_delete_*", "hgS_edit_*", "hgS_share_*",
                   "hgS_gallery_*", "hgS_showDownload_*",
                   "hgS_makeDownload_*", "hgS_doDownload_*", "hgS_cancel"]),
    ],
}


# ---------------------------------------------------------------------------
# hgTracks: navigation
# ---------------------------------------------------------------------------

HGTRACKS_NAV = {
    "what": "The zoom and scroll buttons. These are form button names rather "
            "than anything a user would type, and #37923 proposes collapsing "
            "them behind one hgt.nav= command while keeping every old name "
            "working. The dispatch is a 22-way else-if chain.",
    "cmds": [
        c("hgt.left1", "action", "hg/hgTracks/hgTracks.c:11121", verified=True,
          note="Scroll left, 10% of the window."),
        c("hgt.left2", "action", "hg/hgTracks/hgTracks.c:11119", verified=True,
          note="Scroll left, half a window."),
        c("hgt.left3", "action", "hg/hgTracks/hgTracks.c:11117", verified=True,
          note="Scroll left, a full window."),
        c("hgt.right1", "action", "hg/hgTracks/hgTracks.c:11123",
          verified=True, note="Scroll right, 10%."),
        c("hgt.right2", "action", "hg/hgTracks/hgTracks.c:11125",
          verified=True, note="Scroll right, half a window."),
        c("hgt.right3", "action", "hg/hgTracks/hgTracks.c:11127",
          verified=True, note="Scroll right, a full window."),
        c("hgt.in1", "action", "hg/hgTracks/hgTracks.c:11135", verified=True,
          note="Zoom in 1.5x."),
        c("hgt.in2", "action", "hg/hgTracks/hgTracks.c:11133", verified=True,
          note="Zoom in 3x."),
        c("hgt.in3", "action", "hg/hgTracks/hgTracks.c:11131", verified=True,
          note="Zoom in 10x."),
        c("hgt.inBase", "action", "hg/hgTracks/hgTracks.c:11129",
          verified=True, note="Zoom all the way in to base level."),
        c("hgt.out1", "action", "hg/hgTracks/hgTracks.c:11137", verified=True,
          note="Zoom out 1.5x."),
        c("hgt.out2", "action", "hg/hgTracks/hgTracks.c:11139", verified=True,
          note="Zoom out 3x."),
        c("hgt.out3", "action", "hg/hgTracks/hgTracks.c:11141", verified=True,
          note="Zoom out 10x."),
        c("hgt.out4", "action", "hg/hgTracks/hgTracks.c:11143", verified=True,
          note="Zoom out 100x."),
        c("hgt.dinkLL", "action", "hg/hgTracks/hgTracks.c:11145",
          verified=True, note="Nudge the left edge left by the dink amount."),
        c("hgt.dinkLR", "action", "hg/hgTracks/hgTracks.c:11147",
          verified=True, note="Nudge the left edge right."),
        c("hgt.dinkRL", "action", "hg/hgTracks/hgTracks.c:11149",
          verified=True, note="Nudge the right edge left."),
        c("hgt.dinkRR", "action", "hg/hgTracks/hgTracks.c:11151",
          verified=True, note="Nudge the right edge right."),
        c("hgt.toggleRevCmplDisp", "action", "hg/hgTracks/hgTracks.c:11112",
          public=True, verified=True,
          note="Flip to the reverse complement strand."),
        c("hgt.nextItem", "action", "hg/hgTracks/hgTracks.c:11155",
          value="<track>", verified=True, leaks=True,
          note="Jump to the next item in the named track."),
        c("hgt.prevItem", "action", "hg/hgTracks/hgTracks.c:11157",
          value="<track>", verified=True, leaks=True,
          note="Jump to the previous item in the named track."),
        c("hgt.jump", "action", "hg/hgTracks/hgTracks.c:95",
          note="The go button next to the position box."),
        c("hgt.refresh", "action", "hg/hgTracks/hgTracks.c:95",
          note="Plain redraw."),
        c("position", "setting", "hg/hgTracks/hgTracks.c:10837",
          value="<chrom>:<start>-<end>", persists=True, public=True,
          verified=True,
          note="Persists: it is where the user's session is parked. The "
               "special value lastDbPos restores the last position seen on "
               "this assembly."),
    ],
}


# ---------------------------------------------------------------------------
# hgTracks: which tracks are showing
# ---------------------------------------------------------------------------

HGTRACKS_TRACKSET = {
    "what": "Commands that change the visible track set as a whole. Note that "
            "these are actions while the per-track visibility they interact "
            "with persists, which is where the help page's warning about "
            "conflicting cart variables comes from.",
    "cmds": [
        c("hideTracks", "action", "hg/hgTracks/hgTracks.c:7594",
          public=True, verified=True,
          note="Hide everything, then apply whatever per-track visibility is "
               "also on the URL. Read straight from CGI here while the code "
               "around it reads the cart, so the two disagree; #37923 calls "
               "this out as a bug to fix, not a feature to preserve."),
        c("hgt.reset", "action", "hg/hgTracks/hgTracks.c:9112",
          public=True, verified=True,
          note="Reset to this assembly's default track set."),
        c("hgt.hideAll", "action", "hg/hgTracks/hgTracks.c:9110",
          public=True, verified=True, note="Hide every track."),
        c("hgt.defaultImgOrder", "action", "hg/hgTracks/hgTracks.c:9164",
          verified=True, leaks=True, note="Restore the default vertical track order."),
        c("hgt.visAllFromCt", "action", "hg/hgTracks/hgTracks.c:95",
          note="Set visibility for all custom tracks at once."),
        c("hgt.collapseGroups", "action", "hg/hgTracks/hgTracks.c:95",
          note="Collapse every track group in the track controls."),
        c("hgt.expandGroups", "action", "hg/hgTracks/hgTracks.c:95",
          note="Expand every track group."),
        c("hgt.tui", "action", "hg/hgTracks/hgTracks.c:95",
          note="Jump to the track's configuration page."),
        c("rtsLoad", "action", "hg/hgTracks/hgTracks.c:7546", value="<name>",
          verified=True,
          note="Load a Recommended Track Set by name. Removed from the cart "
               "after use."),
        c("sortExp", "action", "hg/hgTracks/hgTracks.c:5260", value="<track>",
          verified=True, note="Sort the named track by expression score."),
        c("sortSim", "action", "hg/hgTracks/hgTracks.c:5253", value="<track>",
          verified=True, note="Sort the named track by similarity."),
    ],
}


# ---------------------------------------------------------------------------
# hgTracks: image and output mode
# ---------------------------------------------------------------------------

HGTRACKS_IMAGE = {
    "what": "What kind of response hgTracks sends back. Several of these exist "
            "for the browser's own AJAX calls rather than for people.",
    "cmds": [
        c("hgt.psOutput", "action", "hg/hgTracks/hgTracks.c:6123", value="on",
          public=True, verified=True,
          note="Render to PostScript and hand back a PDF. This is what the "
               "PDF/PS link does."),
        c("hgt.contentType", "action", "hg/hgTracks/hgTracks.c:6096",
          value="html|<mime>", verified=True,
          note="Override the Content-Type of the reply."),
        c("hgt.trackImgOnly", "action", "hg/hgTracks/hgTracks.c:95",
          note="Return just the track image and its map, no page around it. "
               "Used by drag-scroll and by track reordering."),
        c("hgt.ideogramToo", "action", "hg/hgTracks/hgTracks.c:95",
          note="With trackImgOnly, include the chromosome ideogram."),
        c("hgt.trackNameFilter", "action", "hg/hgTracks/hgTracks.c:95",
          value="<track>",
          note="Draw only the named track. Used when one track is refreshed "
               "on its own."),
        c("hgt.imageV1", "action", "hg/hgTracks/hgTracks.c:9184",
          verified=True,
          note="Fall back to the pre-drag-scroll single image. Candidate for "
               "removal if nothing still asks for it."),
        c("hgt.internal", "action", "hg/hgTracks/hgTracks.c:95",
          note="Internal-only rendering flag."),
        c("hideControls", "action", "hg/hgTracks/hgTracks.c:95",
          note="Draw the image with no surrounding controls."),
        c("hgt.setWidth", "action", "hg/hgTracks/hgTracks.c:95", value="<n>",
          note="Set the image width for this request."),
        c("hgt.positionInput", "action", "hg/hgTracks/hgTracks.c:95",
          note="Which position box variant submitted the form."),
        c("dirty", "action", "hg/hgTracks/hgTracks.c:95",
          note="Legacy form-state marker."),
    ],
}


# ---------------------------------------------------------------------------
# hgTracks: transient, arriving through the cart and deleted after use
# ---------------------------------------------------------------------------

HGTRACKS_TRANSIENT = {
    "what": "These ride in as ordinary cart variables and are removed once "
            "acted on, so they behave as actions even though nothing in "
            "excludeVars says so. Route 3 in the mechanisms above.",
    "cmds": [
        c("addHighlight", "action", "hg/hgTracks/hgTracks.c:cartRemove",
          value="<db>.<chrom>:<start>-<end>#<color>", public=True,
          verified=True,
          note="Add a highlight without discarding the highlights already "
               "there. This is the difference between it and highlight=, "
               "which replaces the whole set."),
        c("findNearest", "action", "hg/hgTracks/hgTracks.c:cartRemove",
          verified=True,
          note="Snap the view to the nearest item when a search lands between "
               "features."),
        c("ss", "action", "hg/hgTracks/hgTracks.c:cartRemove",
          value="<blatResults>", verified=True,
          note="Hand-off of a BLAT result set from hgBlat into the browser."),
        c("hgTracksConfigPage", "action", "hg/hgTracks/hgTracks.c:cartRemove",
          verified=True, note="Open the main configuration page."),
        c("hgTracksConfigMultiRegionPage", "action",
          "hg/hgTracks/hgTracks.c:cartRemove", verified=True,
          note="Open the multi-region configuration dialog."),
        c("chromInfoPage", "action", "hg/hgTracks/hgTracks.c:cartRemove",
          verified=True, note="Show the chromosome list page."),
        c("nonVirtPosition", "action", "hg/hgTracks/hgTracks.c:cartRemove",
          verified=True,
          note="Real-chromosome position carried alongside a virtual one in "
               "multi-region mode."),
        c("nonVirtHighlight", "action", "hg/hgTracks/hgTracks.c:cartRemove",
          verified=True, note="Same idea for highlights."),
        c("multiRegionsBedInput", "action",
          "hg/hgTracks/hgTracks.c:cartRemove", value="<bed>", verified=True,
          note="Multi-region definition pasted in as BED text."),
        c("virtShortDesc", "action", "hg/hgTracks/hgTracks.c:cartRemove",
          verified=True, note="Label for the current virtual chromosome."),
        c("hgt.convertChromToVirtChrom", "action",
          "hg/hgTracks/hgTracks.c:cartRemove", verified=True,
          note="Translate a real position into virtual coordinates."),
        c("emGeneTable", "action", "hg/hgTracks/hgTracks.c:cartRemove",
          verified=True, note="Gene table chosen by an external tool hand-off."),
        c("gvDisclaimer", "action", "hg/hgTracks/hgTracks.c:cartRemove",
          verified=True, note="Acknowledge the variant-data disclaimer."),
    ],
}


# ---------------------------------------------------------------------------
# hgTracks: track search
# ---------------------------------------------------------------------------

TRACK_SEARCH = {
    "what": "The track search page, whose variables are declared centrally in "
            "hg/inc/search.h and excluded by hgTracks.",
    "cmds": [
        c("hgt_tSearch", "action", "hg/inc/search.h:13", value="<term>",
          verified=True, note="Search tracks in the current assembly."),
        c("hgt_tsPage", "action", "hg/inc/search.h:14", value="<n>",
          verified=True, note="Result page number."),
        c("hgt_tsAddRow", "action", "hg/inc/search.h:15", verified=True,
          note="Add a metadata criterion row to the search form."),
        c("hgt_tsDelRow", "action", "hg/inc/search.h:16", verified=True,
          note="Remove a criterion row."),
        c("hgt.suggest", "action", "hg/hgTracks/hgTracks.c:95", value="<term>",
          note="Gene-name autocomplete hand-off."),
        c("hgt.suggestTrack", "action", "hg/hgTracks/hgTracks.c:95",
          value="<track>", note="Track the suggestion came from."),
    ],
}


# ---------------------------------------------------------------------------
# hgTracks: everything else
# ---------------------------------------------------------------------------

HGTRACKS_MISC = {
    "what": "Odds and ends, several of them tied to features that are gated "
            "off or effectively retired. Good candidates for the deletion "
            "pass in #37923.",
    "cmds": [
        c("hgt.redirectTool", "action", "hg/hgTracks/mainMain.c:61",
          value="<tool>", verified=True, leaks=True,
          note="Hand the current view off to an external tool."),
        c("hgGenomeClick", "action", "hg/hgTracks/hgTracks.c:6272",
          verified=True, leaks=True, note="Arrived from hgGenome."),
        c("startTutorial", "action", "hg/hgTracks/hgTracks.c:12155",
          value="true", public=True, verified=True, leaks=True,
          note="Open the browser with the interactive tutorial running."),
        c("startClinical", "action", "hg/hgTracks/hgTracks.c:12159",
          value="true", verified=True, leaks=True, note="Clinical tutorial variant."),
        c("startCustomTutorial", "action", "hg/hgTracks/hgTracks.c:12155",
          leaks=True, note="Custom tutorial variant."),
        c("dumpTracks", "action", "hg/hgTracks/hgTracks.c:95",
          note="Debug dump of the track list. Retirement candidate."),
        c("ctTest", "action", "hg/hgTracks/hgTracks.c:95",
          note="Custom track test hook. Retirement candidate."),
        c("myVarShare", "action", "hg/hgTracks/hgTracks.c:95",
          note="myVariants sharing, gated behind the doMyVariants hg.conf "
               "flag. Retirement candidate if that flag is off everywhere."),
        c("myVarShareCmd", "action", "hg/hgTracks/mainMain.c:72",
          value="<cmd>", verified=True, leaks=True,
          note="myVariants share API command, same gating."),
        c("pubsFilterExtId", "action", "hg/hgTracks/pubsTracks.c:368",
          value="<id>", verified=True, leaks=True,
          note="Restrict the publications track to one article."),
        c("measureTiming", "setting", "hg/hgGateway/hgGateway.c:1252",
          persists=True, public=True, verified=True,
          note="Persists. Adds per-track timing to the page. Commonly pasted "
               "onto a URL for debugging and then forgotten, which is exactly "
               "the confusion this catalog is meant to remove."),
    ],
}


# ---------------------------------------------------------------------------
# loading data by URL
# ---------------------------------------------------------------------------

DATA_LOADING = {
    "what": "Attaching custom tracks and hubs. All of these are consumed and "
            "dropped, but their effect (the attached hub or track) persists in "
            "the session, which is a distinction worth spelling out on the "
            "help page.",
    "cmds": [
        c("hubUrl", "action", "hg/inc/hubConnect.h:31", value="<url>",
          public=True, verified=True,
          note="Attach a track or assembly hub. Repeatable: several hubUrl= "
               "on one URL attach several hubs."),
        c("hubClear", "action", "hg/inc/hubConnect.h:37", value="<url>",
          public=True, verified=True,
          note="Detach any hub already attached at that URL, then attach it "
               "fresh. The way to make a link that does not accumulate hubs."),
        c("hgt.customText", "action", "hg/inc/customTrack.h:68",
          value="<url>|<text>", public=True, verified=True,
          note="Load a custom track, either inline or from a URL."),
        c("hgct_customText", "action", "hg/inc/customTrack.h:70",
          value="<url>|<text>", alias="hgt.customText", deprecated=True,
          verified=True,
          note="Same thing under a second name. Both keep working; document "
               "hgt.customText only."),
        c("hgt.customFile", "action", "hg/inc/customTrack.h:71",
          value="<upload>", verified=True, note="File-upload form variant."),
        c("hgct_docText", "action", "hg/inc/customTrack.h:73", value="<html>",
          verified=True, leaks=True, note="Description page HTML for a custom track."),
        c("hgct_docFile", "action", "hg/inc/customTrack.h:74",
          value="<upload>", verified=True, leaks=True, note="Same as a file upload."),
        c("hubCheckUrl", "action", "hg/inc/hubConnect.h:28", value="<url>",
          verified=True, note="Run hubCheck against a hub and show the report."),
        c("validateHubUrl", "action", "hg/hgHubConnect/hgHubConnect.c:1620",
          value="<url>", verified=True, leaks=True, note="Hub validation entry point."),
        c("hgHubConnect.remakeTrackHub", "action", "hg/inc/hubConnect.h:103",
          value="on", verified=True,
          note="Force the hub's track list to be rebuilt rather than reused."),
        c("hgHub_do_*", "action", "hg/inc/hubConnect.h:50", value="<varies>",
          note="hgHubConnect's command family.",
          members=["hgHub_do_clear", "hgHub_do_refresh", "hgHub_do_search",
                   "hgHub_do_deleteSearch", "hgHub_do_filter",
                   "hgHub_do_disconnect", "hgHub_do_firstDb",
                   "hgHub_do_decorateDb", "hgHub_do_redirect",
                   "hgHub_do_hubCheck"]),
    ],
}


# ---------------------------------------------------------------------------
# the assembly the URL is talking about
# ---------------------------------------------------------------------------

ASSEMBLY = {
    "what": "Which genome the rest of the URL applies to. All of these persist: "
            "they are the user's current assembly, not a one-off.",
    "cmds": [
        c("db", "setting", "hg/lib/web.c:1053", value="<db>", persists=True,
          public=True, verified=True,
          note="Assembly, e.g. hg38. The one parameter almost every link "
               "should carry."),
        c("org", "setting", "hg/lib/web.c:1057", value="<organism>",
          persists=True, public=True, verified=True, note="Organism name."),
        c("clade", "setting", "hg/lib/web.c:1058", value="<clade>",
          persists=True, public=True, verified=True, note="Clade."),
        c("genome", "setting", "hg/lib/web.c:1055", value="<genome>",
          persists=True, public=True, verified=True,
          note="Assembly hub genome name, for a hub that supplies its own "
               "assembly."),
        c("singleSearch", "action", "hg/lib/hgFind.c:3865", value="<term>",
          verified=True, note="Run one search and go straight to the result."),
    ],
}


# ---------------------------------------------------------------------------
# settings the help page advertises as URL parameters
# ---------------------------------------------------------------------------

SETTINGS_ON_URL = {
    "what": "These are ordinary cart variables. They are in this catalog "
            "because the help page lists them next to the actions above with "
            "no hint that they persist, which is the specific confusion "
            "#37923 exists to fix. Setting one from a link changes that "
            "user's session until something changes it back.",
    "cmds": [
        c("pix", "setting", "hg/hgTracks/hgTracks.c", value="<n>",
          persists=True, public=True, verified=True,
          note="Image width in pixels."),
        c("textSize", "setting", "hg/hgTracks/hgTracks.c", value="<n>",
          persists=True, public=True, verified=True, note="Font size."),
        c("guidelines", "setting", "hg/hgTracks/hgTracks.c", value="on|off",
          persists=True, public=True, verified=True,
          note="The vertical blue guide lines."),
        c("ruler", "setting", "hg/hgTracks/hgTracks.c", value="hide",
          persists=True, public=True, verified=True,
          note="Hide the base position ruler."),
        c("highlight", "setting", "hg/hgTracks/hgTracks.c",
          value="<db>.<chrom>:<start>-<end>#<color>|...", persists=True,
          public=True, verified=True,
          note="Replace the highlight set. Use addHighlight to add to it "
               "instead. Colons, hashes and pipes need URL encoding."),
        c("hgFind.matches", "setting", "hg/hgTracks/hgTracks.c",
          value="<name>,<name>", persists=True, public=True, verified=True,
          note="Outline these items by name."),
        c("hgt.labelWidth", "setting", "hg/hgTracks/hgTracks.c", value="<n>",
          persists=True, public=True, verified=True,
          note="Width of the left label area, in characters. The hgt. prefix "
               "makes this look like an action; it is not."),
        c("hgt.oligoMatch", "setting", "hg/hgTracks/hgTracks.c", value="<dna>",
          persists=True, public=True, verified=True,
          note="Pattern for the Short Match track. Pair with "
               "oligoMatch=pack to make the track visible."),
        c("hgt.baseShowPos", "setting", "hg/hgTracks/hgTracks.c",
          persists=True, verified=True,
          note="Show the full position in the base track."),
        c("hgt.baseShowAsm", "setting", "hg/hgTracks/hgTracks.c",
          persists=True, verified=True,
          note="Show assembly name in the base track."),
        c("udcTimeout", "setting", "hg/lib/hui.c:648", value="<seconds>",
          persists=True, verified=True,
          note="How long to wait on a remote data file."),
        c("virtModeType", "setting", "hg/hgTracks/hgTracks.c",
          value="<mode>", persists=True, verified=True,
          note="Multi-region display mode."),
        c("multiRegionsBedUrl", "setting", "hg/hgTracks/hgTracks.c",
          value="<url>", persists=True, public=True, verified=True,
          note="Multi-region definition fetched from a URL."),
        c("<track>", "setting", "hg/hgTracks/hgTracks.c:7681",
          value="full|pack|squish|dense|hide", persists=True, public=True,
          verified=True,
          note="Per-track visibility, the most-used URL parameter of all. For "
               "a custom track the name is the generated ct_name_#### form."),
        c("<track>_imgOrd", "setting", "hg/hgTracks/imageV2.c", value="<n>",
          persists=True, public=True, verified=True,
          note="Vertical row order. Needs a value for every visible track to "
               "behave predictably."),
        c("<track>_sel", "setting", "hg/lib/hui.c:5434", value="1",
          persists=True, public=True, verified=True,
          note="Tick a member of a container so it can draw."),
        c("<track>_hideKids", "setting", "hg/hgTracks/hgTracks.c:7652",
          value="1", persists=True, public=True, verified=True,
          note="Collapse a container and hide its children."),
        c("<track>.heightPer", "setting", "hg/inc/wiggle.h", value="<n>",
          persists=True, public=True, verified=True,
          note="Track height in pixels."),
    ],
}


# ---------------------------------------------------------------------------
# other CGIs
# ---------------------------------------------------------------------------

OTHER_CGIS = {
    "hgTables": {
        "what": "The Table Browser. Its whole command family shares the "
                "hgta_do prefix and is stripped from the cart in one call at "
                "hgTables.c:1737, so every member is an action.",
        "cmds": [
            c("hgta_do*", "action", "hg/hgTables/hgTables.c:1737",
              value="<varies>", verified=True,
              note="cartRemovePrefix(cart, hgtaDo) removes the lot after "
                   "dispatch. About 50 members.",
              members=["hgta_doMainPage", "hgta_doTopSubmit",
                       "hgta_doSummaryStats", "hgta_doSchema",
                       "hgta_doSchemaTable", "hgta_doSchemaDb",
                       "hgta_doPasteIdentifiers", "hgta_doPastedIdentiers",
                       "hgta_doClearPasteIdentifierText",
                       "hgta_doUploadIdentifiers", "hgta_doClearIdentifiers",
                       "hgta_doFilterPage", "hgta_doFilterSubmit",
                       "hgta_doFilterMore", "hgta_doClearFilter",
                       "hgta_doIntersectPage", "hgta_doIntersectSubmit",
                       "hgta_doIntersectMore", "hgta_doClearIntersect",
                       "hgta_doCorrelatePage", "hgta_doCorrelateSubmit",
                       "hgta_doCorrelateMore", "hgta_doClearCorrelate",
                       "hgta_doClearContinueCorrelate",
                       "hgta_doSubtrackMergePage",
                       "hgta_doSubtrackMergeSubmit",
                       "hgta_doClearSubtrackMerge", "hgta_doValueHistogram",
                       "hgta_doValueRange", "hgta_doPrintSelectedFields",
                       "hgta_doGalaxySelectedFields",
                       "hgta_doSelectFieldsMore",
                       "hgta_doClearAllField.*", "hgta_doSetAllField.*",
                       "hgta_doGenePredSequence", "hgta_doGenomicDna",
                       "hgta_doGetBed", "hgta_doGetCustomTrackGb",
                       "hgta_doGetCustomTrackTb",
                       "hgta_doGetCustomTrackFile",
                       "hgta_doRemoveCustomTrack", "hgta_doGalaxyQuery",
                       "hgta_doGreatOutput", "hgta_doGreatQuery",
                       "hgta_doGsLogin", "hgta_doLookupPosition",
                       "hgta_doMetaData", "hgta_doSetUserRegions",
                       "hgta_doSubmitUserRegions", "hgta_doClearUserRegions",
                       "hgta_doClearSetUserRegionsText", "hgta_doPal",
                       "hgta_palOut", "hgta_doTest"]),
            c("hgta_metaStatus", "action", "hg/hgTables/hgTables.c:1434",
              verified=True, note="Metadata page state, removed after use."),
            c("hgta_metaVersion", "action", "hg/hgTables/hgTables.c:1435",
              verified=True),
            c("hgta_metaDatabases", "action", "hg/hgTables/hgTables.c:1436",
              verified=True),
            c("hgta_metaTables", "action", "hg/hgTables/hgTables.c:1437",
              verified=True),
            c("fbQual", "action", "hg/lib/featureBits.c:350",
              value="<qualifier>", verified=True, leaks=True,
              note="featureBits region qualifier for the output options page."),
            c("fbUpBases", "action", "hg/lib/featureBits.c:363", value="<n>",
              verified=True, leaks=True),
            c("fbDownBases", "action", "hg/lib/featureBits.c:367",
              value="<n>", verified=True, leaks=True),
            c("fbExonBases", "action", "hg/lib/featureBits.c:358",
              value="<n>", verified=True, leaks=True),
            c("fbIntronBases", "action", "hg/lib/featureBits.c:361",
              value="<n>", verified=True, leaks=True),
        ],
    },
    "hgc": {
        "what": "The details page. Its command is the value of g=, which "
                "selects among a long list of handlers, so g is a dispatch "
                "key rather than a boolean.",
        "cmds": [
            c("g", "action", "hg/hgc/hgc.c:28618", value="<track>|<htcVerb>",
              public=True, verified=True,
              note="Which handler runs. A track name shows that track's item "
                   "details; the htc* and get* verbs are internal pages such "
                   "as getDna and htcGeneMrna."),
            c("i", "action", "hg/hgc/hgc.c:28618", value="<item>",
              public=True, verified=True, note="Item name."),
            c("i2", "action", "hg/hgc/hgc.c", value="<item>", leaks=True,
              note="Second item, for paired features."),
            c("aliTable", "action", "hg/hgc/hgc.c:28618", value="<table>",
              verified=True, note="Alignment table to pull the alignment from."),
            c("addp", "action", "hg/hgc/hgc.c:28618", verified=True,
              note="Protein-alignment flag."),
            c("pred", "action", "hg/hgc/hgc.c:28618", value="<table>",
              verified=True, note="Gene prediction table."),
            c("quickLiftCcds", "action", "hg/hgc/hgc.c:28618", verified=True,
              note="quickLift CCDS hand-off."),
            c("doGetBed", "action", "hg/hgc/hgc.c:24549", verified=True, leaks=True,
              note="Return the feature as BED."),
            c("oldFonts", "action", "hg/hgc/barChartClick.c:384",
              verified=True, leaks=True,
              note="Draw bar chart details with the pre-FreeType fonts, added "
                   "so QA could compare. Retirement candidate."),
            c("hgSeq.maskRepeats", "action", "hg/hgc/hgc.c", verified=False, leaks=True,
              note="Mask repeats in retrieved sequence."),
        ],
    },
    "hgTrackUi": {
        "what": "The track configuration page.",
        "cmds": [
            c("g", "action", "hg/hgTrackUi/hgTrackUi.c:4643", value="<track>",
              public=True, verified=True,
              note="Which track's settings page to show."),
            c("track", "action", "hg/hgTrackUi/hgTrackUi.c:4643",
              value="<track>", verified=True, note="Alternate spelling of g."),
            c("fileUrl", "action", "hg/hgTrackUi/hgTrackUi.c:4643",
              value="<url>", verified=True,
              note="Configure a track straight from a data file URL."),
            c("sourceDb", "action", "hg/hgTrackUi/hgTrackUi.c:4643",
              value="<db>", verified=True, note="Assembly the track came from."),
        ],
    },
    "hgCustom": {
        "what": "The custom track management page.",
        "cmds": [
            c("hgct_doRemoveCustomTrack", "action",
              "hg/inc/customTrack.h:77", verified=True,
              note="Delete the selected custom track."),
            c("hgct_table", "action", "hg/inc/customTrack.h:78",
              value="<table>", verified=True, note="Which custom track."),
            c("hgct_updatedTable", "action", "hg/inc/customTrack.h:79",
              value="<table>", verified=True, leaks=True),
            c("op", "action", "hg/hgCustom/hgCustom.c", value="<verb>",
              verified=True, leaks=True, note="Which operation the page should perform."),
            c("SubmitFile", "action", "hg/hgCustom/hgCustom.c:92",
              verified=True, note="Upload button."),
            c("ContinueWithWarn", "action", "hg/hgCustom/hgCustom.c:92",
              verified=True, note="Proceed despite parse warnings."),
        ],
    },
    "hgConvert / hgLiftOver": {
        "what": "Coordinate conversion between assemblies.",
        "cmds": [
            c("hglft_doConvert", "action", "hg/hgConvert/hgConvert.c:40",
              verified=True, note="Run the conversion."),
            c("hglft_toDb", "action", "hg/hgConvert/hgConvert.c:39",
              value="<db>", verified=True, leaks=True, note="Destination assembly."),
            c("hglft_toOrg", "action", "hg/hgConvert/hgConvert.c:38",
              value="<organism>", verified=True, leaks=True),
            c("hglft_userData", "action", "hg/hgLiftOver/hgLiftOver.c:639",
              value="<text>", verified=True, note="Pasted coordinates."),
            c("hglft_dataFile", "action", "hg/hgLiftOver/hgLiftOver.c:640",
              value="<upload>", verified=True),
            c("hglft_errorHelp", "action", "hg/hgLiftOver/hgLiftOver.c:641",
              verified=True, note="Show the error help text."),
        ],
    },
    "hgBlat / hgPcr": {
        "what": "Sequence alignment and primer search.",
        "cmds": [
            c("userSeq", "action", "hg/hgBlat/hgBlat.c:2616", value="<seq>",
              public=True, verified=True, note="Query sequence."),
            c("type", "action", "hg/hgBlat/hgBlat.c:2616", value="<queryType>",
              verified=True, note="Query type, e.g. DNA or protein."),
            c("seqFile", "action", "hg/hgBlat/hgBlat.c:2616",
              value="<upload>", verified=True),
            c("Lucky", "action", "hg/hgBlat/hgBlat.c:2616", verified=True,
              note="Jump straight to the best hit."),
            c("Clear", "action", "hg/hgBlat/hgBlat.c:2616", verified=True),
            c("showPage", "action", "hg/hgBlat/hgBlat.c:2616", verified=True),
            c("changeInfo", "action", "hg/hgBlat/hgBlat.c:2616",
              verified=True),
            c("blatNewPage", "setting", "hg/hgBlat/hgBlat.c:776", value="0|1",
              persists=True, verified=True,
              note="Which results page to draw: the classic hyperlink list or "
                   "the sortable table.  A preference on purpose, so the "
                   "choice sticks for later searches, and hgBlat carries it "
                   "across a session load (hgBlat.c:2679).  hgc reads it too, "
                   "to match the alignment display (hgc.c:9234)."),
            c("blatReopen", "action", "hg/hgBlat/hgBlat.c:2707",
              verified=True,
              note="Re-render the results already in the cart, rather than "
                   "running a new search.  How the banner and the table's "
                   "'Old BLAT result page' link switch format without "
                   "resubmitting the query."),
            c("wp_f", "action", "hg/hgPcr/hgPcr.c:847", value="<primer>",
              public=True, verified=True, note="Forward primer."),
            c("wp_r", "action", "hg/hgPcr/hgPcr.c:847", value="<primer>",
              public=True, verified=True, note="Reverse primer."),
            c("wp_showPage", "action", "hg/hgPcr/hgPcr.c:847", verified=True),
        ],
    },
    "hgGateway / hgSearch / hgChooseDb": {
        "what": "The gateway and search pages, which share a cartJson command "
                "channel.",
        "cmds": [
            c("hgt_tSearch", "action", "hg/hgGateway/hgGateway.c:1250",
              value="<term>", verified=True,
              note="Search term. Same variable name the track search uses."),
            c("cjCmd", "action", "hg/inc/cartJson.h:11", value="<json>",
              verified=True,
              note="cartJson command channel, used by the page's own AJAX."),
            c("search", "action", "hg/hgSearch/hgSearch.c:563", value="<term>",
              public=True, verified=True, leaks=True,
              note="Search term for the search results page."),
        ],
    },
    "hgIntegrator / hgVai": {
        "what": "Data Integrator and Variant Annotation Integrator.",
        "cmds": [
            c("hgi_doQuery", "action", "hg/hgIntegrator/hgIntegrator.c:48",
              verified=True, note="Run the integrator query."),
            c("hgva_startQuery", "action", "hg/hgVai/hgVai.c:68",
              verified=True, note="Run the annotation query."),
        ],
    },
    "hgCollection": {
        "what": "The track collection builder.",
        "cmds": [
            c("cmd", "action", "hg/hgCollection/hgCollection.c:32",
              value="<verb>", verified=True, note="Which operation to run."),
            c("track", "action", "hg/hgCollection/hgCollection.c:32",
              value="<track>", verified=True),
            c("collection", "action", "hg/hgCollection/hgCollection.c:32",
              value="<name>", verified=True),
            c("jsonp", "action", "hg/hgCollection/hgCollection.c:32",
              value="<callback>", verified=True,
              note="JSONP callback name for the AJAX replies."),
        ],
    },
    "hgApi": {
        "what": "The older internal JSON helper, distinct from hubApi.",
        "cmds": [
            c("cmd", "action", "hg/hgApi/hgApi.c:288", value="<verb>",
              verified=True, note="Which query to answer."),
            c("track", "action", "hg/hgApi/hgApi.c:288", value="<track>",
              verified=True),
            c("table", "action", "hg/hgApi/hgApi.c:288", value="<table>",
              verified=True),
            c("name", "action", "hg/hgApi/hgApi.c:288", value="<name>",
              verified=True),
            c("chrom", "action", "hg/hgApi/hgApi.c:288", value="<chrom>",
              verified=True),
            c("num", "action", "hg/hgApi/hgApi.c:288", value="<n>",
              verified=True),
            c("var", "action", "hg/hgApi/hgApi.c:288", value="<var>",
              verified=True),
            c("offset", "action", "hg/hgApi/hgApi.c", value="<n>",
              verified=True, leaks=True),
            c("symbol", "action", "hg/hgApi/hgApi.c", value="<sym>",
              verified=True, leaks=True),
            c("fileSearch", "action", "hg/hgApi/hgApi.c:288", verified=True),
            c("showShortLabel", "action", "hg/hgApi/hgApi.c:288",
              verified=True),
            c("showLongLabel", "action", "hg/hgApi/hgApi.c:288",
              verified=True),
        ],
    },
    "smaller CGIs": {
        "what": "One or two commands each.",
        "cmds": [
            c("resource", "action", "hg/hgLinkIn/hgLinkIn.c:121",
              value="<kind>", public=True, verified=True,
              note="hgLinkIn: what kind of external identifier follows."),
            c("id", "action", "hg/hgLinkIn/hgLinkIn.c:121", value="<id>",
              public=True, verified=True,
              note="hgLinkIn: the external identifier to resolve."),
            c("ajaxSection", "action", "hg/hgGene/hgGene.h:264",
              value="<section>", verified=True,
              note="hgGene: render one section of the gene page."),
            c("showAllRef", "action", "hg/hgGene/gad.c:62", value="Y",
              verified=True, leaks=True, note="hgGene: show all references, not the first few."),
            c("clearCache", "action", "hg/hgFileUi/hgFileUi.c:170",
              verified=True, note="hgFileUi and hgFileSearch: drop cached metadata."),
            c("noDisplay", "action", "hg/cartDump/cartDump.c:253",
              verified=True,
              note="cartDump: do the work without printing the cart."),
            c("backgroundExec", "action", "hg/hgSession/hgSession.c:2030",
              value="<cmd>", verified=True,
              note="hgSession: run a long save in the background."),
            c("backgroundProgress", "action", "hg/hgSession/hgSession.c:1923",
              value="<url>", verified=True,
              note="hgSession: poll a background job's progress."),
            c("hgS_sessionDataDbSuffix", "action",
              "hg/hgSession/hgSession.c:59", value="<suffix>", verified=True),
            c("hgLogin.do.*", "action", "hg/hgLogin/hgLogin.c:43",
              value="<varies>", note="hgLogin's command family."),
            c("debug", "action", "hg/hgTracks/extTools.c:177", verified=True,
              note="extTools and a few others: show debugging output."),
            c("prefix", "action", "hg/hgSuggest/hgSuggest.c", value="<text>",
              verified=True, nocart=True,
              note="hgSuggest: gene name prefix to complete. The whole CGI is "
                   "URL-driven and keeps no cart."),
        ],
    },
}


# ---------------------------------------------------------------------------
# hubApi, which is a REST API rather than a cart CGI
# ---------------------------------------------------------------------------

HUBAPI = {
    "what": "The public REST API at /cgi-bin/hubApi. The endpoint is chosen by "
            "path (/list/..., /getData/..., /search, /findGenome) and these are "
            "its query arguments. No cart is involved, so nothing here "
            "persists. Names come from hg/hubApi/dataApi.h.",
    "cmds": [
        # documented on goldenPath/help/api.html
        c("hubUrl", "action", "hg/hubApi/dataApi.h:66", value="<url>",
          public=True, verified=True,
          note="Track hub or assembly hub URL."),
        c("genome", "action", "hg/hubApi/dataApi.h:67", value="<name>",
          public=True, verified=True,
          note="Assembly in the browser or in a hub. With "
               "/list/genarkGenomes it tests whether that genome exists."),
        c("track", "action", "hg/hubApi/dataApi.h:69", value="<trackName>",
          public=True, verified=True,
          note="Which data track, in a hub or a browser assembly."),
        c("trackLeavesOnly", "action", "hg/hubApi/dataApi.h:68", value="1",
          public=True, verified=True,
          note="On /list/tracks, list only tracks and omit the container "
               "information."),
        c("chrom", "action", "hg/hubApi/dataApi.h:70", value="<chrN>",
          public=True, verified=True,
          note="Chromosome, for sequence or track data."),
        c("start", "action", "hg/hubApi/dataApi.h:71", value="<n>",
          public=True, verified=True,
          note="Start coordinate. Zero-based here, unlike the browser's own "
               "display."),
        c("end", "action", "hg/hubApi/dataApi.h:72", value="<n>",
          public=True, verified=True, note="End coordinate, one-based."),
        c("revComp", "action", "hg/hubApi/dataApi.h:73", value="1",
          public=True, verified=True,
          note="On /getData/sequence, return the reverse complement."),
        c("maxItemsOutput", "action", "hg/hubApi/dataApi.h:74", value="<n>",
          public=True, verified=True,
          note="Cap the number of items returned. Default and maximum are "
               "1,000,000; -1 asks for the maximum."),
        c("format", "action", "hg/hubApi/dataApi.h:75", value="text",
          public=True, verified=True,
          note="On /list/files, return a plain text listing instead of JSON."),
        c("jsonOutputArrays", "action", "hg/hubApi/dataApi.h:76", value="1",
          public=True, verified=True,
          note="On /getData/track, return each item as an array rather than "
               "an object."),
        c("search", "action", "hg/hubApi/dataApi.h:78", value="<term>",
          public=True, verified=True,
          note="On /search, the term to look for. Needs genome= too."),
        c("categories", "action", "hg/hubApi/dataApi.h:77",
          value="helpDocs|publicHubs|trackDb", public=True, verified=True,
          note="On /search, restrict the search to one category."),
        c("q", "action", "hg/hubApi/dataApi.h:81", value="<query>",
          public=True, verified=True,
          note="On /findGenome, the assembly search string. Supports +term, "
               "-term and trailing wildcards."),
        c("statsOnly", "action", "hg/hubApi/dataApi.h:82", value="1",
          public=True, verified=True,
          note="On /findGenome, return counts rather than the matches."),
        c("browser", "action", "hg/hubApi/dataApi.h:83", value="<value>",
          public=True, verified=True,
          note="On /findGenome, restrict by whether a browser exists."),
        c("year", "action", "hg/hubApi/dataApi.h:84", value="<yyyy>",
          public=True, verified=True,
          note="On /findGenome, restrict to assemblies from one year."),
        c("category", "action", "hg/hubApi/dataApi.h:85",
          value="reference|representative", public=True, verified=True,
          note="On /findGenome, restrict by NCBI assembly category."),
        c("status", "action", "hg/hubApi/dataApi.h:86",
          value="reference|representative", public=True, verified=True,
          note="On /findGenome, restrict by NCBI assembly status."),
        c("level", "action", "hg/hubApi/dataApi.h:87",
          value="complete|chromosome|scaffold|contig", public=True,
          verified=True,
          note="On /findGenome, restrict by NCBI assembly level."),

        # present in dataApi.h but absent from the public API help page
        c("skipContext", "action", "hg/hubApi/dataApi.h:79", value="1",
          note="Not documented on the API help page."),
        c("liftable", "action", "hg/hubApi/dataApi.h:88", value="1",
          note="Not documented on the API help page."),
        c("fromGenome", "action", "hg/hubApi/dataApi.h:89", value="<name>",
          note="Lift source assembly. Not documented on the help page."),
        c("toGenome", "action", "hg/hubApi/dataApi.h:90", value="<name>",
          note="Lift destination assembly. Not documented on the help page."),
        c("filter", "action", "hg/hubApi/dataApi.h:103", value="<expr>",
          note="Not documented on the API help page."),
        c("fileType", "action", "hg/hubApi/dataApi.h:105", value="<type>",
          note="Not documented on the API help page."),
        c("userSeq", "action", "hg/hubApi/dataApi.h:107", value="<seq>",
          note="Not documented on the API help page."),
        c("apiKey", "action", "hg/hubApi/dataApi.h:108", value="<key>",
          note="Not documented on the API help page."),
        c("asmId", "action", "hg/hubApi/dataApi.h:96", value="<id>",
          note="Assembly request endpoint, not the public data API."),
        c("name", "action", "hg/hubApi/dataApi.h:97", value="<name>",
          note="Assembly request endpoint."),
        c("betterName", "action", "hg/hubApi/dataApi.h:98", value="<name>",
          note="Assembly request endpoint."),
        c("email", "action", "hg/hubApi/dataApi.h:92", value="<addr>",
          note="Assembly request endpoint."),
        c("comment", "action", "hg/hubApi/dataApi.h:93", value="<text>",
          note="Assembly request endpoint."),
        c("returnTo", "action", "hg/hubApi/dataApi.h:94", value="<url>",
          note="Assembly request endpoint."),
        c("requestType", "action", "hg/hubApi/dataApi.h:100", value="<type>",
          note="ottoRequest relay, used between the Euro and Asia nodes and "
               "the RR rather than by users."),
        c("relaySecret", "action", "hg/hubApi/dataApi.h:101", value="<secret>",
          note="ottoRequest relay shared secret. Internal."),
    ],
}


# ---------------------------------------------------------------------------
# assembly
# ---------------------------------------------------------------------------

SECTIONS = [
    ("Works on any CGI", GLOBAL),
    ("Sessions", SESSIONS),
    ("Which assembly", ASSEMBLY),
    ("hgTracks: navigation", HGTRACKS_NAV),
    ("hgTracks: which tracks show", HGTRACKS_TRACKSET),
    ("hgTracks: image and output", HGTRACKS_IMAGE),
    ("hgTracks: consumed then dropped", HGTRACKS_TRANSIENT),
    ("hgTracks: track search", TRACK_SEARCH),
    ("hgTracks: everything else", HGTRACKS_MISC),
    ("Loading custom tracks and hubs", DATA_LOADING),
    ("Settings the help page calls URL parameters", SETTINGS_ON_URL),
]


def build():
    # hubApi is a REST API with no cart behind it, so none of its arguments can
    # persist and the audit must not expect an excludeVars entry for them.
    for e in HUBAPI["cmds"]:
        e["nocart"] = True
    cat = {
        "ticket": "#37923",
        "mechanisms": MECHANISMS,
        "boundary": BOUNDARY,
        "sections": [{"title": t, "what": s["what"], "cmds": s["cmds"]}
                     for t, s in SECTIONS],
        "otherCgis": {k: v for k, v in OTHER_CGIS.items()},
        "hubApi": HUBAPI,
    }
    return cat


def all_cmds(cat):
    out = []
    for s in cat["sections"]:
        out.extend(s["cmds"])
    for g in cat["otherCgis"].values():
        out.extend(g["cmds"])
    out.extend(cat["hubApi"]["cmds"])
    return out


def counts(cat):
    cmds = all_cmds(cat)
    pub = [e for e in cmds if e["public"]]
    return {
        "total": len(cmds),
        "actions": len([e for e in cmds if e["kind"] == "action"]),
        "settings": len([e for e in cmds if e["kind"] == "setting"]),
        "persists": len([e for e in cmds if e["persists"]]),
        "leaks": len([e for e in cmds if e.get("leaks")]),
        "public": len(pub),
        "publicUnverified": len([e for e in pub if not e["verified"]]),
        "unverified": len([e for e in cmds if not e["verified"]]),
        "deprecated": len([e for e in cmds if e.get("deprecated")]),
        "familyMembers": sum(len(e.get("members", [])) for e in cmds),
        "sections": len(cat["sections"]) + len(cat["otherCgis"]) + 1,
    }


# ---------------------------------------------------------------------------
# checks
# ---------------------------------------------------------------------------

def check(cat, out=sys.stderr):
    """Internal consistency.  Returns a problem count."""
    bad = 0
    cmds = all_cmds(cat)

    for e in cmds:
        if e["kind"] not in ("action", "setting"):
            print("bad kind %r on %s" % (e["kind"], e["name"]), file=out)
            bad += 1
        # an action that persists by design is a contradiction; an action that
        # leaks is a defect we are recording on purpose, so leaks and persists
        # are mutually exclusive
        if e["kind"] == "action" and e["persists"]:
            print("%s is an action but marked persists; did you mean leaks?"
                  % e["name"], file=out)
            bad += 1
        if e.get("leaks") and e["persists"]:
            print("%s cannot both leak and persist by design" % e["name"],
                  file=out)
            bad += 1
        if e.get("leaks") and e["kind"] != "action":
            print("%s is marked leaks but is not an action" % e["name"],
                  file=out)
            bad += 1
        if e["kind"] == "setting" and not e["persists"]:
            print("%s is a setting but not marked persists" % e["name"],
                  file=out)
            bad += 1
        if e["public"] and not e.get("note"):
            print("%s is public but has no description" % e["name"], file=out)
            bad += 1
        if not e["src"]:
            print("%s has no src" % e["name"], file=out)
            bad += 1

    # an alias must point at something real
    names = set(e["name"] for e in cmds)
    for e in cmds:
        if e.get("alias") and e["alias"] not in names:
            print("%s aliases unknown %s" % (e["name"], e["alias"]), file=out)
            bad += 1

    # duplicate names inside one section are a curation slip
    for s in cat["sections"]:
        seen = set()
        for e in s["cmds"]:
            if e["name"] in seen:
                print("duplicate %s in section %r" % (e["name"], s["title"]),
                      file=out)
                bad += 1
            seen.add(e["name"])
    return bad


def read_baseline(path=BASELINE_FILE):
    """The tree names already known to be outside the catalog's scope."""
    names = set()
    try:
        with open(path) as f:
            for line in f:
                line = line.split("#", 1)[0].strip()
                if line:
                    names.add(line)
    except OSError:
        pass                    # no baseline yet: every tree name is new
    return names


def write_baseline(names, sites=None, path=BASELINE_FILE):
    """Write the baseline, annotating each name with the file that reads it.

    The comment carries the file but not the line, so that ordinary edits above
    a read do not rewrite hundreds of lines here and bury the one name that
    actually changed.
    """
    with open(path, "w") as f:
        f.write("""\
# urlNamesNotCataloged.txt - CGI variable names the tree reads that
# urlCommandCatalog.py deliberately does not describe: form fields, debug
# switches, and the arguments of utility CGIs that are not browser URL
# parameters.  Refs #37923.
#
# urlCommandCatalog.py --reconcile complains about any name in neither the
# catalog nor this file, so this is what keeps a nightly run quiet until
# something actually changes.  Regenerate with --update-baseline, then read the
# diff before committing: a name appearing here is a decision that it does not
# belong on a URL, and a name disappearing means its read went away.
#
# The first version of this file was accepted wholesale, as a snapshot of the
# gap on the day reconcile learned to fail.  So a name being in here is not
# evidence that anybody has looked at it; only the ones added since, which
# arrive a few at a time in a reviewable diff, carry that weight.
#
# The comment on each line is the file that reads it, as a hint for whoever
# reviews the next change to this file.  Names under hg/hgTracks, hg/hgc,
# hg/hgTrackUi, hg/hgTables and hg/lib are worth a second look, since those are
# the CGIs whose parameters the catalog is supposed to cover.
""")
        sites = sites or {}
        for n in sorted(names):
            where = sites.get(n, "").rsplit(":", 1)[0]
            if where:
                f.write("%-34s # %s\n" % (n, where))
            else:
                f.write("%s\n" % n)


def tree_names(cat):
    """(tree names, name -> file:line, catalog names, raw harvest), or None.

    None means the harvester could not be imported.  Shared by reconcile and
    --update-baseline so the two cannot disagree about what counts as a name.
    """
    try:
        import harvestUrlCommands as h
    except ImportError:
        print("harvestUrlCommands.py not importable from here", file=sys.stderr)
        return None
    found, _ = h.harvest()

    # stand-ins that are not literal parameter names, and {ident} names the
    # harvester could not resolve to a literal
    def literal(n):
        return not any(ch in n for ch in "<*") and not n.startswith("{")

    site = {}
    for which in found:
        for pairs in found[which].values():
            for name, src in pairs:
                site.setdefault(name, src)

    cat_names = set()
    for e in all_cmds(cat):
        cat_names.add(e["name"])
        for m in e.get("members", []):
            cat_names.add(m)

    tree = set(n for n in h.all_names(found) if literal(n))
    return tree, site, cat_names, found


def reconcile(cat, out=sys.stdout, verbose=False):
    """Diff the catalog against what the tree actually says.

    This is the loop the whole design is for: the harvester finds what is
    there, the catalog says what it means, and this reports where they have
    drifted apart.

    Two of those differences mean somebody has to do something:

      1. a name the tree reads that is in neither the catalog nor the baseline,
         which is a parameter that arrived without anybody saying what it is
      2. a disagreement between a persists=False claim and what the tree
         actually excludes or removes, in either direction

    The rest is standing drift.  A catalog name with no read found is usually a
    family stand-in like hgta_do*, and the several hundred tree names outside the
    catalog's scope are in the baseline; both would print identically on every
    run, so they go out only under --verbose.  Silent and 0 means nothing new.
    """
    t = tree_names(cat)
    if t is None:
        return 1
    tree, site, cat_names, found = t

    # A scan that finds almost nothing is a broken scan, not a clean tree, and
    # the difference matters: pointed at the wrong KENT_SRC or an empty clone,
    # everything below would come up empty and report all clear forever.
    if len(tree) < MIN_TREE_NAMES:
        print("only %d names found: expected at least %d, so the scan is "
              "broken rather\nthan the tree being clean.  Check KENT_SRC."
              % (len(tree), MIN_TREE_NAMES), file=out)
        return 1

    baseline = read_baseline()

    only_cat = sorted(n for n in cat_names - tree
                      if not any(ch in n for ch in "<*"))
    new = sorted(tree - cat_names - baseline)
    problems = len(new)

    if verbose:
        print("catalog names      %d" % len(cat_names), file=out)
        print("tree names         %d" % len(tree), file=out)
        print("both               %d" % len(cat_names & tree), file=out)
        print("baseline names     %d" % len(baseline), file=out)
        print("\nin catalog, not found in tree (%d)" % len(only_cat), file=out)
        for n in only_cat:
            print("    %s" % n, file=out)
        known = sorted((tree - cat_names) & baseline)
        print("\nin tree, in the baseline rather than the catalog (%d)"
              % len(known), file=out)
        for n in known:
            print("    %-28s %s" % (n, site.get(n, "")), file=out)

    if new:
        print("\nread by the tree, in neither the catalog nor the baseline "
              "(%d):" % len(new), file=out)
        print("    (describe each in urlCommandCatalog.py if it can go on a "
              "browser URL,\n     otherwise accept it with "
              "--update-baseline)", file=out)
        for n in new:
            print("    %-28s %s" % (n, site.get(n, "")), file=out)

    problems += persistence_audit(cat, found, out=out, verbose=verbose)
    return problems


def persistence_audit(cat, found, out=sys.stdout, verbose=False):
    """Check every persists=False claim against what the tree actually does.

    cartExclude() is the only thing that keeps a CGI variable out of the saved
    cart (hg/lib/cart.c:1800), and cartRemove() is the only thing that takes one
    back out afterwards.  So a parameter persists unless it appears in some
    CGI's excludeVars[] or is explicitly removed.  Anything this flags is a
    parameter that reads like a one-shot command but is quietly being written
    into the user's session, which is the class of bug #37923 is chasing.

    Returns the number of disagreements found, either direction, and prints
    them.  The count of leaks already recorded in the catalog is not one: those
    are known and are the finding of #37923, not news.
    """
    dropped = set()
    for which in ("excludeVars", "cartRemoves"):
        for pairs in found[which].values():
            dropped.update(n for n, _ in pairs)

    unclaimed, overclaimed = [], []
    for e in all_cmds(cat):
        if e["persists"] or e.get("nocart") or e.get("droppedBy"):
            continue
        name = e["name"]
        if any(ch in name for ch in "<*"):      # family stand-in, not literal
            continue
        if name not in dropped and not e.get("leaks"):
            unclaimed.append(e)
        elif name in dropped and e.get("leaks"):
            overclaimed.append(e)

    if unclaimed:
        print("\nleaks into the session but not marked leaks=True (%d)"
              % len(unclaimed), file=out)
        print("    (nothing excludes or removes these, so they are being "
              "written into the\n     user's session; mark them leaks=True or "
              "fix the CGI)", file=out)
        for e in unclaimed:
            print("    %-28s %s" % (e["name"], e["src"]), file=out)
    if overclaimed:
        print("\nmarked leaks=True but the tree does drop them (%d)"
              % len(overclaimed), file=out)
        print("    (somebody fixed these; drop leaks=True from the catalog "
              "row)", file=out)
        for e in overclaimed:
            print("    %-28s %s" % (e["name"], e["src"]), file=out)

    if verbose:
        n_leak = len([e for e in all_cmds(cat) if e.get("leaks")])
        print("\npersistence audit: %d known leaks, %d unrecorded, %d stale"
              % (n_leak, len(unclaimed), len(overclaimed)), file=out)
    return len(unclaimed) + len(overclaimed)


# ---------------------------------------------------------------------------
# HTML rendering
# ---------------------------------------------------------------------------

CSS = """
body { font: 14px/1.5 -apple-system, "Segoe UI", Helvetica, Arial, sans-serif;
       margin: 0; color: #12191f; background: #fff; }
header { background: #14385c; color: #fff; padding: 18px 28px; }
header h1 { margin: 0 0 4px; font-size: 20px; font-weight: 600; }
header p { margin: 0; font-size: 13px; color: #c5d6e8; max-width: 90ch; }
main { max-width: 1080px; margin: 0 auto; padding: 22px 28px 60px; }
h2 { font-size: 17px; margin: 30px 0 6px; padding-bottom: 5px;
     border-bottom: 2px solid #14385c; }
h3 { font-size: 15px; margin: 20px 0 4px; color: #14385c; }
p.what { margin: 4px 0 10px; color: #4a5764; font-size: 13px;
         max-width: 82ch; }
table { border-collapse: collapse; width: 100%; margin: 6px 0 14px;
        font-size: 13px; }
th { text-align: left; background: #eef2f6; padding: 5px 8px;
     border-bottom: 1px solid #c9d3dd; font-weight: 600; }
td { padding: 5px 8px; border-bottom: 1px solid #eceff2;
     vertical-align: top; }
tr:hover td { background: #f7fafd; }
code, .n { font-family: ui-monospace, Menlo, Consolas, monospace;
           font-size: 12.5px; }
.n { font-weight: 600; color: #0b3d62; white-space: nowrap; }
.v { color: #7a3ba8; white-space: nowrap; font-family: ui-monospace, Menlo,
     Consolas, monospace; font-size: 12.5px; }
.src { color: #6b7885; font-size: 11.5px; white-space: nowrap; }
.note { color: #4a5764; font-size: 12.5px; }
.persist { background: #fff8e1; }
.leak { background: #fdf1f1; }
.lk { background: #fbe3e3; color: #8f2b2b; }
.pill { display: inline-block; border-radius: 9px; padding: 1px 8px;
        font-size: 11.5px; margin-right: 5px; }
.act { background: #e6f2ea; color: #1d5c37; }
.set { background: #fdeccd; color: #8a5a09; }
.dep { background: #fbe3e3; color: #8f2b2b; }
.unv { background: #eceff2; color: #6b7885; }
.members { color: #4a5764; font-size: 12px; font-family: ui-monospace, Menlo,
           Consolas, monospace; }
.legend { background: #f7fafd; border: 1px solid #dde5ec; padding: 12px 14px;
          font-size: 13px; margin: 12px 0 4px; }
.legend b { color: #14385c; }
#filter { width: 340px; padding: 6px 9px; font-size: 13px;
          border: 1px solid #b9c4cf; border-radius: 4px; margin: 10px 0; }
.hidden { display: none; }
.warn { background: #fbe3e3; border: 1px solid #e9b8b8; padding: 10px 14px;
        font-size: 13px; margin: 12px 0; color: #7a2020; }
"""


def esc(s):
    return html.escape(str(s), quote=False)


def cmd_rows(cmds):
    out = []
    for e in cmds:
        cls = ""
        if e["persists"]:
            cls = ' class="persist"'
        elif e.get("leaks"):
            cls = ' class="leak"'
        bits = []
        if e["kind"] == "action":
            bits.append('<span class="pill act">one-shot</span>')
        else:
            bits.append('<span class="pill set">persists</span>')
        if e.get("leaks"):
            bits.append('<span class="pill lk">leaks into session</span>')
        if e.get("deprecated"):
            bits.append('<span class="pill dep">deprecated</span>')
        if not e["verified"]:
            bits.append('<span class="pill unv">unverified</span>')
        if e.get("alias"):
            bits.append('<span class="note">same as <code>%s</code></span>'
                        % esc(e["alias"]))
        if e.get("note"):
            bits.append('<span class="note">%s</span>' % esc(e["note"]))
        if e.get("members"):
            bits.append('<div class="members">%s</div>'
                        % esc("  ".join(e["members"])))
        out.append(
            "<tr%s><td class='n'>%s</td><td class='v'>%s</td>"
            "<td>%s</td><td class='src'>%s</td></tr>"
            % (cls, esc(e["name"]), esc(e["value"]), " ".join(bits),
               esc(e["src"])))
    return "\n".join(out)


def table(cmds):
    if not cmds:
        return "<p class='note'>None.</p>"
    return ("<table><tr><th>parameter</th><th>value</th>"
            "<th>what it does</th><th>read at</th></tr>%s</table>"
            % cmd_rows(cmds))


def render_html(cat):
    n = counts(cat)
    p = []
    p.append("<title>Genome Browser URL parameters (#37923)</title>")
    p.append("<style>%s</style>" % CSS)
    p.append("<header><h1>Genome Browser URL parameters</h1>"
             "<p>Refs #37923 &mdash; generated from urlCommandCatalog.py, not "
             "hand-edited. %d parameters: %d one-shot commands and %d that "
             "persist in the user's session.</p></header>"
             % (n["total"], n["actions"], n["settings"]))
    p.append("<main>")

    if n["publicUnverified"]:
        p.append("<div class='warn'><b>Not ready to publish.</b> %d of the %d "
                 "public parameters have not had their persistence confirmed "
                 "against the code yet. A wrong persistence column is worse "
                 "than the ambiguous page this replaces, so the public help "
                 "page should keep pointing at the hand-written list until "
                 "this reads zero.</div>" % (n["publicUnverified"], n["public"]))

    p.append("<div class='legend'>"
             "<p><span class='pill act'>one-shot</span> The parameter asks for "
             "something to happen. It is consumed by that one request and is "
             "gone afterwards.</p>"
             "<p><span class='pill set'>persists</span> The parameter sets a "
             "cart variable. It is saved in the user's session and stays until "
             "something changes it. Rows shaded yellow are these.</p>"
             "<p><span class='pill lk'>leaks into session</span> Reads as a "
             "one-shot command, but nothing excludes or removes it, so it is "
             "written into the session anyway. %d of these. They are defects "
             "rather than features, and the audit in --reconcile keeps the "
             "list honest against the code.</p>"
             "<p><span class='pill unv'>unverified</span> The one-shot vs "
             "persists call has not been confirmed against the code yet.</p>"
             "<p>%s</p></div>" % (n["leaks"], esc(cat["boundary"])))

    p.append("<h2>How a parameter reaches a CGI</h2>")
    for key, m in cat["mechanisms"].items():
        p.append("<p class='what'><b>%s.</b> %s <span class='src'>%s</span></p>"
                 % (esc(key), esc(m["what"]), esc(m["src"])))

    p.append("<input id='filter' placeholder='filter parameters...'>")

    for s in cat["sections"]:
        p.append("<h2>%s</h2>" % esc(s["title"]))
        p.append("<p class='what'>%s</p>" % esc(s["what"]))
        p.append(table(s["cmds"]))

    p.append("<h2>Other CGIs</h2>")
    for name in sorted(cat["otherCgis"]):
        g = cat["otherCgis"][name]
        p.append("<h3>%s</h3>" % esc(name))
        p.append("<p class='what'>%s</p>" % esc(g["what"]))
        p.append(table(g["cmds"]))

    p.append("<h2>hubApi</h2>")
    p.append("<p class='what'>%s</p>" % esc(cat["hubApi"]["what"]))
    p.append(table(cat["hubApi"]["cmds"]))

    p.append("</main>")
    p.append("""<script>
document.getElementById('filter').addEventListener('input', function() {
    var q = this.value.toLowerCase();
    document.querySelectorAll('table tr').forEach(function(tr) {
        if (tr.querySelector('th')) return;
        tr.classList.toggle('hidden',
            q && tr.textContent.toLowerCase().indexOf(q) < 0);
    });
});
</script>""")
    return "\n".join(p)


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--json")
    ap.add_argument("--html")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--reconcile", action="store_true")
    ap.add_argument("--verbose", action="store_true",
                    help="with --reconcile, also print the standing drift "
                         "that needs no action")
    ap.add_argument("--update-baseline", dest="updateBaseline",
                    action="store_true",
                    help="rewrite %s from the current tree; read the diff "
                         "before committing it"
                         % os.path.basename(BASELINE_FILE))
    args = ap.parse_args()

    cat = build()

    if args.json:
        with open(args.json, "w") as f:
            json.dump(cat, f, indent=1)
        print("wrote %s" % args.json)
    if args.html:
        with open(args.html, "w") as f:
            f.write(render_html(cat))
        print("wrote %s" % args.html)
    if args.updateBaseline:
        t = tree_names(cat)
        if t is None:
            return 1
        tree, site, cat_names, _ = t
        was = read_baseline()
        now = tree - cat_names
        write_baseline(now, site)
        print("wrote %s: %d names, %d added, %d dropped"
              % (BASELINE_FILE, len(now), len(now - was), len(was - now)))
        return 0
    if args.reconcile:
        return 1 if reconcile(cat, verbose=args.verbose) else 0
    if args.check or not (args.json or args.html):
        c = counts(cat)
        for k in sorted(c):
            print("%-18s %s" % (k, c[k]))
        bad = check(cat)
        if bad:
            print("%d problems" % bad, file=sys.stderr)
            return 1
        print("catalog ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
