#!/usr/bin/env python3
"""hgConfCatalog.py - the registry of hg.conf variables.

Third of the browser configuration inventories, after cartTrackVarCatalog.py
(#37838, track-scoped cart variables) and urlCommandCatalog.py (#37923, CGI URL
parameters).  This one covers hg.conf: the per-machine configuration file every
CGI reads at startup through the cfgOption* accessors in hg/lib/hgConfig.c.

What makes hg.conf different from the other two, and the reason this file
needed a mechanism the others do not have:

  A cart variable belongs to a user and a URL parameter belongs to a request.
  An hg.conf variable belongs to a machine, and a machine we do not control.
  Mirrors, the GBiB and the GBiC all carry their own hg.conf, so a variable is
  reachable long after the tree stops caring about it, and deleting one is a
  compatibility decision rather than a cleanup.

  On top of that, the tree deliberately manufactures short-lived hg.conf
  variables as part of the release process.  A user-visible feature is expected
  to ship dark behind cfgOptionBooleanDefault(name, FALSE) so it can sit on
  master through QA without blocking the release train, then have its default
  flipped to TRUE once it is released, then have the flag deleted.  That last
  step is the one nobody does.  Nothing in the tree records that a flag was
  meant to be temporary, so shipped gates accumulate: showTutorial has been
  defaulting TRUE since v466, greyBarIcons since v492, and both are still
  branch points in hgTracks today.

So the registry separates two populations that look identical in the source:

  gate   introduced to gate a release.  Temporary by intent.  Has a lifecycle
         (added, flipped, sunset) and is expected to be deleted.
  knob   a genuine deployment switch a mirror is entitled to set forever.
         isGbib, browser.dumpStack and hgta.disableAllTables are knobs.  These
         are exempt from sunsetting and saying so explicitly is what keeps the
         report from crying wolf.

The distinction cannot be made mechanically, which is why it is curated here
rather than harvested.  Everything else about a gate's lifecycle is mechanical:
harvestHgConf.py --age dates each variable's first read, and each boolean
flag's first TRUE default, out of the git history, mapped onto the CGI_VERSION
in effect at the time.  So `added` and `flipped` come from the tree and only
`sunset` is a judgement call.  A gate with no explicit sunset gets
flipped + KEEP_AFTER_FLIP, which is the release after mirrors have had a cycle
to object.

Sunset policy, all in versions (releases are about three weeks apart):

  KEEP_AFTER_FLIP = 4   once the default is TRUE the feature is public.  Keep
                        the flag four more releases so a mirror or hgwbeta can
                        switch it back off without a code change, then delete
                        it along with every branch that reads it.
  QA_GRACE        = 6   a gate still defaulting FALSE this long after it landed
                        is not being gated, it is being forgotten.  Either turn
                        it on or delete the feature.

Verification status.  Rows carry verified=True only where the classification
was confirmed by reading the code at the cited file:line.  --check counts what
is left, because a variable described as a permanent knob when it is really a
forgotten gate defeats the purpose of the exercise.

Usage:
    hgConfCatalog.py --json out.json
    hgConfCatalog.py --html out.html
    hgConfCatalog.py --check         # counts and internal consistency
    hgConfCatalog.py --reconcile     # diff the catalog against the tree
    hgConfCatalog.py --reconcile --verbose      # ... with the standing drift
    hgConfCatalog.py --sunset        # what should be deleted, and when
    hgConfCatalog.py --sunset-new    # ... only what changed since the backlog
    hgConfCatalog.py --update-baseline          # accept the current backlog
    hgConfCatalog.py --refresh --sunset-new     # ... rebuilding the age cache
    hgConfCatalog.py --cache /tmp/ages.json --refresh --sunset-new
                                     # ... without writing into the tree

--reconcile is the mode meant for a nightly cron: it prints nothing and exits 0
when the tree holds no setting the catalog has not classified, and exits 1 with
the list when somebody has added one.  --sunset-new is the same shape for the
release side, and is what belongs in the weekly build's wrap-up: it reports the
gates that went overdue or stalled since the backlog file was last accepted, and
the ones that were cleaned up, and says nothing about the standing backlog.

--sunset itself reports every overdue gate on every run whether or not anything
changed.  That is the right thing for a person working the list down and the
wrong thing for anything automated, which is why it is a separate mode.
"""

import argparse
import html
import json
import os
import sys

# Sunset policy, in releases.  See the module docstring.
KEEP_AFTER_FLIP = 4
QA_GRACE = 6

# The gates already known to be overdue or stalled, so --sunset-new can report
# what changed rather than the whole standing list.  Regenerate with
# --update-baseline and commit it; the diff is then the release-to-release
# history of the backlog, in the git log rather than in somebody's memory.
BACKLOG_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            "hgConfGateBacklog.txt")

# Floor on how many settings a working scan finds; well under the real count.
# See the check in reconcile().
MIN_TREE_NAMES = 150

REDMINE = "https://redmine.gi.ucsc.edu/issues/%d"


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def h(name, kind, src, default=None, note=None, public=False, verified=False,
      role=None, sunset=None, env=None, deprecated=False, family=None,
      required=False, ticket=None, debatable=None):
    """One catalog entry.

    name        the hg.conf setting name
    kind        what sort of setting: path, table, profile, credential, url,
                email, limit, flag, branding, debug, internal, dead
    src         file:line where the tree reads it
    default     compiled-in default if the read supplies one
    role        for boolean flags only: "gate" (a release gate, temporary by
                intent, subject to sunsetting) or "knob" (a permanent
                deployment switch, exempt).  Every cfgOptionBooleanDefault
                flag in the tree must be one or the other; --reconcile
                enforces that
    sunset      release by which a gate should be gone from the tree.  Omit to
                take the default of flipped + KEEP_AFTER_FLIP; the age cache
                supplies the flip version
    public      documented for mirror operators in product/ex.hg.conf, or
                belongs there
    verified    True if the classification was confirmed by reading src
    env         environment variable that overrides it, via cfgOptionEnv
    family      groups members of one prefix family, for the docs
    required    read with cfgVal, so the CGI dies if it is absent
    deprecated  the feature it configures is gone or going
    ticket      Redmine ticket that introduced or tracks it
    debatable   why this row's gate-or-knob call could reasonably go the other
                way.  Set it rather than picking a side quietly: the whole
                point of separating the two is that someone has decided, and a
                decision nobody argued with is not the same as a decision.
                --check lists these as the review agenda
    """
    d = {"name": name, "kind": kind, "src": src, "public": public,
         "verified": verified}
    for key, val in (("default", default), ("note", note), ("role", role),
                     ("sunset", sunset), ("env", env), ("family", family),
                     ("ticket", ticket), ("debatable", debatable)):
        if val is not None:
            d[key] = val
    if required:
        d["required"] = True
    if deprecated:
        d["deprecated"] = True
    return d


# ---------------------------------------------------------------------------
# how hg.conf is read at all
# ---------------------------------------------------------------------------

ACCESSORS = {
    "cfgOption": "Returns the value or NULL.  Absent means off.",
    "cfgOptionDefault": "Returns the value or a compiled-in default.",
    "cfgOptionBooleanDefault":
        "Boolean with a compiled-in default.  Accepts yes/no, on/off, "
        "true/false.  This is the accessor used to gate a release.",
    "cfgVal": "Returns the value or errAborts.  The CGI will not run without it.",
    "cfgOptionEnv":
        "Environment variable first, then hg.conf.  Note the argument order: "
        "the environment name comes first and is not an hg.conf setting.",
    "cfgOptionEnvDefault": "As cfgOptionEnv, with a compiled-in default.",
    "cfgOption2":
        "Reads prefix.suffix.  The prefix is usually a runtime value, which is "
        "how one call site serves db.host, central.host and every other "
        "profile at once.",
    "cfgOptionDefault2": "As cfgOption2, with a compiled-in default.",
}

BOUNDARY = (
    "hg.conf is read once per CGI invocation and is never written by the "
    "browser.  It is not user state: nothing in a session, a saved session or "
    "a URL can change it, which is exactly why it is the right place to gate "
    "a feature during a release.  A mirror's copy is outside our control, so "
    "removing a variable has to be treated as an interface change, not a "
    "cleanup.  Precedence for a value is: the environment (only for the "
    "cfgOptionEnv settings, and only where the CGI allows it), then hg.conf, "
    "then the compiled-in default."
)

PROFILE_SUFFIXES = {
    "what":
        "A database profile is a set of hg.conf settings sharing one prefix, "
        "read through cfgOption2(profileName, suffix) where profileName is a "
        "runtime value.  So these suffixes are legal under any profile prefix, "
        "and none of the resulting names appears as a literal anywhere in the "
        "tree.  This is why product/ex.hg.conf documents "
        "archivecentral.password while a search of the source finds nothing.",
    "src": "hg/lib/jksql.c:231",
    "suffixes": ["host", "port", "socket", "user", "password", "db",
                 "verifyServerCert", "ca", "caPath", "cert", "key", "cipher",
                 "crl", "crlPath"],
    "knownProfiles": ["db", "central", "cart", "customTracks", "archivecentral",
                      "backupcentral", "myStuff", "myGenome", "rrcentral", "pq",
                      "rtdb", "cdw"],
}


# ---------------------------------------------------------------------------
# release gates: boolean flags that exist to hold a feature back
# ---------------------------------------------------------------------------
# These are the reason this catalog has a sunset mode.  Each was added so a
# user-visible change could sit on master without shipping.  Ordered by age so
# the backlog is visible at a glance.

RELEASE_GATES = {
    "what": "Boolean flags introduced to ship a feature dark during a release. "
            "Temporary by intent: each should be deleted once the feature it "
            "guards is public and mirrors have had a cycle to object.",
    "vars": [
        h("showMouseovers", "flag", "hg/hgTracks/config.c:671", default="FALSE",
          role="gate", verified=True,
          note="Mouseover text on track items instead of the browser's own "
               "title tooltips.  Added v446 and still defaulting FALSE, which "
               "makes it the oldest gate in the tree that never shipped.  Four "
               "call sites across config.c and imageV2.c.  Either the feature "
               "is wanted, in which case flip it, or it is not, in which case "
               "the flag and the code behind it should go."),
        h("storeUserFiles", "flag", "hg/hgHubConnect/hgHubConnect.c:1730",
          default="FALSE", role="gate", verified=True,
          note="Hub space, the user file store behind hgHubConnect's upload "
               "wizard.  Added v447 and briefly defaulted TRUE around v454 "
               "before going back to FALSE, so the history shows a flip that "
               "was reverted.  Four call sites."),
        h("hgSession.shortLink", "flag", "hg/hgSession/hgSession.c:175",
          default="FALSE", role="gate", verified=True,
          note="Short session links.  Added v374 and never flipped, which is "
               "the longest-running dark feature here."),
        h("showHubApiKey", "flag", "hg/hgHubConnect/hgHubConnect.c:571",
          default="FALSE", role="gate", verified=True,
          note="Expose the hub API key UI.  Shares its call site with "
               "storeUserFiles, so the two should be retired together."),
        h("autoBlatBigPsl", "flag", "hg/hgBlat/hgBlat.c:2629",
          default="FALSE", role="gate", verified=True, ticket="32751",
          note="Always create a custom track from BLAT results, so a result "
               "page can be reopened and shared.  The read at hgBlat.c:2972 "
               "overrides the file-scope autoBigPsl, which is initialised "
               "FALSE at hgBlat.c:60 under the comment \"DEFAULT VALUE change "
               "to TRUE in future\", and eleven branches downstream test it.  "
               "Filed as a knob until that was read, on the strength of the "
               "default being an identifier the harvester could not resolve; "
               "a flag whose own source says to flip it later is a gate."),
        h("blatShowLocus", "flag", "hg/hgBlat/hgBlat.c:784", default="FALSE",
          role="gate", verified=True,
          note="Show the genomic locus alongside BLAT results."),
        h("blatNewPageBanner", "flag", "hg/hgBlat/hgBlat.c:741", default="TRUE",
          role="gate", verified=True,
          note="The banner on the classic BLAT results page that offers a "
               "one-click switch to the new sortable table display.  Guards "
               "the advertisement, not the feature: turning it off stops the "
               "browser recommending the new page without releasing new CGIs, "
               "and users who already opted in or follow a direct link still "
               "get it.  Goes away with the banner, once the new page is the "
               "default.  Kept a gate rather than a knob because what it "
               "turns off is a sentence recommending another page, and no "
               "mirror needs that switch forever; the deadline the report "
               "computes for it is really a deadline on deciding whether the "
               "new page becomes the default, which is the conversation the "
               "nag is supposed to force."),
        h("genarkLiftOver", "flag", "hg/lib/genark.c:413", default="FALSE",
          role="gate", verified=True,
          note="Offer liftOver between GenArk assemblies.  Four call sites in "
               "genark.c and hdb.c."),
        h("showIgv", "flag", "hg/hgTracks/hgTracks.c:12116", default="FALSE",
          role="gate", verified=True,
          note="An IGV link in the track hamburger menus."),
        h("showLiftRequest", "flag", "hg/hgConvert/hgConvert.c:178",
          default="FALSE", role="gate", verified=True, ticket="37973",
          note="A link from the Convert page to liftRequest.html, the page "
               "that requests a new whole-genome alignment.  The assembly "
               "list only offers targets that already have a chain from the "
               "source, so the Convert page is where a user finds out theirs "
               "is missing, but nothing in the tree linked to the request "
               "page.  Off until the request pipeline is confirmed ready to "
               "take traffic from the browser UI."),
        h("groupDropdown", "flag", "hg/hgTracks/hgTracks.c:10152",
          default="FALSE", role="gate", verified=True,
          note="Track group chooser as a dropdown rather than the current "
               "layout."),
        h("gcOnTheFlyCoExist", "flag", "hg/hgTracks/hgTracks.c:7515",
          default="FALSE", role="gate", verified=True,
          note="Let the calculated GC track coexist with the stored one.  A "
               "sub-flag of gcOnTheFly, so it should be deleted with it "
               "rather than outliving it."),
        h("showAliases", "flag", "hg/hgTracks/hgTracks.c:9824", default="FALSE",
          role="gate", verified=True,
          note="Show chromosome alias names in the position box."),
        h("showColorPicker", "flag", "hg/lib/hui.c:6066", default="FALSE",
          role="gate", verified=True,
          note="The track colour picker in track UI."),
        h("doMyVariants", "flag", "hg/hgCustom/hgCustom.c:1226",
          default="FALSE", role="gate", verified=True,
          note="The My Variants track and its upload path.  Thirteen call "
               "sites across seven files, the widest gate in the tree, which "
               "is a fair measure of what deleting a stale one costs."),
        h("hguidIpTracking.enabled", "flag", "hg/lib/botDelay.c:157",
          default="FALSE", role="gate", verified=True,
          note="Per-hguid IP tracking for abuse detection.  Its three "
               "companion settings (maxIps, table, windowSeconds) are plain "
               "values and are listed under abuse control."),
        h("canColorItems", "flag", "hg/hgTracks/hgTracks.c:9124",
          default="FALSE", role="gate", verified=True,
          note="Added in the current release, so it is doing exactly what a "
               "gate is supposed to do and has not earned a deadline yet."),
        # Gates whose default has flipped TRUE.  These are the deletable ones:
        # the feature is public and the flag is now only an off switch.
        h("showTutorial", "flag", "hg/hgCustom/hgCustom.c:180", default="TRUE",
          role="gate", verified=True,
          note="The interactive tutorials.  Public since v466, five call "
               "sites across four CGIs.  Nothing is gating any more."),
        h("canDupTracks", "flag", "hg/lib/dupTrack.c:257", default="TRUE",
          role="gate", verified=True,
          note="Duplicate-track feature.  Public since v443."),
        h("canSnake", "flag", "hg/hgc/hgc.c:3928", default="TRUE", role="gate",
          verified=True,
          note="Snake display for chain and alignment tracks.  Public since "
               "v467."),
        h("showDownloadUi", "flag", "hg/hgTracks/hgTracks.c:8999",
          default="TRUE", role="gate", verified=True,
          note="The download-current-track UI.  Public since v467."),
        h("mergeRecommended", "flag", "hg/hgTracks/recTrackSets.c:194",
          default="TRUE", role="gate", verified=True,
          note="Merge behaviour for recommended track sets.  Public since "
               "v467."),
        h("svgBarChart", "flag", "hg/hgc/barChartClick.c:584", default="TRUE",
          role="gate", verified=True,
          note="SVG rather than raster bar charts on the details page.  "
               "Public since v428, the longest-shipped gate still in place."),
        h("canDoHgcInPopUp", "flag", "hg/hgTracks/config.c:792", default="TRUE",
          role="gate", verified=True,
          note="Details pages in a popup instead of a page load.  Public "
               "since v492.  Three call sites."),
        h("greyBarIcons", "flag", "hg/hgTracks/hgTracks.c:10404",
          default="TRUE", role="gate", verified=True,
          note="The grey side-bar icons on track images.  Public since v492.  "
               "Four call sites in hgTracks.c and imageV2.c."),
        h("bigBedOnePath", "flag", "hg/hgTracks/bigBedTrack.c:1110",
          default="TRUE", role="gate", verified=True,
          note="Single code path for bigBed fetching, replacing the older "
               "split.  Public since v492.  Four call sites, and deleting it "
               "removes a whole alternative path rather than just a branch."),
        h("trackHubsCanAddGroups", "flag", "hg/lib/hubConnect.c:40",
          default="TRUE", role="gate", verified=True,
          note="Let hubs declare their own track groups.  Public since v492."),
        h("newBotDelay", "flag", "hg/lib/botDelay.c:215", default="TRUE",
          role="gate", verified=True,
          note="The reworked bot-delay logic.  Public since v492."),
        h("gcOnTheFly", "flag", "hg/hgTracks/hgTracks.c:7514", default="TRUE",
          role="gate", verified=True,
          note="Calculate the GC percent track at draw time instead of "
               "reading a stored table.  Public since v496, so it is inside "
               "its grace period."),
        h("useBlatBigPsl", "flag", "hg/hgBlat/hgBlat.c:480", default="TRUE",
          role="gate", verified=True,
          note="bigPsl output from BLAT.  Public since v348."),
        h("alwaysItemRgb", "flag", "hg/cgilib/bedCart.c:34", default="TRUE",
          role="gate", verified=True,
          note="Honour a BED's itemRgb without requiring the track setting.  "
               "Defaulted TRUE from v466.  Born TRUE, so it never gated a "
               "release; it is the switch back to how the browser coloured "
               "BED items before v466, and unlike sleepOn429 the off position "
               "describes an old version of the browser rather than a "
               "property of the machine, which is what keeps it a gate.  The "
               "last test in bedItemRgb is the only thing it controls, so "
               "deleting it is a two-line change."),
        h("hgHubConnect.validateHub", "flag",
          "hg/hgHubConnect/hgHubConnect.c:1728", default="TRUE", role="gate",
          verified=True,
          note="The Hub Development tab on hgHubConnect, which is where a hub "
               "author runs hubCheck from the browser.  Two call sites, the "
               "tab itself and hgHubConnectDeveloperMode below it.  Public "
               "since v427.  Described here as \"run hubCheck when a hub is "
               "attached\" until the call site was read: it gates the tab, "
               "not attachment, so a hub is validated on attach either way."),
    ],
}


# ---------------------------------------------------------------------------
# mirror knobs: boolean flags that are meant to live forever
# ---------------------------------------------------------------------------

MIRROR_KNOBS = {
    "what": "Boolean flags that are legitimate, permanent deployment switches. "
            "Listed explicitly so the sunset report does not nag about them.",
    "vars": [
        h("isGbib", "flag", "hg/lib/hdb.c:3712", default="FALSE", role="knob",
          public=True, verified=True,
          note="This is the Genome Browser in a Box.  Changes paths and "
               "disables features that make no sense on a VM."),
        h("isGbic", "flag", "hg/lib/hdb.c:3718", default="FALSE", role="knob",
          public=True, verified=True,
          note="This is a Genome Browser in the Cloud install."),
        h("allowNib", "flag", "hg/lib/hdb.c:2772", default="TRUE", role="knob",
          public=True, verified=True,
          note="Permit nib sequence files.  In hDbDbNibPath: on, the sequence "
               "directory comes from dbDb.nibPath; off, it is /gbdb/<db> "
               "through hReplaceGbdbSeqDir.  Ancient, but an old mirror may "
               "still hold nib assemblies, so it stays.  See forceTwoBit, "
               "which decides the same question one level up."),
        h("forceTwoBit", "flag", "hg/lib/hdb.c:1224", default="TRUE",
          role="knob", public=True, verified=True,
          note="Where hNibForChrom looks for a chromosome's sequence: on, "
               "always /gbdb/<db>/<db>.2bit through hReplaceGbdb; off, fall "
               "back to chromInfo.fileName and then to a nib under "
               "dbDb.nibPath.  Filed as a gate until this was settled "
               "against the code, on the strength of its TRUE default, but "
               "the off position describes where a machine keeps its "
               "sequence, not a browser feature waiting to ship, which is "
               "the same thing allowNib says.  Deleting it "
               "would delete the chromInfo and nib fallbacks with it, and "
               "nobody has proposed that."),
        h("freeType", "flag", "hg/cgilib/trackLayout.c:65", default="TRUE",
          role="knob", public=True, verified=True,
          note="FreeType font rendering in track images rather than the built "
               "in bitmap fonts.  Off selects the Helvetica bitmap path in "
               "trackLayoutInit.  Filed as a gate until this was settled "
               "against the code, because it shipped at v412, but a mirror "
               "without the URW fonts installed has to be able to turn it "
               "off, and the bitmap path is also "
               "measurably faster to draw, so both positions have a "
               "constituency.  Its companions freeTypeDir and freeTypeFont "
               "are plain values and stay."),
        h("trustTrackDb", "flag", "hg/lib/hdb.c:4130", default="FALSE",
          role="knob", verified=True,
          note="Skip the per-track check that a trackDb row's table or file "
               "actually exists, in addTrackIfDataAccessible.  Whether a "
               "machine's trackDb can be trusted is a property of that "
               "machine: on a mirror carrying only tracks it loaded itself "
               "the check is pure cost, and on hgwdev it catches real "
               "breakage.  Filed as a gate until this was settled against "
               "the code, where its FALSE default put it in the stalled list "
               "for 43 releases; it was never a feature "
               "waiting to ship.  The flip to TRUE at v458 and back is a "
               "record of somebody trying it on a machine, not of a release."),
        h("sleepOn429", "flag", "hg/lib/botDelay.c:423", default="TRUE",
          role="knob", verified=True,
          note="After emitting the 429 page, hold the process for ten seconds "
               "before exiting, which slows a robot that ignores the status "
               "code.  Filed as a gate until this was settled against the "
               "code, but it was born TRUE at v481 and so never held a "
               "feature back; the off position is for "
               "a machine that would rather not tie up an Apache child, which "
               "is a deployment call."),
        h("browser.dumpStack", "flag", "hg/lib/hCommon.c:370", default="FALSE",
          role="knob", public=True, verified=True,
          note="Dump a stack trace to the error log on a crash.  A debugging "
               "switch an operator turns on when needed."),
        h("showEarlyErrors", "flag", "hg/lib/hgConfig.c:395", default="FALSE",
          role="knob", public=True, verified=True,
          note="Show errors that happen before the HTML header is written.  "
               "Off in production because it leaks internals; on when "
               "debugging a CGI that dies immediately."),
        h("suppressVeryEarlyErrors", "flag", "hg/lib/hgConfig.c:398",
          default="FALSE", role="knob", verified=True,
          note="The opposite switch, for hiding a broken hg.conf from users."),
        h("hgta.disableAllTables", "flag", "hg/lib/hCommon.c:419",
          default="FALSE", role="knob", public=True, verified=True,
          note="Remove the all-tables option from the Table Browser.  A load "
               "control a mirror is entitled to set."),
        h("hgta.disableSendOutput", "flag", "hg/hgTables/mainPage.c:449",
          default="FALSE", role="knob", public=True, verified=True,
          note="Remove the send-output-to-Galaxy destinations."),
        h("udc.useLocalDiskCache", "flag", "hg/lib/hui.c:656", default="TRUE",
          role="knob", public=True, verified=True,
          note="Use the local UDC cache.  A mirror on a read-only filesystem "
               "turns this off."),
        h("db.neverLocal", "flag", "hg/lib/jksql.c:2141", default="0",
          role="knob", verified=True,
          note="Never treat the database as local, so no local file "
               "shortcuts.  Deployment topology, not a feature."),
        h("traceGbdb", "flag", "hg/lib/hdb.c:1594", default="FALSE",
          role="knob", verified=True,
          note="Log every /gbdb file the CGI opens.  A diagnostic."),
        h("drawDot", "flag", "hg/hgc/hgc.c:3452", default="FALSE", role="knob",
          verified=True,
          note="Emit graphviz dot output from the details page instead of a "
               "rendered image.  A developer diagnostic."),
        h("login.https", "flag", "hg/lib/wikiLink.c:295", default="TRUE",
          role="knob", public=True, verified=True,
          note="Require https for login.  A mirror without a certificate has "
               "to be able to turn this off."),
        h("login.basicAuth", "flag", "hg/lib/wikiLink.c:43", default="FALSE",
          role="knob", public=True, verified=True,
          note="Take identity from HTTP basic auth rather than the login "
               "system."),
        h("login.relativeLink", "flag", "hg/lib/hdb.c:3650", default="FALSE",
          role="knob", public=True, verified=True,
          note="Relative rather than absolute login links."),
        h("login.acceptAnyId", "flag", "hg/lib/wikiLink.c:248",
          default="FALSE", role="knob", verified=True,
          note="Accept any identity token.  Development only, and dangerous "
               "on a public machine."),
        h("login.acceptIdx", "flag", "hg/lib/wikiLink.c:255", default="FALSE",
          role="knob", verified=True, note="Companion to login.acceptAnyId."),
        h("login.pwdEyeIcon", "flag", "hg/hgLogin/hgLogin.c:1429",
          default="TRUE", role="knob", verified=True,
          note="Show-password eye icon on the login form."),
        h("login.emailLink", "flag", "hg/hgLogin/hgLogin.c:1563",
          default="FALSE", role="knob", public=True, verified=True,
          ticket="37929",
          note="Passwordless sign-in: the user is emailed a one-time link "
               "instead of typing a password.  The same switch shows the "
               "change-email page, since that page has no password check "
               "either.  It needs working outbound mail, so a mirror without "
               "it leaves this off permanently, and that is what settles it as "
               "a knob rather than a gate: a machine that cannot send mail can "
               "never turn it on, so there is no release at which the flag "
               "could be deleted.  If the RR's default ever flips TRUE the "
               "flag still has to stay for everyone else."),
        h("analytics.trackClicks", "flag", "hg/lib/googleAnalytics.c:63",
          default="TRUE", role="knob", verified=True,
          note="Report link clicks to analytics.  A mirror with its own "
               "privacy policy turns this off."),
        h("analytics.trackButtons", "flag", "hg/lib/googleAnalytics.c:64",
          default="TRUE", role="knob", verified=True,
          note="Report button presses to analytics."),
        h("wikiTrack.readOnly", "flag", "hg/lib/wikiTrack.c:292",
          default="FALSE", role="knob", verified=True, deprecated=True,
          note="Make the wiki annotation track read-only.  The wiki track "
               "itself is effectively retired."),
        h("cdw.siteIsPublic", "flag",
          "hg/cirm/cdw/cdwGetFile/cdwGetFile.c:62", default="FALSE",
          role="knob", verified=True, deprecated=True,
          note="CIRM data warehouse is public.  Belongs to the cirm CGIs, "
               "which are not part of the browser release."),
        h("multiRegionButtonTop", "flag", "hg/hgTracks/config.c:990",
          default="FALSE", role="knob", public=True, verified=True,
          note="Where the multi-region button lives, which is a layout "
               "preference a mirror is entitled to keep, so a knob.  But the "
               "two reads disagree about the compiled-in default: "
               "hgTracks.c:9126 uses TRUE and config.c:990 uses FALSE, both "
               "through MULTI_REGION_CFG_BUTTON_TOP.  So on a machine that "
               "does not set it the button is in the top bar while the \"Show "
               "all\" checkbox the same flag guards in the multi-region "
               "dialog is hidden, which cannot be what either read intended.  "
               "Needs whoever owns that dialog to say which default is right; "
               "the classification does not depend on the answer."),
        h("ignoreDefaultKnown", "flag", "hg/lib/hdb.c:6144", default="FALSE",
          role="knob", verified=True,
          note="In hdbDefaultKnownDb, ignore the defaultKnown table and treat "
               "the requested db as its own known-genes db.  A property of a "
               "machine whose gene tables are not laid out the way the RR's "
               "are, so a knob; it gates no feature and there is nothing to "
               "flip."),
    ],
}


# ---------------------------------------------------------------------------
# database connections and profiles
# ---------------------------------------------------------------------------

DATABASE = {
    "what": "Where the CGIs find MySQL.  A profile is a prefix; see the "
            "profile suffix family for the settings legal under any of them.",
    "vars": [
        h("db.host", "profile", "hg/qaPushQ/qaPushQ.c:2435", public=True,
          verified=True, env="HGDB_HOST", family="db",
          note="The main assembly database server."),
        h("db.user", "profile", "hg/hgc/hgc.c:1295", public=True,
          verified=True, env="HGDB_USER", family="db"),
        h("db.password", "credential", "hg/hgc/hgc.c:1296", public=True,
          verified=True, env="HGDB_PASSWORD", family="db"),
        h("db.trackDb", "table", "hg/lib/hdb.c:352", public=True,
          verified=True, env="HGDB_TRACKDB", family="db",
          note="Comma-separated list of trackDb tables, searched in order.  "
               "This is how a developer layers a personal trackDb over "
               "production, and why a sandbox hg.conf can be much slower than "
               "the CGI's own."),
        h("db.metaDb", "table", "hg/lib/mdb.c:945", verified=True, family="db"),
        h("db.grp", "table", "hg/lib/hdb.c:5479", default="grp", public=True,
          verified=True, family="db",
          note="Comma-separated list of grp tables holding the track groups.  "
               "The name reaches cfgOption as a parameter of loadGrps(), so "
               "the harvester can only see it as {confName} and the reconcile "
               "would otherwise call it dead documentation."),
        h("db.relatedTrack", "table", "hg/lib/hui.c:10772",
          default='"relatedTrack"', verified=True, family="db"),
        h("central.host", "profile", "hg/qaPushQ/qaPushQ.c:2448", public=True,
          verified=True, family="central",
          note="hgcentral, which holds sessions, users, hub status and the "
               "assembly list."),
        h("central.user", "profile", "hg/qaPushQ/qaPushQ.c:2449", public=True,
          verified=True, family="central"),
        h("central.password", "credential", "hg/qaPushQ/qaPushQ.c:2450",
          public=True, verified=True, family="central"),
        h("central.db", "profile", "hg/hgc/lowelab.c:2362", public=True,
          verified=True, family="central"),
        h("central.domain", "internal", "hg/hubApi/apiUtils.c:854",
          public=True, verified=True, required=True, family="central",
          note="Cookie domain for the central cookie.  Read with cfgVal in "
               "hubApi, so that CGI will not start without it."),
        h("central.cookie", "internal", "hg/lib/hui.c:636", default='"hguid"',
          public=True, verified=True, family="central",
          note="Name of the user-identity cookie.  Changing it logs every "
               "user out."),
        h("cart.host", "profile", "hg/lib/hdb.c:937", public=True,
          verified=True, family="cart",
          note="Optional separate server for cart traffic, which is the "
               "heaviest write load in the browser."),
        h("cart.user", "profile", "hg/lib/hdb.c:938", public=True,
          verified=True, family="cart"),
        h("cart.password", "credential", "hg/lib/hdb.c:938", public=True,
          verified=True, family="cart"),
        h("cart.db", "profile", "hg/lib/hdb.c:937", public=True, verified=True,
          family="cart"),
        h("customTracks.host", "profile", "product/ex.hg.conf", public=True,
          family="customTracks",
          note="Read through the profile mechanism, so it has no literal call "
               "site in the tree."),
        h("customTracks.user", "profile", "product/ex.hg.conf", public=True,
          family="customTracks"),
        h("customTracks.password", "credential", "product/ex.hg.conf",
          public=True, family="customTracks"),
        h("customTracks.tmpdir", "path", "hg/lib/customAdjacency.c:138",
          default='"/data/tmp"', public=True, verified=True,
          family="customTracks"),
        h("customTracks.maxBytes", "limit", "hg/lib/customFactory.c:2448",
          public=True, verified=True, family="customTracks"),
        h("customTracks.useAll", "flag", "hg/lib/customTrack.c:167",
          default="NULL", public=True, verified=True, family="customTracks"),
        h("customTracks.botCheckMult", "limit", "hg/lib/customTrack.c:1043",
          default='"1"', verified=True, family="customTracks",
          note="Multiplier on the bot-delay penalty for custom track loads."),
        h("showTableCache", "table", "hg/lib/jksql.c:850",
          default='"tableList"', public=True, verified=True, required=True,
          note="Table holding a cached list of table names, which avoids a "
               "slow SHOW TABLES.  One of the four settings read with cfgVal."),
    ],
}


# ---------------------------------------------------------------------------
# hgcentral table names
# ---------------------------------------------------------------------------

CENTRAL_TABLES = {
    "what": "Names of the hgcentral tables.  Nearly all are overridable from "
            "the environment as well, which is how a test instance points at "
            "its own copies without editing hg.conf.",
    "vars": [
        h("dbDbTableName", "table", "hg/lib/hdb.c:87", public=True,
          verified=True, env="HGDB_DBDBTABLE", default="dbDb",
          note="The assembly list."),
        h("defaultDbTableName", "table", "hg/lib/hdb.c:107", public=True,
          verified=True, env="HGDB_DEFAULTDBTABLE", default="defaultDb"),
        h("cladeTableName", "table", "hg/lib/hdb.c:117", public=True,
          verified=True, env="HGDB_CLADETABLE", default="clade"),
        h("genomeCladeTableName", "table", "hg/lib/hdb.c:97", public=True,
          verified=True, env="HGDB_GENOMECLADETABLE", default="genomeClade"),
        h("userDbName", "table", "hg/lib/cartDb.c:329", public=True,
          verified=True, env="HGDB_USERDBTABLE", default="userDb",
          note="Per-user cart storage."),
        h("sessionDbName", "table", "hg/lib/cartDb.c:339", public=True,
          verified=True, env="HGDB_SESSIONDBTABLE", default="sessionDb"),
        h("defaultCartName", "table", "hg/lib/cartDb.c:319", public=True,
          verified=True, env="HGDB_DEFAULTCARTTABLE"),
        h("namedSessionDbName", "table", "hg/lib/cart.c:382", verified=True,
          env="HGDB_NAMED_SESSION_DB", default="namedSessionDb",
          note="Saved sessions."),
        h("hub.publicTableName", "table",
          "hg/hgHubConnect/hgHubConnect.c:1388", public=True, verified=True,
          env="HGDB_HUB_PUBLIC_TABLE", family="hub"),
        h("hub.statusTableName", "table",
          "hg/hgHubConnect/hgHubConnect.c:1390", public=True, verified=True,
          env="HGDB_HUB_STATUS_TABLE", family="hub"),
        h("hub.genArkTableName", "table", "hg/lib/genark.c:347",
          verified=True, env="HGDB_GENARK_STATUS_TABLE", family="hub"),
        h("hub.assemblyListTableName", "table", "hg/lib/assemblyList.c:433",
          verified=True, env="HGDB_ASSEMBLYLIST_STATUS_TABLE", family="hub"),
        h("liftOverChainName", "table", "hg/lib/liftOver.c:1984",
          verified=True, env="LIFTOVERCHAINNAME", default="liftOverChain"),
        h("quickLiftChainName", "table", "hg/lib/quickLift.c:582",
          verified=True, env="QUICKLIFTCHAINNAME", default="quickLiftChain",
          ticket="37788"),
        h("blatServersTbl", "table", "hg/hgPcr/hgPcr.c:122",
          default='"blatServers"', verified=True),
        h("hubSearchTextTable", "table", "hg/hgGateway/hgGateway.c:930",
          default='"hubSearchText"', verified=True),
        h("authTableName", "table", "hg/lib/hubSpaceKeys.c:132",
          verified=True, note="Hub space API keys."),
        h("ottoTable", "table", "hg/hubApi/apiUtils.c:884", verified=True,
          note="Table the hub API reads to report otto track update times."),
        h("genbankDb", "profile", "hg/hgVai/hgVai.c:803", public=True,
          verified=True, env="GENBANKDB"),
        h("cart.trace", "debug", "hg/lib/cart.c:106", verified=True,
          note="Log every cart read and write.  Very noisy; a debugging aid "
               "for session problems."),
        h("cartVersion", "internal", "hg/cgilib/cartRewrite.c:46",
          default='"on"', verified=True,
          note="Run the cart rewrite steps that migrate old sessions "
               "forward.  Turning it off strands old sessions."),
        h("browser.sessionKey", "internal", "hg/lib/cartDb.c:76",
          verified=True,
          note="Read through cfgOption2 with a literal prefix, so it is one "
               "of the few two-part names that does appear as a literal."),
    ],
}


# ---------------------------------------------------------------------------
# paths and caches
# ---------------------------------------------------------------------------

PATHS = {
    "what": "Where things live on disk.  These are the settings a mirror is "
            "most likely to have to change.",
    "vars": [
        h("gbdbLoc1", "path", "hg/lib/hdb.c:1551", public=True, verified=True,
          note="Primary /gbdb location.  Any C code opening a /gbdb path is "
               "supposed to run it through hReplaceGbdb() so this takes "
               "effect."),
        h("gbdbLoc2", "path", "hg/lib/hdb.c:1582", public=True, verified=True,
          note="Fallback /gbdb location, tried when a file is missing from "
               "gbdbLoc1.  This is how a mirror keeps part of /gbdb local and "
               "the rest remote."),
        h("udc.cacheDir", "path", "hg/lib/hui.c:660", default="udcDefaultDir()",
          public=True, verified=True,
          note="UDC cache for remote bigData files.  Wants real disk: this is "
               "where every hub file lands."),
        h("udc.localDir", "path", "hg/lib/customFactory.c:204", public=True,
          verified=True),
        h("udcLog", "debug", "hg/hgTracks/hgTracks.c:12061", verified=True,
          note="Log UDC fetches, which is the first thing to turn on when a "
               "hub is slow."),
        h("cacheTrackDbDir", "path", "hg/lib/trackDbCache.c:483",
          default='"/dev/shm/trackDbCache"', verified=True,
          note="Shared-memory cache of parsed trackDb.  Setting it empty "
               "forces a fresh read from MySQL every request, which is the "
               "right way to test trackDb changes; deleting the directory is "
               "not, since it is shared between users."),
        h("sessionDataDir", "path", "hg/hgPcr/hgPcr.c:564", verified=True,
          note="Where session-scoped data (custom tracks belonging to a saved "
               "session) is kept so it survives cleanup."),
        h("sessionDataDirOld", "path", "hg/lib/customFactory.c:180",
          verified=True, note="Previous location, still read so old sessions "
                              "keep working."),
        h("sessionDataDbPrefix", "internal", "hg/lib/sessionData.c:474",
          verified=True),
        h("sessionThumbnail.imgDir", "path", "hg/cgilib/sessionThumbnail.c:29",
          public=True, verified=True, family="sessionThumbnail"),
        h("sessionThumbnail.webPath", "url",
          "hg/cgilib/sessionThumbnail.c:30", public=True, verified=True,
          family="sessionThumbnail"),
        h("sessionThumbnail.convertPath", "path",
          "hg/hgSession/hgSession.c:1153", public=True, verified=True,
          family="sessionThumbnail"),
        h("sessionThumbnail.suppress", "flag",
          "hg/hgSession/hgSession.c:1149", public=True, verified=True,
          family="sessionThumbnail"),
        h("freeTypeDir", "path", "hg/hgTracks/config.c:164",
          default='"../htdocs/urw-fonts"', verified=True),
        h("freeTypeFont", "internal", "hg/cgilib/trackLayout.c:67",
          default='"Bitmap"', verified=True),
        h("fonts.extra", "path", "hg/cgilib/trackLayout.c:49", default="NULL",
          public=True, verified=True),
        h("textSize", "internal", "hg/cgilib/trackLayout.c:80",
          default='"small"', verified=True),
        h("tusdDataDir", "path", "hg/lib/userdata.c:115", verified=True,
          family="hubSpace", note="Hub space upload staging."),
        h("tusdMountPoint", "path", "hg/lib/userdata.c:116", verified=True,
          family="hubSpace"),
        h("hubSpaceUrl", "url", "hg/lib/userdata.c:176", verified=True,
          family="hubSpace"),
        h("hubSpaceTusdEndpoint", "url",
          "hg/hgHubConnect/trackHubWizard.c:260", default="NULL",
          verified=True, family="hubSpace"),
        h("myVariantsDataDir", "path", "hg/lib/myVariants.c:783",
          verified=True, note="Goes with the doMyVariants gate."),
        h("hgPhyloPlaceServerDir", "path", "hg/hgPhyloPlace/runUsher.c:1067",
          verified=True),
        h("browser.documentRoot", "path", "hg/lib/hui.c:692",
          default="DOCUMENT_ROOT", public=True, verified=True),
        h("browser.cgiRoot", "path", "hg/lib/hui.c:759", default="defaultDir",
          public=True, verified=True),
        h("browser.javaScriptDir", "path", "hg/lib/web.c:1441",
          default='"js"', public=True, verified=True),
        h("browser.styleDir", "path", "hg/lib/web.c:1381", default='"style"',
          public=True, verified=True),
        h("browser.styleImagesDir", "path", "hg/lib/web.c:1445",
          default='"style/images"', public=True, verified=True),
        h("browser.trixPath", "path", "hg/cgilib/search.c:19",
          default='"/gbdb/$db/trackDb.ix"', public=True, verified=True,
          note="Track search index.  The $db is substituted at runtime."),
        h("downloads.server", "url", "hg/lib/hui.c:642",
          default='"hgdownload.soe.ucsc.edu"', public=True, verified=True),
        h("cramRef", "path", "hg/hgTables/bam.c:180", public=True,
          verified=True, note="Reference sequence cache for CRAM."),
        h("grepIndex.default", "path", "hg/lib/hgFind.c:168", public=True,
          verified=True, family="grepIndex"),
        h("grepIndex.genbank", "path", "hg/lib/hgFind.c:1201", public=True,
          verified=True, family="grepIndex"),
    ],
}


# ---------------------------------------------------------------------------
# limits and load control
# ---------------------------------------------------------------------------

LIMITS = {
    "what": "Caps on what one request may consume.  These are the settings "
            "that decide whether a heavy request is answered slowly or "
            "refused.",
    "vars": [
        h("maxMem", "limit", "hg/lib/hgConfig.c:376", public=True,
          verified=True,
          note="Address-space cap applied by cfgSetMaxMem() at CGI startup.  "
               "Exceeding it is what produces the hogExit entries in the "
               "error log."),
        h("warnSeconds", "limit", "hg/hgTracks/hgTracks.c:10786",
          verified=True, note="Log a warning for any hgTracks render slower "
                              "than this."),
        h("maxItemsPossible", "limit", "hg/hgTracks/simpleTracks.c:830",
          default='"100000"', public=True, verified=True),
        h("BAMMaxItems", "limit", "hg/hgTracks/bamTrack.c:54",
          default='"10000"', verified=True),
        h("bigBedMaxItems", "limit", "hg/hgTracks/bigBedTrack.c:481",
          default='"10000"', public=True, verified=True),
        h("vcfMaxItems", "limit", "hg/hgTracks/vcfTrack.c:3023",
          default='"10000"', public=True, verified=True),
        h("maxTrackImageHeightPx", "limit", "hg/hgTracks/hgTracks.c:5321",
          default='"32000"', verified=True,
          note="Hard ceiling on image height, which is what stops a dense "
               "track from producing a PNG no browser will render."),
        h("maxDisplayPixelWidth", "limit", "hg/cgilib/trackLayout.c:20",
          default="NULL", public=True, verified=True),
        h("barbMergePixels", "limit", "hg/hgTracks/simpleTracks.c:4481",
          default='"3"', public=True, verified=True),
        h("quickLift.lengthLimit", "limit", "hg/lib/quickLift.c:446",
          default='"10000"', verified=True, ticket="37788"),
        h("liftDailyLimit", "limit", "hg/hubApi/apiUtils.c:908", verified=True,
          note="Per-day liftOver cap for the hub API."),
        h("hgBlat.maxSequenceCount", "limit", "hg/hgBlat/hgBlat.c:1776",
          default="NULL", public=True, verified=True),
        h("parallelFetch.threads", "limit", "hg/hgBlat/hgBlat.c:2442",
          default='"20"', public=True, verified=True),
        h("parallelFetch.timeout", "limit", "hg/hgBlat/hgBlat.c:2473",
          default='"90"', public=True, verified=True),
        h("logCgiVarMaxLen", "limit", "hg/lib/hgConfig.c:386", default='"0"',
          public=True, verified=True,
          note="Truncate logged CGI variables at this length.  Zero disables "
               "the logging that cfgSetLogCgiVars() would otherwise do."),
    ],
}


# ---------------------------------------------------------------------------
# abuse control
# ---------------------------------------------------------------------------

ABUSE = {
    "what": "Bot delay, rate limiting and captchas.  Mostly read from "
            "hg/lib/botDelay.c.",
    "vars": [
        h("bottleneck.host", "url", "hg/lib/botDelay.c:331", public=True,
          verified=True, family="bottleneck",
          note="The bottleneck server that tracks per-IP request rates."),
        h("bottleneck.port", "internal", "hg/lib/botDelay.c:332", public=True,
          verified=True, family="bottleneck"),
        h("bottleneck.except", "internal", "hg/lib/botDelay.c:273",
          verified=True, family="bottleneck",
          note="IPs exempt from delay."),
        h("hguidIpTracking.maxIps", "limit", "hg/lib/botDelay.c:170",
          default='"10"', verified=True, family="hguidIpTracking"),
        h("hguidIpTracking.windowSeconds", "limit", "hg/lib/botDelay.c:171",
          default='"600"', verified=True, family="hguidIpTracking"),
        h("hguidIpTracking.table", "table", "hg/lib/botDelay.c:172",
          default='"hguidIpAccess"', verified=True, family="hguidIpTracking"),
        h("cloudFlareSiteKey", "credential", "hg/lib/cart.c:1526",
          verified=True, required=True,
          note="Turnstile captcha site key.  Read with cfgVal, so a machine "
               "that enables the captcha path must set it."),
        h("cloudFlareSecretKey", "credential", "hg/lib/cart.c:1501",
          verified=True, required=True),
        h("noCaptchaAgent.", "internal", "hg/lib/botDelay.c:307",
          verified=True,
          note="A prefix family rather than one setting: every "
               "noCaptchaAgent.* value is a user-agent string exempt from the "
               "captcha.  Enumerated at runtime with cfgNamesWithPrefix()."),
    ],
}


# ---------------------------------------------------------------------------
# logging and diagnostics
# ---------------------------------------------------------------------------

LOGGING = {
    "what": "Diagnostics.  Several of these are safe to leave on and a few "
            "are expensive, so they are worth telling apart.",
    "vars": [
        h("browser.cgiTime", "debug", "hg/lib/cart.c:3866", default='"yes"',
          public=True, verified=True,
          note="Log per-CGI timing.  On by default and cheap; this is what "
               "the log analysis relies on."),
        h("trackLog", "debug", "hg/hgTracks/hgTracks.c:9228", default='"off"',
          verified=True, note="Log which tracks were drawn per request."),
        h("noSqlInj.level", "internal", "hg/lib/cart.c:2732",
          default='"abort"', verified=True, family="noSqlInj",
          note="What to do when the SQL injection guard fires: abort, warn or "
               "ignore.  Production wants abort."),
        h("noSqlInj.dumpStack", "debug", "hg/lib/cart.c:2735", verified=True,
          family="noSqlInj"),
        h("signalsHandler", "internal", "hg/lib/cart.c:2695", public=True,
          verified=True, note="Install handlers that turn a segfault into a "
                              "logged error rather than a blank page."),
        h("httpsCertCheck", "internal", "hg/lib/cart.c:2699", public=True,
          verified=True, family="httpsCertCheck",
          note="How strictly to verify certificates on outbound https, which "
               "matters because hubs are fetched over it."),
        h("httpsCertCheckVerbose", "debug", "hg/lib/cart.c:2702",
          verified=True, family="httpsCertCheck"),
        h("httpsCertCheckDepth", "internal", "hg/lib/cart.c:2705",
          verified=True, family="httpsCertCheck"),
        h("httpsCertCheckDomainExceptions", "internal", "hg/lib/cart.c:2708",
          public=True, verified=True, family="httpsCertCheck"),
        h("httpProxy", "url", "hg/lib/cart.c:2715", public=True,
          verified=True, family="proxy"),
        h("httpsProxy", "url", "hg/lib/cart.c:2718", public=True,
          verified=True, family="proxy"),
        h("ftpProxy", "url", "hg/lib/cart.c:2721", public=True, verified=True,
          family="proxy"),
        h("noProxy", "internal", "hg/lib/cart.c:2724", public=True,
          verified=True, family="proxy"),
        h("logProxy", "debug", "hg/lib/cart.c:2727", verified=True,
          family="proxy"),
    ],
}


# ---------------------------------------------------------------------------
# branding and site text
# ---------------------------------------------------------------------------

BRANDING = {
    "what": "Text, styling and links that differ between UCSC and a mirror.",
    "vars": [
        h("browser.style", "internal", "hg/lib/cart.c:2993", public=True,
          verified=True, note="Stylesheet override."),
        h("browser.theme.", "internal", "hg/hgTracks/config.c:31",
          public=True, verified=True,
          note="A prefix family: each browser.theme.N.Name value defines a "
               "selectable theme.  Enumerated with cfgNamesWithPrefix(), "
               "which is why the individual names never appear in the source."),
        h("addJs", "internal", "hg/lib/web.c:1587", verified=True,
          note="Extra JavaScript file to include on every page."),
        h("help.html", "path", "hg/lib/hui.c:702", verified=True),
        h("hgTracksNoteHtml", "internal", "hg/hgTracks/hgTracks.c:9895",
          public=True, verified=True,
          note="A banner on the browser page.  This is where a mirror puts "
               "its own notice."),
        h("survey", "url", "hg/hgGateway/hgGateway.c:371", public=True,
          verified=True, env="HGDB_SURVEY",
          note="Survey link on the gateway.  Set to 'off' to hide it."),
        h("surveyLabel", "internal", "hg/hgGateway/hgGateway.c:375",
          default='"Please take our survey"', public=True, verified=True,
          env="HGDB_SURVEY_LABEL"),
        h("surveyLabelImage", "url", "hg/hgGateway/hgGateway.c:377",
          verified=True),
        h("hubSurvey", "url", "hg/hgHubConnect/hgHubConnect.c:1675",
          verified=True, env="HGDB_HUB_SURVEY"),
        h("hubSurveyLabel", "internal",
          "hg/hgHubConnect/hgHubConnect.c:1676", verified=True,
          env="HGDB_HUB_SURVEY_LABEL"),
        h("searchHelpUrl", "url", "hg/hgTracks/hgTracks.c:8867",
          default='"../goldenPath/help/query.html"', verified=True),
        h("searchHelpLabel", "internal", "hg/hgTracks/hgTracks.c:8868",
          default='"Examples"', verified=True),
        h("analyticsKey", "credential", "hg/lib/googleAnalytics.c:13",
          public=True, verified=True),
        h("mouseOverEnabled", "internal", "hg/hgTracks/hgTracks.c:11996",
          default='"on"', verified=True,
          note="Not the same thing as the showMouseovers gate: this one is on "
               "by default and controls the existing tooltip behaviour.  The "
               "near-identical names are a trap worth fixing."),
        h("bigWarn", "internal", "hg/hgTracks/bigWarn.c:56", default='"on"',
          public=True, verified=True),
        h("defaultGenome", "internal", "hg/lib/hdb.c:513",
          default="DEFAULT_GENOME", public=True, verified=True),
        h("browser.popularGenomes", "internal",
          "hg/hgIntegrator/hgIntegrator.c:956",
          default='"hg38,hg19,mm39,mm10..."', verified=True),
        h("geneTracks", "internal", "hg/lib/hgFind.c:3795", verified=True),
        h("browser.recTrackSets", "internal", "hg/hgTracks/recTrackSets.c:58",
          verified=True, family="recTrackSets",
          note="Recommended track sets.  Goes with the mergeRecommended "
               "gate."),
        h("browser.recTrackSetsDetectChange", "internal",
          "hg/hgTracks/recTrackSets.c:66", verified=True,
          family="recTrackSets",
          note="Drives the 'session changed' banner.  The mergeRecommended "
               "gate belongs to the same feature."),
        h("browser.exportedDataHubs", "internal",
          "hg/lib/exportedDataHubs.c:181", verified=True),
        h("browser.cgiExpireMinutes", "limit",
          "hg/hgHubConnect/hgHubConnect.c:404", default='"20"', verified=True),
        h("curatedHubPrefix", "internal", "hg/lib/hubConnect.c:1214",
          verified=True, note="Which curated hubs this machine shows."),
        h("genarkHubPrefix", "internal", "hg/hgGateway/hgGateway.c:1128",
          verified=True),
        h("test.preview", "flag", "hg/lib/hdb.c:3726", verified=True,
          note="Marks a preview machine, which changes some banners and "
               "links.  Part of the release plumbing, but a permanent part."),
        h("restoreMapFind", "internal", "hg/hgTracks/imageV2.c:496",
          verified=True),
    ],
}


# ---------------------------------------------------------------------------
# geographic mirroring
# ---------------------------------------------------------------------------

GEO = {
    "what": "Settings for the geographically distributed mirrors, which route "
            "users to a nearby machine.",
    "vars": [
        h("browser.node", "internal", "hg/lib/geoMirror.c:43", public=True,
          verified=True,
          note="Which node this machine is.  Drives the geo redirect."),
        h("browser.geoSuffix", "internal",
          "hg/geoIpToCountry/geoIpToCountry.c:68", default='""', verified=True,
          note="Suffix appended to central table names so each node can have "
               "its own copies."),
    ],
}


# ---------------------------------------------------------------------------
# login and wiki
# ---------------------------------------------------------------------------

LOGIN = {
    "what": "The login system, and the older wiki-based identity it replaced. "
            "Most of these are read through macros, which is why the harvester "
            "cannot date them.",
    "vars": [
        h("login.systemName", "internal", "hg/lib/wikiLink.c:31", public=True,
          verified=True, family="login"),
        h("login.browserName", "internal", "hg/hgLogin/hgLogin.c:67",
          public=True, verified=True, family="login"),
        h("login.browserAddr", "url", "hg/hgLogin/hgLogin.c:76", public=True,
          verified=True, family="login"),
        h("login.mailSignature", "internal", "hg/hgLogin/hgLogin.c:85",
          public=True, verified=True, family="login"),
        h("login.mailReturnAddr", "email", "hg/hgLogin/hgLogin.c:96",
          public=True, verified=True, family="login"),
        h("login.approvedReturn", "url", "hg/hgLogin/hgLogin.c:326",
          default="NULL", verified=True, family="login"),
        h("login.cookieSalt", "credential",
          "hg/hgPhyloPlace/hgPhyloPlace.c:581", public=True, verified=True,
          family="login", note="Salt for the login cookie.  A secret."),
        h("login.oauth.providers", "internal",
          "hg/hgLogin/oauthLogin.c:127", public=True, verified=True,
          family="login", ticket="37984",
          note="Comma-separated list of social sign-in providers to offer.  "
               "Each name listed here is then configured through its own "
               "login.oauth.<name>.* settings, none of which appear as "
               "literals in the tree; see {key}.  google, orcid and github "
               "are picked up even when unlisted, if they carry a clientId."),
        h("wiki.host", "url", "hg/lib/wikiLink.c:199", public=True,
          verified=True, family="wiki", deprecated=True),
        h("wiki.userNameCookie", "internal", "hg/lib/wikiLink.c:50",
          default='"hgLoginUserName"', public=True, verified=True,
          family="wiki"),
        h("wiki.loggedInCookie", "internal", "hg/lib/wikiLink.c:51",
          default='"hgLoginIdKey"', public=True, verified=True, family="wiki"),
        h("wiki.sessionCookie", "internal", "hg/lib/wikiTrack.c:342",
          public=True, verified=True, family="wiki", deprecated=True),
        h("wikiTrack.URL", "url", "hg/hgGene/wikiTrack.c:38", default="NULL",
          public=True, verified=True, family="wikiTrack", deprecated=True),
        h("wikiTrack.browser", "internal", "hg/hgGene/wikiTrack.c:297",
          default="DEFAULT_BROWSER", public=True, verified=True,
          family="wikiTrack", deprecated=True),
        h("wikiTrack.dbList", "internal", "hg/lib/wikiTrack.c:318",
          public=True, verified=True, family="wikiTrack", deprecated=True),
        h("wikiTrack.editors", "internal", "hg/hgc/variomeClick.c:203",
          default="NULL", public=True, verified=True, family="wikiTrack",
          deprecated=True),
    ],
}


# ---------------------------------------------------------------------------
# external services
# ---------------------------------------------------------------------------

EXTERNAL = {
    "what": "Hosts, URLs and helper programs outside the browser.  Each is a "
            "dependency that can fail independently of us.",
    "vars": [
        h("galaxyUrl", "url", "hg/hgTables/galaxy.c:40", public=True,
          verified=True),
        h("rnaPlotPath", "path", "hg/hgGene/rnaStructure.c:65",
          default='"../cgi-bin/RNAplot"', public=True, verified=True),
        h("hgc.psxyPath", "path", "hg/hgc/hgdpClick.c:277", public=True,
          verified=True, family="hgc"),
        h("hgc.ps2rasterPath", "path", "hg/hgc/hgdpClick.c:297", public=True,
          verified=True, family="hgc"),
        h("hgc.ghostscriptPath", "path", "hg/hgc/hgdpClick.c:298",
          public=True, verified=True, family="hgc"),
        h("nextstrainHost", "url", "hg/hgPhyloPlace/phyloPlace.c:1456",
          verified=True),
        h("microbeTraceHost", "url", "hg/hgPhyloPlace/phyloPlace.c:1532",
          verified=True),
        h("hgPhyloPlaceEnabled", "flag", "hg/hgPhyloPlace/phyloPlace.c:331",
          verified=True,
          note="Read with cfgOption rather than the boolean accessor, so "
               "absent means off.  It gates a whole CGI, which is why it is "
               "not in the gate list: hgPhyloPlace is optional by design, not "
               "pending release."),
        h("resolvProts", "internal", "hg/lib/hui.c:672", public=True,
          verified=True, family="resolv"),
        h("resolvPrefix", "internal", "hg/lib/hui.c:673", public=True,
          verified=True, family="resolv"),
        h("resolvCmd", "path", "hg/lib/hui.c:674", public=True, verified=True,
          family="resolv"),
        h("hubApi.allowHtml", "flag", "hg/hubApi/hubApi.c:1703",
          default='"off"', verified=True, family="hubApi"),
        h("hubApi.showActive0", "flag", "hg/hubApi/apiUtils.c:554",
          default='"off"', verified=True, family="hubApi"),
        h("hubApi.blatDelayFraction", "limit", "hg/hubApi/blat.c:312",
          default="NULL", verified=True, family="hubApi"),
        h("hubApi.relaySecret", "credential", "hg/hubApi/apiUtils.c:1016",
          verified=True, family="hubApi"),
        h("apiFromEmail", "email", "hg/hubApi/liftOver.c:528", verified=True),
        h("chainFileRequestEmail", "email", "hg/hubApi/liftOver.c:527",
          verified=True),
        h("newCustomTrackValidate", "flag", "hg/lib/customFactory.c:773",
          verified=True,
          note="Use the newer custom track validator.  Reads like a gate but "
               "is set per machine on purpose while the two validators "
               "coexist; worth revisiting."),
        h("suggest.mailToAddr", "email",
          "hg/hgUserSuggestion/hgUserSuggestion.c:35", public=True,
          verified=True, family="suggest"),
        h("suggest.mailFromAddr", "email",
          "hg/hgUserSuggestion/hgUserSuggestion.c:36", public=True,
          verified=True, family="suggest"),
        h("suggest.filterKeyword", "internal",
          "hg/hgUserSuggestion/hgUserSuggestion.c:37", public=True,
          verified=True, family="suggest"),
        h("suggest.mailSignature", "internal",
          "hg/hgUserSuggestion/hgUserSuggestion.c:38", public=True,
          verified=True, family="suggest"),
        h("suggest.mailReturnAddr", "email",
          "hg/hgUserSuggestion/hgUserSuggestion.c:39", public=True,
          verified=True, family="suggest"),
        h("suggest.browserName", "internal",
          "hg/hgUserSuggestion/hgUserSuggestion.c:40", public=True,
          verified=True, family="suggest"),
        h("suggest.siteKey", "credential",
          "hg/hgUserSuggestion/hgUserSuggestion.c:191", verified=True,
          family="suggest"),
        h("suggest.secretKey", "credential",
          "hg/hgUserSuggestion/hgUserSuggestion.c:539", verified=True,
          family="suggest"),
        h("suggest.humanThreshold", "limit",
          "hg/hgUserSuggestion/hgUserSuggestion.c:543", default='"-0.1"',
          verified=True, family="suggest"),
        h("hgGateway.dbDbTaxonomy", "internal", "hg/hgGateway/hgGateway.c:407",
          default="defaultDbDbTree", verified=True),
        h("hgEncodeVocabDocBaseUrl", "url",
          "hg/encode/hgEncodeVocab/hgEncodeVocab.c:67", default='""',
          verified=True, deprecated=True),
        h("namedSessionAlt.", "internal", "hg/lib/cart.c:666", verified=True,
          note="A prefix family enumerated at runtime: alternative "
               "namedSessionDb locations to search when resolving a shared "
               "session."),
    ],
}


# ---------------------------------------------------------------------------
# retired: settings whose feature is gone or was never part of the browser
# ---------------------------------------------------------------------------

RETIRED = {
    "what": "Settings for features that are gone, or for CGIs that are not "
            "part of the browser release.  Kept in the catalog because the "
            "reads are still in the tree and a mirror's hg.conf may still "
            "carry them.  Deleting the code is a separate job from "
            "sunsetting a release gate, and these are candidates for it.",
    "vars": [
        h("paypalServer", "url", "hg/gsid/gsidMember/gsidMember.c:237",
          deprecated=True, verified=True, family="gsid",
          note="The GSID member site took payments.  That CGI is not built "
               "for the browser."),
        h("paypalIpnServer", "url", "hg/gsid/gsidMember/gsidMember.c:257",
          deprecated=True, verified=True, family="gsid"),
        h("paypalCommercialFee", "internal",
          "hg/gsid/gsidMember/gsidMember.c:739", deprecated=True,
          verified=True, family="gsid"),
        h("paypalAcademicFee", "internal",
          "hg/gsid/gsidMember/gsidMember.c:741", deprecated=True,
          verified=True, family="gsid"),
        h("paypalEmail", "email", "hg/gsid/gsidMember/gsidMember.c:771",
          deprecated=True, verified=True, family="gsid"),
        h("paypalCert", "credential", "hg/gsid/gsidMember/gsidMember.c:816",
          deprecated=True, verified=True, family="gsid"),
        h("gsidCertId", "credential", "hg/gsid/gsidMember/gsidMember.c:808",
          deprecated=True, verified=True, family="gsid"),
        h("gisaid.structDir", "path", "hg/hgc/virusClick.c:127",
          deprecated=True, verified=True, family="gisaid"),
        h("gisaid.structUrl", "url", "hg/hgc/virusClick.c:140",
          deprecated=True, verified=True, family="gisaid"),
        h("genomeSpace.{variable}", "internal",
          "hg/hgTables/genomeSpace.c:105", deprecated=True, verified=True,
          note="GenomeSpace is shut down.  Read with cfgOption2 and a runtime "
               "suffix, so the harvester reports it half-resolved."),
        h("rtdb.server", "profile", "hg/rtdbWebUpdate/rtdbWebUpdate.c:87",
          deprecated=True, verified=True, family="rtdb"),
        h("rtdb.port", "profile", "hg/rtdbWebUpdate/rtdbWebUpdate.c:88",
          deprecated=True, verified=True, family="rtdb"),
        h("rtdb.databases", "profile", "hg/rtdbWebUpdate/rtdbWebUpdate.c:89",
          deprecated=True, verified=True, family="rtdb"),
        h("pq.host", "profile", "hg/qaPushQ/qaPushQ.c:3832", verified=True,
          family="pq", note="qaPushQ, a QA tool rather than a browser CGI."),
        h("pq.user", "profile", "hg/qaPushQ/qaPushQ.c:3833", verified=True,
          family="pq"),
        h("pq.password", "credential", "hg/qaPushQ/qaPushQ.c:3834",
          verified=True, family="pq"),
        h("pq.db", "profile", "hg/qaPushQ/qaPushQ.c:3831", verified=True,
          family="pq"),
        h("pq.crossHost", "internal", "hg/qaPushQ/qaPushQ.c:3619",
          verified=True, family="pq"),
        h("rrcentral.host", "profile", "hg/qaPushQ/qaPushQ.c:3322",
          verified=True, family="rrcentral"),
        h("rrcentral.user", "profile", "hg/qaPushQ/qaPushQ.c:3323",
          verified=True, family="rrcentral"),
        h("rrcentral.password", "credential", "hg/qaPushQ/qaPushQ.c:3324",
          verified=True, family="rrcentral"),
        h("rrcentral.db", "profile", "hg/qaPushQ/qaPushQ.c:3325",
          verified=True, family="rrcentral"),
        h("encodeDataWarehouse.dataRoot", "path",
          "hg/encode3/encodeDataWarehouse/edwWebXSendFile/edwWebXSendFile.c:29",
          deprecated=True, verified=True, family="encodeDataWarehouse"),
        h("encodeDataWarehouse.key", "credential",
          "hg/encode3/encodeDataWarehouse/edwWebXSendFile/edwWebXSendFile.c:35",
          deprecated=True, verified=True, family="encodeDataWarehouse"),
    ],
}


# ---------------------------------------------------------------------------
# names the scan could not resolve
# ---------------------------------------------------------------------------

RUNTIME_NAMES = {
    "what": "Reads whose setting name is built at run time, so there is no "
            "fixed name to document.  Each is a small family rather than a "
            "single setting, and each is a place where a typo in hg.conf is "
            "silently ignored.",
    "vars": [
        h("{themeKey}", "internal", "hg/lib/cart.c:3006", verified=True,
          note="browser.theme.<name>, resolved from the theme the user "
               "picked."),
        h("{cfgName}", "internal", "hg/hgTracks/hgTracks.c:8894",
          verified=True),
        h("{confName}", "internal", "hg/lib/hdb.c:5447", verified=True,
          note="Resolves to db.grp, passed in by loadGrps()'s only caller at "
               "hdb.c:5479.  Catalogued under its real name as well."),
        h("{confVariable}", "internal", "hg/hgTracks/quickLift.c:26",
          verified=True, ticket="37788",
          note="quickLift colour settings, named per use."),
        h("{overlapKey}", "internal", "hg/hgc/myVariantsClick.c:579",
          verified=True, note="Goes with the doMyVariants gate."),
        h("{key}", "internal", "hg/hgLogin/oauthLogin.c:44", verified=True,
          ticket="37984",
          note="login.oauth.<provider>.<field>, built with safef from the "
               "provider names in login.oauth.providers, so the whole family "
               "is invisible to any scan.  The fields are clientId, "
               "clientSecret, label, type, issuer, authUrl, tokenUrl, "
               "userinfoUrl and scopes.  A second read tries the older "
               "login.<provider>.<field> spelling, which is why a mirror can "
               "have credentials under either prefix."),
        h("{temp}", "internal", "hg/hgcentralTidy/hgcentralTidy.c:80",
          verified=True),
        h("{cdwSetting}", "internal",
          "hg/cirm/cdw/cdwWebBrowse/cdwWebBrowse.c:341", verified=True,
          deprecated=True),
    ],
}


# ---------------------------------------------------------------------------
# assembly
# ---------------------------------------------------------------------------

SECTIONS = [
    ("Release gates", RELEASE_GATES),
    ("Mirror knobs", MIRROR_KNOBS),
    ("Database connections", DATABASE),
    ("hgcentral tables", CENTRAL_TABLES),
    ("Paths and caches", PATHS),
    ("Limits and load control", LIMITS),
    ("Abuse control", ABUSE),
    ("Logging and diagnostics", LOGGING),
    ("Branding and site text", BRANDING),
    ("Geographic mirroring", GEO),
    ("Login and wiki", LOGIN),
    ("External services", EXTERNAL),
    ("Retired", RETIRED),
    ("Runtime-built names", RUNTIME_NAMES),
]


def build():
    """The catalog as one structure."""
    return {"boundary": BOUNDARY,
            "accessors": ACCESSORS,
            "profileSuffixes": PROFILE_SUFFIXES,
            "policy": {"keepAfterFlip": KEEP_AFTER_FLIP,
                       "qaGrace": QA_GRACE},
            "sections": [{"title": t, "what": s["what"], "vars": s["vars"]}
                         for t, s in SECTIONS]}


def all_vars(cat):
    for sec in cat["sections"]:
        for v in sec["vars"]:
            yield v


def by_name(cat):
    out = {}
    for v in all_vars(cat):
        out.setdefault(v["name"], v)
    return out


def gates(cat):
    return [v for v in all_vars(cat) if v.get("role") == "gate"]


def knobs(cat):
    return [v for v in all_vars(cat) if v.get("role") == "knob"]


def counts(cat):
    vs = list(all_vars(cat))
    return {
        "sections": len(cat["sections"]),
        "vars": len(vs),
        "distinctNames": len({v["name"] for v in vs}),
        "gates": len(gates(cat)),
        "knobs": len(knobs(cat)),
        "public": len([v for v in vs if v["public"]]),
        "deprecated": len([v for v in vs if v.get("deprecated")]),
        "verified": len([v for v in vs if v["verified"]]),
        "unverified": len([v for v in vs if not v["verified"]]),
        "required": len([v for v in vs if v.get("required")]),
        "envOverridable": len([v for v in vs if v.get("env")]),
    }


# ---------------------------------------------------------------------------
# the harvester, for --reconcile and --sunset
# ---------------------------------------------------------------------------

def load_harvester():
    """Import harvestHgConf from next door."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    try:
        import harvestHgConf
    except ImportError:
        return None
    return harvestHgConf


# ---------------------------------------------------------------------------
# sunset report
# ---------------------------------------------------------------------------

def gate_lifecycle(cat, ages):
    """Join each gate with the version history.

    Returns a record per gate carrying what the tree knows (added, flipped,
    current default) and what the catalog decided (sunset).  Everything the
    report says follows from this join, so a gate cannot be described as
    healthy just because nobody updated its entry.
    """
    first = ages.get("first", {})
    first_true = ages.get("firstTrue", {})
    cur = ages.get("current")
    hh = load_harvester()
    out = []
    for v in gates(cat):
        name = v["name"]
        added = (first.get(name) or {}).get("version")
        flipped = (first_true.get(name) or {}).get("version")
        tickets, ticket_from = ([], None)
        if hh and hasattr(hh, "ticket_for"):
            tickets, ticket_from = hh.ticket_for(name, ages)
        shipped = v.get("default") == "TRUE"
        # A flip date with a FALSE default now means the flip was reverted, so
        # the flag is back to gating and the flip date must not drive a
        # deadline.
        reverted = bool(flipped) and not shipped
        sunset = v.get("sunset")
        if sunset is None and shipped and flipped:
            sunset = flipped + KEEP_AFTER_FLIP
        out.append({
            "name": name, "src": v["src"], "default": v.get("default"),
            "note": v.get("note"), "added": added, "flipped": flipped,
            "shipped": shipped, "reverted": reverted, "sunset": sunset,
            "current": cur,
            "age": (cur - added) if (cur and added) else None,
            "tickets": tickets, "ticketFrom": ticket_from,
        })
    return out


def gate_states(life, cur):
    """name -> "overdue" or "stalled", for the gates a release can act on.

    The two states the backlog file tracks.  A gate in neither is scheduled,
    inside its QA grace period, or undatable, and none of those is news.  The
    precedence matches sunset_report: an overdue gate is reported as overdue
    even if it would also qualify as stalled.
    """
    states = {}
    for g in life:
        if g["sunset"] and cur and g["sunset"] <= cur:
            states[g["name"]] = "overdue"
        elif (not g["shipped"] and g["age"] is not None
              and g["age"] > QA_GRACE):
            states[g["name"]] = "stalled"
    return states


def read_backlog(path=None):
    """The overdue and stalled gates already accounted for.

    Returns {name: (state, firstVersion)}.  A missing file means nothing has
    been accepted yet, so every flagged gate reads as new; that is the right
    behaviour the first time this runs, and --update-baseline is how it stops.

    The default resolves at call time rather than at import, so a test can point
    BACKLOG_FILE somewhere else.
    """
    path = path or BACKLOG_FILE
    known = {}
    try:
        with open(path) as f:
            for line in f:
                line = line.split("#", 1)[0].strip()
                if not line:
                    continue
                f3 = line.split()
                if len(f3) < 2:
                    continue
                ver = None
                if len(f3) > 2 and f3[2].lstrip("v").isdigit():
                    ver = int(f3[2].lstrip("v"))
                known[f3[0]] = (f3[1], ver)
    except OSError:
        pass
    return known


def write_backlog(states, cur, life=None, path=None):
    """Write the backlog, keeping each gate's original first-reported version.

    A gate that changes state keeps the version it was first flagged at, since
    the useful number is how long it has been somebody's problem, not when it
    last changed shape.
    """
    path = path or BACKLOG_FILE
    was = read_backlog(path)
    notes = {g["name"]: g for g in (life or [])}
    with open(path, "w") as f:
        f.write("""\
# hgConfGateBacklog.txt - the hg.conf release gates already known to be overdue
# for deletion or stalled in QA.  Refs #37925.
#
# hgConfCatalog.py --sunset-new reports only the gates that are not in here, so
# this is what lets the weekly build's wrap-up say "two gates went overdue this
# release" instead of reprinting the whole standing list every three weeks.
# Regenerate with --update-baseline and read the diff before committing: a name
# arriving is a new gate nobody sunset, and a name leaving means somebody
# actually deleted a flag, which is the outcome the whole exercise is for.
#
# The first version of this file was accepted wholesale, as a snapshot of the
# backlog on the day the report learned to tell new from standing.  So a name
# being in here is not evidence that anybody reviewed it; only the ones added
# since, which arrive a few at a time in a reviewable diff, carry that weight.
#
# Fields: name, state (overdue or stalled), and the release it was first
# reported at.  hgConfCatalog.py --sunset prints the full list with dates.
""")
        for name in sorted(states):
            first = was.get(name, (None, None))[1]
            if first is None:
                first = cur
            g = notes.get(name, {})
            note = ""
            if g.get("flipped") and states[name] == "overdue":
                note = "  # flipped v%s" % g["flipped"]
            elif g.get("added"):
                note = "  # added v%s" % g["added"]
            f.write("%-26s %-8s %s%s\n"
                    % (name, states[name],
                       "v%s" % first if first else "?", note))


def sunset_delta(cat, ages, sites=None, out=sys.stdout):
    """Only what changed since the backlog was last accepted.

    The mode meant for the weekly build's wrap-up, and the reason the backlog
    file exists.  --sunset is an 18-line standing list that reads as a worklist
    the first week and as wallpaper by the third; what a release can actually
    act on is the handful of gates that crossed a deadline while it was being
    built.

    Prints a one-line summary on every run, so silence means the step did not
    run rather than that nothing changed.  Exits nonzero when there is
    something new, the same contract as --reconcile, and also when the age
    cache is stale, because a stale cache is exactly the condition under which
    a newly overdue gate would go unreported.
    """
    cur = ages.get("current")
    life = gate_lifecycle(cat, ages)
    now = gate_states(life, cur)
    was = read_backlog()
    byname = {g["name"]: g for g in life}

    fresh = sorted(n for n in now if n not in was)
    moved = sorted(n for n in now if n in was and was[n][0] != now[n])
    gone = sorted(n for n in was if n not in now)

    print("hg.conf release gates at v%s: %d newly overdue or stalled, "
          "%d resolved, %d in the standing backlog (%s)"
          % (cur, len(fresh) + len(moved), len(gone), len(now),
             os.path.basename(BACKLOG_FILE)), file=out)

    def describe(name):
        g = byname.get(name, {})
        bits = []
        if g.get("added"):
            bits.append("added v%d" % g["added"])
        if g.get("flipped") and not g.get("reverted"):
            bits.append("flipped v%d" % g["flipped"])
        if g.get("sunset"):
            bits.append("sunset v%d" % g["sunset"])
        n = len((sites or {}).get(name, [])) or 0
        if n:
            bits.append("%d call site%s" % (n, "" if n == 1 else "s"))
        tik = ", ".join("#%d" % t for t in g.get("tickets") or [])
        if tik:
            bits.append(tik)
        return "  %-26s %s" % (name, ", ".join(bits))

    if fresh:
        print("\nNEWLY FLAGGED (was fine at the last release, is not now):",
              file=out)
        for name in sorted(fresh, key=lambda n: now[n]):
            print(describe(name) + "  [%s]" % now[name], file=out)
    if moved:
        print("\nCHANGED STATE:", file=out)
        for name in moved:
            print(describe(name) + "  [%s -> %s]" % (was[name][0], now[name]),
                  file=out)
    if gone:
        print("\nRESOLVED since the backlog was accepted:", file=out)
        for name in gone:
            n = len((sites or {}).get(name, [])) or 0
            if n:
                # Still read, so it left the list by being reclassified, by
                # shipping, or by being given a later deadline, not by being
                # deleted.  Worth naming either way: the flag is still there.
                print("  %-26s no longer flagged, but still read at %d call "
                      "site%s" % (name, n, "" if n == 1 else "s"), file=out)
            else:
                print("  %-26s reads are gone from the tree; drop its catalog "
                      "row too" % name, file=out)
    if fresh or moved or gone:
        print("\nFull list: hgConfCatalog.py --sunset\n"
              "Accept the new state: hgConfCatalog.py --update-baseline, then "
              "commit %s" % os.path.basename(BACKLOG_FILE), file=out)

    if ages.get("stale"):
        print("\nWARNING: the age cache was built at v%s and the tree is now "
              "at v%s, so a gate\nadded since v%s has no date and cannot be "
              "reported here at all.  Rebuild with\nharvestHgConf.py --age "
              "--refresh and commit hgConfAges.json."
              % (ages.get("cachedAt"), cur, ages.get("cachedAt")), file=out)
        return 1
    return 1 if (fresh or moved) else 0


def sunset_report(cat, ages, sites=None, out=sys.stdout):
    """What should be deleted, what has no deadline, what is stuck in QA."""
    cur = ages.get("current")
    life = gate_lifecycle(cat, ages)
    known = read_backlog()
    print("current tree version: v%s" % cur, file=out)
    if ages.get("stale"):
        print("\nWARNING: the age cache was built at v%s and the tree is now "
              "at v%s.\nDeadlines below are still correct, but any flag added "
              "since v%s has no date\nand will show as 'age unknown' rather "
              "than being reported.  Rebuild with\nharvestHgConf.py --age "
              "--refresh.\n" % (ages.get("cachedAt"), cur, ages.get("cachedAt")),
              file=out)
    undated = [g for g in life if g["added"] is None]
    if undated:
        print("\n%d gate(s) could not be dated from history, so no deadline "
              "applies to them:\n  %s\n" % (len(undated),
              ", ".join(sorted(g["name"] for g in undated))), file=out)
    print("policy: keep a flag %d releases after its default flips TRUE; "
          "a gate\nstill defaulting FALSE after %d releases is stalled.\n"
          % (KEEP_AFTER_FLIP, QA_GRACE), file=out)
    noticket = [g["name"] for g in life if not g["tickets"]]
    if noticket:
        print("The ticket column is the ticket cited by the commit that added "
              "the flag, or\nby the commit that turned it on, marked (flip).  "
              "%d of %d gates have neither\nand are blank: %s\n"
              % (len(noticket), len(life), ", ".join(sorted(noticket))),
              file=out)

    def line(g):
        bits = []
        if g["added"]:
            bits.append("added v%d" % g["added"])
        if g["flipped"] and not g["reverted"]:
            bits.append("flipped v%d" % g["flipped"])
        if g["reverted"]:
            bits.append("flip v%d reverted" % g["flipped"])
        if g["sunset"]:
            bits.append("sunset v%d" % g["sunset"])
        n = len((sites or {}).get(g["name"], [])) or None
        tail = "%d call site%s" % (n, "" if n == 1 else "s") if n else ""
        tik = ", ".join("#%d" % t for t in g["tickets"])
        if tik and g["ticketFrom"] == "flip":
            tik += " (flip)"
        # A gate in the backlog file has been reported before, so say since
        # when.  Anything unmarked in the overdue or stalled lists crossed its
        # line during this release and is what --sunset-new would have printed.
        first = known.get(g["name"], (None, None))[1]
        if first:
            tail = (tail + "  backlog since v%d" % first).strip()
        return "  %-26s %-46s %-16s %s" % (g["name"], ", ".join(bits),
                                           tik, tail)

    overdue = sorted([g for g in life if g["sunset"] and cur
                      and g["sunset"] <= cur], key=lambda g: g["sunset"])
    print("OVERDUE (delete the flag and every branch that reads it): %d"
          % len(overdue), file=out)
    for g in overdue:
        print(line(g), file=out)

    due = sorted([g for g in life if g["sunset"] and cur
                  and g["sunset"] > cur], key=lambda g: g["sunset"])
    print("\nSCHEDULED (shipped, deadline not yet reached): %d" % len(due),
          file=out)
    for g in due:
        print(line(g), file=out)

    stalled = sorted([g for g in life
                      if not g["shipped"] and g["age"] is not None
                      and g["age"] > QA_GRACE], key=lambda g: -g["age"])
    print("\nSTALLED IN QA (default still FALSE %d+ releases after it landed): "
          "%d" % (QA_GRACE, len(stalled)), file=out)
    print("  Either turn these on or delete the feature.  A flag nobody "
          "flips is not\n  gating a release, it is hiding dead code.",
          file=out)
    for g in stalled:
        print(line(g) + "  %d releases old" % g["age"], file=out)

    fresh = [g for g in life if not g["shipped"]
             and (g["age"] is None or g["age"] <= QA_GRACE)]
    print("\nIN QA (recent, inside the grace period): %d" % len(fresh),
          file=out)
    for g in sorted(fresh, key=lambda g: -(g["age"] or 0)):
        print(line(g) + ("  %d releases old" % g["age"] if g["age"] is not None
                         else "  age unknown"), file=out)

    nodate = [g for g in life if g["sunset"] is None and g["shipped"]]
    if nodate:
        print("\nNO DEADLINE (shipped but the flip version is unknown, so no "
              "deadline\ncould be computed; needs a sunset= in the catalog): "
              "%d" % len(nodate), file=out)
        for g in nodate:
            print(line(g), file=out)
    return len(overdue)


# ---------------------------------------------------------------------------
# checks
# ---------------------------------------------------------------------------

def check(cat, out=sys.stderr):
    """Internal consistency.  Returns the number of problems found."""
    problems = 0
    c = counts(cat)
    print("=== counts ===", file=out)
    for k in sorted(c):
        print("%-16s %s" % (k, c[k]), file=out)

    print("\n=== consistency ===", file=out)
    seen = {}
    for sec in cat["sections"]:
        for v in sec["vars"]:
            seen.setdefault(v["name"], []).append(sec["title"])
    dupes = {n: s for n, s in seen.items() if len(s) > 1}
    if dupes:
        print("names in more than one section (%d):" % len(dupes), file=out)
        for n, s in sorted(dupes.items()):
            print("    %-34s %s" % (n, ", ".join(s)), file=out)

    for v in all_vars(cat):
        if v.get("role") == "gate" and v.get("kind") != "flag":
            print("gate not of kind flag: %s" % v["name"], file=out)
            problems += 1
        if v.get("sunset") and not v.get("role") == "gate":
            print("sunset on a non-gate: %s" % v["name"], file=out)
            problems += 1

    unver = [v["name"] for v in all_vars(cat) if not v["verified"]]
    if unver:
        print("\nunverified rows (%d): classification not confirmed against "
              "the code" % len(unver), file=out)
        for n in sorted(unver):
            print("    %s" % n, file=out)

    arguable = [v for v in all_vars(cat) if v.get("debatable")]
    if arguable:
        print("\n=== gate or knob: the calls worth arguing about (%d) ==="
              % len(arguable), file=out)
        print("Everything else in the split is either obvious or was confirmed "
              "at its\ncall site.  These are the ones a second opinion should "
              "settle, because\nfiling a gate as a knob hides it from the "
              "sunset report forever.\n", file=out)
        for v in sorted(arguable, key=lambda x: x["name"].lower()):
            print("  %s  (currently: %s, default %s)"
                  % (v["name"], v.get("role"), v.get("default")), file=out)
            for line in wrap_text(v["debatable"], 72):
                print("      %s" % line, file=out)
        print(file=out)

    print("problems: %d" % problems, file=out)
    return problems


def wrap_text(s, width):
    """Wrap without pulling in textwrap for one caller."""
    words = s.split()
    lines, cur = [], ""
    for w in words:
        if cur and len(cur) + 1 + len(w) > width:
            lines.append(cur)
            cur = w
        else:
            cur = (cur + " " + w).strip()
    if cur:
        lines.append(cur)
    return lines


def reconcile(cat, out=sys.stdout, verbose=False):
    """Diff the catalog against what the tree actually reads.

    Four questions:
      1. does the catalog list something the tree no longer reads
      2. does the tree read something the catalog has not classified
      3. is every boolean flag in the tree classified gate or knob
      4. does each flag's hand-written default still match the tree's
    The last two are the ones that keep the sunset report honest: an
    unclassified flag is one nobody has decided the fate of, and a wrong
    default puts a shipped gate in the stalled list or the reverse.

    Only 2, 3 and 4 count as problems, because only those mean somebody
    changed a setting and did not write it down, which is the one thing a
    person has to act on.  The rest is drift that has been there for years (documentation for
    a feature that was deleted, a read that moved into a helper), and printing
    it on every run is what would turn a nightly cron into mail nobody reads.
    So it goes out only under --verbose.  Silent and 0 means nothing new.
    """
    hh = load_harvester()
    if hh is None:
        print("harvestHgConf.py not importable; cannot reconcile", file=out)
        return 1

    found, _ = hh.harvest()
    tree = hh.by_name(found)
    cataloged = by_name(cat)
    problems = 0

    # A scan that finds almost nothing is a broken scan, not a clean tree, and
    # the difference matters: pointed at the wrong KENT_SRC or an empty clone,
    # everything below would come up empty and report all clear forever.  The
    # real number is in the hundreds.
    if len(tree) < MIN_TREE_NAMES:
        print("only %d settings found under %s: expected at least %d, so the "
              "scan is\nbroken rather than the tree being clean.  Check "
              "KENT_SRC." % (len(tree), hh.ROOT, MIN_TREE_NAMES), file=out)
        return 1

    # Three kinds of name legitimately have no literal read to point at, and
    # all three have to be excused or the report is nothing but false alarms:
    # profile members (read through cfgOption2 with a runtime prefix), prefix
    # families (enumerated with cfgNamesWithPrefix), and the members of such a
    # family as spelled out in the example configs.
    suffixes = set(PROFILE_SUFFIXES["suffixes"])
    prefix_scans = set(found["prefixScans"])

    def is_profile_member(name):
        return "." in name and name.rsplit(".", 1)[1] in suffixes

    def is_prefix_family(name):
        if name in prefix_scans or name.endswith("."):
            return True
        return any(name.startswith(p) for p in prefix_scans)

    if verbose:
        print("=== catalog vs tree ===", file=out)

    missing = sorted(n for n in tree
                     if n not in cataloged and not n.startswith("{"))
    # {ident} names are carried in the catalog with the braces, so match those
    # separately.
    missing += sorted(n for n in tree
                      if n.startswith("{") and n not in cataloged)
    if missing:
        problems += len(missing)
        print("\nread by the tree, not in the catalog (%d):" % len(missing),
              file=out)
        for n in missing:
            print("    %-40s %s" % (n, sorted(tree[n]["sites"])[0]), file=out)

    stale = sorted(n for n in cataloged
                   if n not in tree and not is_profile_member(n)
                   and not is_prefix_family(n))
    if stale and verbose:
        print("\nin the catalog, no literal read found (%d):" % len(stale),
              file=out)
        print("    (a prefix family or a profile member is expected here; "
              "anything else\n     is a catalog row whose read has gone away)",
              file=out)
        for n in stale:
            print("    %-40s %s" % (n, cataloged[n]["src"]), file=out)

    if verbose:
        print("\n=== boolean flags: every one must be a gate or a knob ===",
              file=out)
    tree_flags = {n for n, d in tree.items() if d["boolean"]}
    classified = {v["name"] for v in gates(cat)} | {v["name"] for v in knobs(cat)}
    unclassified = sorted(tree_flags - classified)
    if unclassified:
        problems += len(unclassified)
        print("\nboolean flag classified neither gate nor knob (%d): nobody "
              "has decided\nwhether these are temporary:" % len(unclassified),
              file=out)
        for n in unclassified:
            print("    %-40s %s" % (n, sorted(tree[n]["sites"])[0]), file=out)
    elif verbose:
        print("all %d boolean flags in the tree are classified" %
              len(tree_flags), file=out)

    # Does the catalog's default= still match the tree's?
    #
    # This field is hand-written while everything else about a gate's lifecycle
    # is read out of git, and until this check nothing compared the two.  It
    # matters more than it looks: gate_lifecycle decides a gate has shipped
    # from this field alone, so a flag whose compiled-in default was flipped
    # TRUE without the row being updated sits in the sunset report's stalled
    # list forever, under a heading telling somebody to turn on a flag that is
    # already on.  A stale age cache is announced; this was not.
    #
    # Only reads whose default is the literal TRUE or FALSE can be compared.
    # Where the default is an identifier (autoBlatBigPsl's is the file-scope
    # autoBigPsl) the tree does not say what it is at this level, so the row's
    # default stands unchallenged and the note has to carry the argument.
    bool_defaults = {}
    for rec in hh.all_reads(found):
        if rec.get("boolean") and rec.get("default") in ("TRUE", "FALSE"):
            bool_defaults.setdefault(rec["name"], {}).setdefault(
                rec["default"], []).append(rec["src"])

    mismatched = []
    split = []
    for n in sorted(bool_defaults):
        seen = bool_defaults[n]
        if len(seen) > 1:
            split.append(n)
            continue
        row = cataloged.get(n)
        if row is None or row.get("default") not in ("TRUE", "FALSE"):
            continue
        treeval = next(iter(seen))
        if row["default"] != treeval:
            mismatched.append((n, row["default"], treeval, seen[treeval][0]))
    if mismatched:
        problems += len(mismatched)
        print("\nthe catalog's default disagrees with the tree (%d): the "
              "sunset report reads\nthis field to decide whether a gate has "
              "shipped, so it has to be right:" % len(mismatched), file=out)
        for n, catval, treeval, site in mismatched:
            print("    %-40s catalog %s, tree %s at %s"
                  % (n, catval, treeval, site), file=out)
    if split and verbose:
        # Two reads of one flag with opposite compiled-in defaults.  Not a
        # catalog error, so not a problem here: on a machine that does not set
        # the flag the two halves of the feature disagree, and only whoever
        # owns that code can say which default was meant.
        print("\nread with both TRUE and FALSE as the compiled-in default "
              "(%d):" % len(split), file=out)
        for n in split:
            print("    %s" % n, file=out)
            for val in sorted(bool_defaults[n]):
                for site in sorted(bool_defaults[n][val]):
                    print("        %-8s %s" % (val, site), file=out)

    phantom = sorted(classified - tree_flags)
    if phantom and verbose:
        print("\nclassified as a flag but not read with "
              "cfgOptionBooleanDefault (%d):" % len(phantom), file=out)
        for n in phantom:
            print("    %-40s %s" % (n, cataloged[n]["src"]), file=out)

    if verbose:
        print("\n=== product/ex.hg.conf ===", file=out)
    docs = hh.parse_doc_files()
    pub = {v["name"] for v in all_vars(cat) if v["public"]}
    # A prefix family counts as documented if any member of it is.
    def documented(name):
        if name in docs:
            return True
        return name.endswith(".") and any(d.startswith(name) for d in docs)
    undocumented = sorted(n for n in pub if not documented(n))
    if undocumented and verbose:
        print("marked public in the catalog, absent from the example configs "
              "(%d):" % len(undocumented), file=out)
        print("    (these are settings a mirror operator would want and has no "
              "way to\n     discover)", file=out)
        for n in undocumented:
            print("    %-40s %s" % (n, cataloged[n]["src"]), file=out)
    only_docs = sorted(n for n in docs
                       if n not in cataloged and not is_profile_member(n)
                       and not is_prefix_family(n))
    if only_docs and verbose:
        print("\nin the example configs, nothing in the tree reads them (%d):"
              % len(only_docs), file=out)
        print("    (either the feature was deleted and the documentation was "
              "not, or the\n     documented spelling is wrong, in which case a "
              "mirror that sets it is\n     silently ignored)", file=out)
        for n in only_docs:
            print("    %-40s %s" % (n, docs[n]["sites"][0]), file=out)

    if problems:
        print("\nproblems: %d.  Add or correct the row in hgConfCatalog.py "
              "for each, in the same\ncommit as the code that reads it; see "
              "the 'Registering a new hg.conf variable'\nsection of the "
              "edit-kent-code skill." % problems, file=out)
    elif verbose:
        print("\nproblems: 0", file=out)
    return problems


# ---------------------------------------------------------------------------
# HTML
# ---------------------------------------------------------------------------

CSS = """
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica,
       Arial, sans-serif; margin: 0 auto; max-width: 1180px; padding: 1em 2em;
       color: #222; line-height: 1.45; }
h1 { font-size: 1.6em; margin-bottom: 0.2em; }
h2 { font-size: 1.2em; margin-top: 1.8em; border-bottom: 2px solid #4b6c9e;
     padding-bottom: 0.2em; color: #1a3a6b; }
h3 { font-size: 1.05em; margin-top: 1.4em; color: #1a3a6b; }
p.what { color: #444; margin: 0.4em 0 1em 0; }
table { border-collapse: collapse; width: 100%; font-size: 0.87em;
        margin-bottom: 1.2em; }
th { background: #eaf0f8; text-align: left; padding: 5px 7px;
     border-bottom: 2px solid #4b6c9e; font-weight: 600; }
td { padding: 5px 7px; border-bottom: 1px solid #dde3ec;
     vertical-align: top; }
tr:hover td { background: #f6f9fd; }
code { font-family: Menlo, Consolas, monospace; font-size: 0.94em; }
td.name code { font-weight: 600; }
td.src { color: #666; font-size: 0.9em; white-space: nowrap; }
span.kind { display: inline-block; padding: 1px 6px; border-radius: 3px;
            font-size: 0.82em; background: #e8e8e8; color: #444; }
span.gate { background: #fde9c8; color: #7a4a00; }
span.knob { background: #d9ead9; color: #24541f; }
span.overdue { background: #f8d3d3; color: #8a1f1f; font-weight: 600; }
span.stalled { background: #fdf0c0; color: #6b5300; font-weight: 600; }
span.dep { background: #eee; color: #777; }
span.req { background: #dce6f8; color: #1a3a6b; }
div.note { color: #555; margin-top: 3px; font-size: 0.95em; }
div.arguable { color: #6b4a00; background: #fdf6e3; border-left: 3px solid #d9a441;
               padding: 3px 7px; margin-top: 4px; font-size: 0.93em; }
span.arguable { background: #fdf0c0; color: #6b5300; }
div.box { background: #f6f8fb; border-left: 4px solid #4b6c9e;
          padding: 0.7em 1em; margin: 1em 0; }
div.policy { background: #fff8ec; border-left: 4px solid #d9a441;
             padding: 0.7em 1em; margin: 1em 0; }
ul.toc { columns: 3; list-style: none; padding-left: 0; font-size: 0.92em; }
"""


def esc(s):
    return html.escape(str(s), quote=False)


def ticket_map(cat, ages):
    """name -> (tickets, kind) for every setting git can attribute.

    kind is "introduced" when the commit that added the read cited the ticket
    and "flip" when only the commit that turned a flag on did.  A setting
    missing from this map has no ticket in either commit, which for anything
    added before about v270 means the tree predates Redmine.
    """
    hh = load_harvester()
    if not (ages and hh and hasattr(hh, "ticket_for")):
        return {}
    out = {}
    for v in all_vars(cat):
        tickets, kind = hh.ticket_for(v["name"], ages)
        if tickets:
            out[v["name"]] = (tickets, kind)
    return out


def ticket_links(rec):
    tickets, kind = rec
    links = " ".join('<a href="%s">#%d</a>' % (REDMINE % t, t) for t in tickets)
    if kind == "flip":
        return "turned on by %s" % links
    return "introduced by %s" % links


def var_rows(vs, life_by_name=None, tickets=None):
    rows = []
    for v in sorted(vs, key=lambda x: x["name"].lower()):
        tags = ['<span class="kind">%s</span>' % esc(v["kind"])]
        role = v.get("role")
        if role:
            tags.append('<span class="kind %s">%s</span>' % (role, role))
        if v.get("required"):
            tags.append('<span class="kind req">required</span>')
        if v.get("deprecated"):
            tags.append('<span class="kind dep">retired</span>')
        life = (life_by_name or {}).get(v["name"])
        if life:
            cur = life.get("current")
            if life.get("sunset") and cur and life["sunset"] <= cur:
                tags.append('<span class="kind overdue">overdue v%d</span>'
                            % life["sunset"])
            elif life.get("sunset"):
                tags.append('<span class="kind">sunset v%d</span>'
                            % life["sunset"])
            if (not life.get("shipped") and life.get("age") is not None
                    and life["age"] > QA_GRACE):
                tags.append('<span class="kind stalled">stalled %d</span>'
                            % life["age"])
        extra = ""
        if life and life.get("added"):
            extra = "added v%d" % life["added"]
            if life.get("flipped"):
                extra += ", flipped v%d" % life["flipped"]
        tik = (tickets or {}).get(v["name"])
        if tik:
            extra += (", " if extra else "") + ticket_links(tik)
        note = ""
        if v.get("note"):
            note = '<div class="note">%s</div>' % esc(v["note"])
        if v.get("debatable"):
            tags.append('<span class="kind arguable">gate or knob?</span>')
            note += ('<div class="arguable"><b>Arguable:</b> %s</div>'
                     % esc(v["debatable"]))
        env = ""
        if v.get("env"):
            env = '<div class="note">environment: <code>%s</code></div>' \
                  % esc(v["env"])
        default = esc(v.get("default") or "")
        rows.append(
            "<tr><td class='name'><code>%s</code>%s%s</td>"
            "<td>%s</td><td><code>%s</code></td>"
            "<td class='src'><code>%s</code>%s</td></tr>"
            % (esc(v["name"]), note, env, " ".join(tags), default,
               esc(v["src"]),
               ("<div class='note'>%s</div>" % extra) if extra else ""))
    return "\n".join(rows)


def table_of(vs, life_by_name=None, tickets=None):
    return ("<table><tr><th>setting</th><th>kind</th><th>default</th>"
            "<th>read at</th></tr>\n%s\n</table>"
            % var_rows(vs, life_by_name, tickets))


def render_html(cat, ages=None, sites=None):
    life_by_name = {}
    sunset_html = ""
    tickets = ticket_map(cat, ages)
    if ages:
        life = gate_lifecycle(cat, ages)
        life_by_name = {g["name"]: g for g in life}
        cur = ages.get("current")
        overdue = [g for g in life if g["sunset"] and cur
                   and g["sunset"] <= cur]
        stalled = [g for g in life if not g["shipped"]
                   and g["age"] is not None and g["age"] > QA_GRACE]
        sunset_html = (
            '<div class="policy"><b>Sunset status at v%s.</b> '
            '%d shipped gates are past their removal deadline and %d have been '
            'sitting at a FALSE default for more than %d releases. '
            'Policy: keep a flag %d releases after its default flips TRUE, '
            'then delete it and every branch that reads it. '
            '<code>hgConfCatalog.py --sunset</code> prints the working list.'
            '</div>' % (cur, len(overdue), len(stalled), QA_GRACE,
                        KEEP_AFTER_FLIP))

    c = counts(cat)
    parts = ["<h1>Genome Browser hg.conf variables</h1>",
             "<p>%d settings the CGIs read from <code>hg.conf</code>, "
             "generated from <code>hg/utils/hgConfCatalog/</code>. "
             "%d are release gates and %d are permanent deployment knobs."
             "</p>" % (c["distinctNames"], c["gates"], c["knobs"]),
             '<div class="box">%s</div>' % esc(cat["boundary"]),
             sunset_html]

    if tickets:
        intro = len([1 for _, k in tickets.values() if k == "introduced"])
        parts.append(
            '<div class="policy"><b>Where a setting came from.</b> Each entry '
            'below carries the Redmine ticket cited by the commit that added '
            'the read, or for a flag the commit that turned its default on, '
            'marked there as "turned on by". %d of %d settings are attributed '
            'that way. The rest are blank on purpose: no commit in the chain '
            'names a ticket, and for anything added before about v270 the tree '
            'predates our use of Redmine, so there is nothing to find. Nothing '
            'here is inferred from a later commit that merely edited the line, '
            'which would have credited half the file to whatever refactor '
            'touched it last. <code>harvestHgConf.py --tickets</code> lists '
            'the unattributed settings, separating the ones old enough to be '
            'hopeless from the ones somebody could still fill in.</div>'
            % (intro, c["distinctNames"]))

    parts.append("<h2>Contents</h2><ul class='toc'>")
    for sec in cat["sections"]:
        parts.append("<li><a href='#%s'>%s</a></li>"
                     % (esc(sec["title"].replace(" ", "-")), esc(sec["title"])))
    parts.append("</ul>")

    parts.append("<h2>How a setting is read</h2>")
    parts.append("<table><tr><th>accessor</th><th>behaviour</th></tr>")
    for fn, what in sorted(ACCESSORS.items()):
        parts.append("<tr><td><code>%s</code></td><td>%s</td></tr>"
                     % (esc(fn), esc(what)))
    parts.append("</table>")

    ps = cat["profileSuffixes"]
    parts.append("<h2>Database profile suffixes</h2>")
    parts.append("<p class='what'>%s</p>" % esc(ps["what"]))
    parts.append("<p>Suffixes: %s</p>"
                 % ", ".join("<code>%s</code>" % esc(s) for s in ps["suffixes"]))
    parts.append("<p>Profiles in use: %s</p>"
                 % ", ".join("<code>%s.</code>" % esc(s)
                             for s in ps["knownProfiles"]))

    for sec in cat["sections"]:
        parts.append("<h2 id='%s'>%s</h2>"
                     % (esc(sec["title"].replace(" ", "-")), esc(sec["title"])))
        parts.append("<p class='what'>%s</p>" % esc(sec["what"]))
        parts.append(table_of(sec["vars"], life_by_name, tickets))

    return ("<!DOCTYPE html>\n<html><head><meta charset='utf-8'>"
            "<title>hg.conf variables</title><style>%s</style></head>"
            "<body>\n%s\n</body></html>\n" % (CSS, "\n".join(parts)))


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--json")
    ap.add_argument("--html")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--reconcile", action="store_true")
    ap.add_argument("--sunset", action="store_true")
    ap.add_argument("--sunset-new", dest="sunsetNew", action="store_true",
                    help="report only the gates that went overdue or stalled "
                         "since %s was accepted, and exit 1 if any did"
                         % os.path.basename(BACKLOG_FILE))
    ap.add_argument("--update-baseline", dest="updateBaseline",
                    action="store_true",
                    help="rewrite %s from the current tree; read the diff "
                         "before committing it"
                         % os.path.basename(BACKLOG_FILE))
    ap.add_argument("--verbose", action="store_true",
                    help="with --reconcile, also print the standing drift "
                         "that needs no action")
    ap.add_argument("--cache",
                    help="read and write the age cache here instead of "
                         "hgConfAges.json next to the harvester.  Also "
                         "settable as HGCONF_AGE_CACHE")
    ap.add_argument("--refresh", action="store_true",
                    help="rebuild the age cache before reporting (walks "
                         "history, a few minutes).  Needed after a release "
                         "bumps CGI_VERSION, or a flag added since the cache "
                         "was built has no date at all")
    args = ap.parse_args()

    cat = build()

    ages = None
    sites = None
    if (args.sunset or args.sunsetNew or args.updateBaseline
            or args.html or args.json or args.refresh):
        hh = load_harvester()
        if hh is None:
            print("harvestHgConf.py not importable", file=sys.stderr)
            return 1
        if args.cache:
            hh.CACHE = args.cache
        found, _ = hh.harvest()
        ages = hh.harvest_ages(names=sorted(hh.by_name(found)),
                               refresh=args.refresh)
        sites = {n: d["sites"] for n, d in hh.by_name(found).items()}
        if args.refresh:
            print("rebuilt the age cache at %s: v%s, %d dated names, %d "
                  "recorded flips"
                  % (hh.CACHE, ages.get("current"),
                     len([1 for e in ages.get("first", {}).values()
                          if e.get("version")]),
                     len(ages.get("firstTrue", {}))))

    rc = 0
    if args.check:
        rc |= 1 if check(cat) else 0
    if args.reconcile:
        rc |= 1 if reconcile(cat, verbose=args.verbose) else 0
    if args.sunset:
        sunset_report(cat, ages, sites)
    if args.sunsetNew:
        rc |= 1 if sunset_delta(cat, ages, sites) else 0
    if args.updateBaseline:
        cur = ages.get("current")
        life = gate_lifecycle(cat, ages)
        states = gate_states(life, cur)
        was = read_backlog()
        write_backlog(states, cur, life)
        print("wrote %s: %d gates, %d added, %d dropped"
              % (BACKLOG_FILE, len(states),
                 len(set(states) - set(was)), len(set(was) - set(states))))
    if args.json:
        # Attribution rides along with each setting rather than in a table of
        # its own, so a consumer reading one entry sees where it came from.
        tmap = ticket_map(cat, ages)
        for v in all_vars(cat):
            if v["name"] in tmap:
                v["tickets"], v["ticketFrom"] = tmap[v["name"]]
        with open(args.json, "w") as f:
            json.dump(cat, f, indent=1)
        print("wrote %s" % args.json)
    if args.html:
        with open(args.html, "w") as f:
            f.write(render_html(cat, ages, sites))
        print("wrote %s" % args.html)

    if not any([args.check, args.reconcile, args.sunset, args.sunsetNew,
                args.updateBaseline, args.json, args.html, args.refresh]):
        for k, v in sorted(counts(cat).items()):
            print("%-16s %s" % (k, v))
    return rc


if __name__ == "__main__":
    sys.exit(main())
