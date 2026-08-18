#!/usr/bin/env python3
"""cartFileVarCatalog.py - the registry of cart variables that hold a file name.

Refs #37623.  Most cart variables hold a setting.  A few hold the name of a
file the server made for the user, and a CGI reads one back out of the cart and
opens it.  Those are the ones an attacker can retarget, so hg/lib/cart.c screens
them on the way in, against hand-written arrays:

    fileNameCartVars[]              exact names
    fileNameCartVarPrefixes[]       families whose name carries the db or an id
    urlOrFileNameCartVars[]         may hold a remote URL instead
    fileNamePairCartVarPrefixes[]   value is two file names and a trailing word

A hand-written list goes stale the moment somebody adds a variable and does not
know the list exists.  It was already incomplete on the day it was written.  So
this catalog says, for every cart variable the tree opens as a file, whether
cart.c screens it and why, and --reconcile checks that claim against both the
tree and cart.c itself.

Usage:
    cartFileVarCatalog.py --list        # the catalog, grouped by verdict
    cartFileVarCatalog.py --check       # sanity checks on this file alone
    cartFileVarCatalog.py --reconcile   # diff the catalog against the tree
    cartFileVarCatalog.py --reconcile --verbose
    cartFileVarCatalog.py --json out.json
    cartFileVarCatalog.py --html out.html

--reconcile is the mode meant for a nightly cron: silent and exit 0 when
nothing has changed, non-zero with a list when something has.  Four things make
it fail, and each one is somebody having to do something:

  1. the tree opens a cart variable that is not in this catalog at all.  That
     is a new file-name variable nobody classified.
  2. an entry says cart.c screens it and cart.c no longer does.  That is a line
     deleted from fileNameCartVars[].
  3. an entry says cart.c does not screen it and cart.c now does.  Somebody
     fixed a gap; the entry needs to say so.
  4. cart.c screens a name this catalog has never heard of.  Somebody added to
     the array without saying what the variable is.

Unlike its sibling catalogs there is no baseline file of names deliberately
left undescribed.  The whole set is under two dozen names, so every one of them
gets a row and a verdict, and the strictness is the point.

--check is a different thing and not a substitute: it only reads this file, so
it cannot see the tree move at all.
"""

import argparse
import html
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harvestCartFileVars as harvest        # noqa: E402


# Floor on how many cart-to-file flows a working scan finds; well under the
# real count.  Pointed at an empty tree or the wrong KENT_SRC every check below
# would otherwise come up empty and report all clear forever.
MIN_TREE_FLOWS = 12

# The verdicts.  Every entry has exactly one.
VERDICTS = {
    "screened":
        "cart.c screens it, and reconcile checks that it still does",
    "gap":
        "holds a server file name and cart.c does not screen it",
    "trackDbNamed":
        "the cart variable is named by a trackDb setting, so no fixed list can "
        "cover it; the check has to be at the point of use",
    "otherCheck":
        "the value is validated where it is used, by a check of its own",
    "notBuilt":
        "the CGI is in the tree but not in hg/makefile",
}


def e(name, verdict, where, sink=None, ident=None, screen=None, note=None):
    """One cart variable.

    name    the cart variable, or a <db>-style pattern for a family
    ident   what the harvester reports when the name is built at run time
    where   the file that opens the value
    sink    the call that opens it
    screen  the entry in cart.c that covers it, exact name or "prefix:x"
    """
    return dict(name=name, verdict=verdict, where=where, sink=sink,
                ident=ident, screen=screen, note=note)


CATALOG = [

    # ---- screened by cart.c -------------------------------------------------

    e("ctfile_<db>", "screened", "hg/lib/customTrack.c", "fileExists",
      ident="{ctFileVar}", screen="prefix:ctfile_",
      note="The custom track file.  Also ctfile_hub_<id> for a hub's assembly, "
           "and the name is built from the db at run time, which is why the "
           "cart.c entry is a prefix rather than a name."),

    e("mvCtfile_<db>", "screened", "hg/lib/customTrack.c", "fileExists",
      ident="{mvVar}", screen="prefix:mvCtfile_",
      note="myVariants custom track.  Its file lives under myVariantsDataDir "
           "when hg.conf sets one, which is why isServerUserFilePath() accepts "
           "that directory and the bigDataUrl check does not."),

    e("customComposite-<db>", "screened", "hg/hgCollection/hgCollection.c",
      "open, mustOpen", ident="{buffer}", screen="prefix:customComposite-",
      note="The track collection hub.  hgCollection writes it, so a retargeted "
           "value is a write, not only a read.  The write path also needs a "
           "logged-in session (hgCollection.c:1007); the read path does not."),

    e("hubQuickLift-<db>", "screened", "hg/lib/trackHub.c",
      "open, mustOpen, chmod", ident="{buffer}",
      screen="prefix:hubQuickLift-",
      note="The quickLift hub.  Shares the harvested token with "
           "customComposite-<db> because both names are built into a local "
           "buffer by safef()."),

    e("hgta_userRegionsFile", "screened", "hg/hgTables/userRegions.c",
      "fileExists, lineFileOpen", screen="hgta_userRegionsFile",
      note="Also read by hgIntegrator, which unlinks it.  userRegionsFileName() "
           "returns NULL on a bad value without calling "
           "cartRemoveUserRegions(); removing the regions instead would delete "
           "the user's own region list, which is how the first attempt at this "
           "fix destroyed data."),

    e("hgg_mrnaFoldPs", "screened", "hg/hgGene/rnaStructure.c", "fileExists",
      screen="hgg_mrnaFoldPs",
      note="PostScript for the mRNA fold picture.  hgGene runs ps2pdf on it "
           "and writes the .pdf beside it, so this one is a write too."),

    e("hgp_matchFile", "screened", "hg/visiGene/hgVisiGene/hgVisiGene.c",
      "lineFileOpen", screen="hgp_matchFile",
      note="hgVisiGene search matches.  Filled from trashDirFile() at "
           "hgVisiGene.c:536.  No flow is harvested because the open is in a "
           "different function from the cart read."),

    e("dup_tracks", "screened", "hg/lib/dupTrack.c", "fileExists, mustOpen",
      screen="dup_tracks",
      note="Duplicated track stanzas.  Both reads now go through "
           "dupFileNameFromCart(), which puts the cart read and the open in "
           "different functions, so the scan no longer sees the flow.  That is "
           "the scan's main blind spot; the entry is what keeps the name "
           "covered anyway."),

    e("blatLastBigBed", "screened", "hg/lib/blatShare.c", "bigBedFileOpen",
      screen="blatLastBigBed",
      note="BLAT bigPsl, read by blatFindPinnedBigPsl().  Had its own "
           "hand-rolled copy of the allow-list before this work folded it into "
           "isTrashOrSessionDataPath()."),

    # ---- found by the scan, screened afterwards ------------------------------
    # None of these seven was in cart.c when the fix was written.  The scan
    # found them, which is the argument for having the scan.

    e("hgta_identifierFile", "screened", "hg/hgTables/identifiers.c",
      "fileExists, lineFileOpen", screen="hgta_identifierFile",
      note="The pasted identifier list, normally a trash file.  "
           "identifierFileName() checked only fileExists() before, then "
           "identifiers.c opened it.  Returns NULL on a bad value without "
           "removing the cart variable, same reasoning as user regions."),

    e("blatPslFile", "screened", "hg/hgBlat/hgBlat.c",
      "fileExists, showAliPlaces", screen="blatPslFile",
      note="The saved BLAT result, a trash file set at hgBlat.c:2323.  Its "
           "sibling blatLastBigBed was screened from the start and these two "
           "were not."),

    e("blatFaFile", "screened", "hg/hgBlat/hgBlat.c", "showAliPlaces",
      screen="blatFaFile",
      note="The other half of the saved BLAT result.  No flow is harvested: it "
           "reaches showAliPlaces() without a fileExists() of its own, which is "
           "exactly the shape the scan cannot see.  Caught by reading the code "
           "next to blatPslFile, not by the scan."),

    e("near.customFile", "screened", "hg/near/hgNear/customColumn.c",
      "fileExists", screen="near.customFile",
      note="hgNear custom column file, normally makeTempName(near, .col).  "
           "hgNear is in hg/makefile, so this one is built and reachable."),

    e("gsTemp", "screened", "hg/hgTables/genomeSpace.c",
      "fileSize, md5ForFile, gsS3Upload", screen="gsTemp",
      note="The file hgTables uploads to GenomeSpace.  A retargeted value is "
           "read and sent to a remote service, not only read locally, so the "
           "point-of-use check errAborts rather than returning quietly.  Gated "
           "as well: genomeSpace is enabled only by the presence of its hg.conf "
           "settings (genomeSpace.c:110) and no conf in confs/ sets them."),

    # ---- two file names in one value -----------------------------------------
    # Screened against fileNamePairCartVarPrefixes[], which checks the first two
    # words rather than the whole value.

    e("hgPcrResult_<db>", "screened", "hg/cgilib/pcrResult.c",
      "fileExists, lineFileOpen", ident="{cartVar}", screen="pair:hgPcrResult_",
      note="In-silico PCR results.  The value is two trash file names and an "
           "optional targetDb name, not one file name, which is why it needs an "
           "array of its own.  Found by the daily code review of the commit that "
           "wrote this catalog, not by the scan: pcrResultParseCart() chopLine()s "
           "the value into different locals before opening them, so no flow is "
           "harvested, and a name built at run time reads as {cartVar}, which the "
           "--suspects name test cannot recognize either.  hgPcr appends to both "
           "files when the user checks 'Append to existing PCR result', so an "
           "unscreened value was an arbitrary file write as well as a read.  Both "
           "names are checked at the point of use too, in pcrResultParseCart() "
           "and in hgPcr's pcrResultCartFiles().  hgPcrResult_targetStyle shares "
           "the prefix and is a display setting, so cart.c excludes it by name."),

    # ---- either a URL or a file we made --------------------------------------
    # Screened against urlOrFileNameCartVars[] with isServerUserFileOrUrl(),
    # because isServerUserFilePath() alone would reject every legitimate URL.

    e("multiRegionsBedUrl", "screened", "hg/hgTracks/config.c",
      "fileExists, lineFileMayOpen", screen="urlOrFile:multiRegionsBedUrl",
      note="Multi-region custom BED: either a URL the user gave, or the trash "
           "file hgTracks wrote the pasted BED to (hgTracks.c:4357).  The code "
           "picks the branch by looking for '://', so a value with no protocol "
           "falls through to opening a local file.  config.c prints what it "
           "reads back into the multi-region textarea, so an unscreened value "
           "was a file read the user could see.  Both local-file branches now "
           "check the path as well."),

    e("hgS_loadUrlName", "screened", "hg/lib/cart.c", "netLineFileOpen",
      screen="urlOrFile:hgS_loadUrlName",
      note="hgSession load-settings-from-URL.  netUrlOpen() treats a string "
           "with no '://' as a local path and open()s it (lib/net.c:1518), so "
           "this reached a local file even though the name says URL.  What it "
           "could do with the contents is bounded: they are parsed as session "
           "settings."),

    # ---- covered another way ------------------------------------------------

    e("<trackDb speciesUseFile>", "trackDbNamed", "hg/lib/cart.c",
      "lineFileOpen", ident="{speciesUseFile}",
      note="The speciesUseFile trackDb setting names a cart variable, so the "
           "variable's name is data, not code, and no fixed array can list it.  "
           "cartGetOrderFromFile() checks the value with isServerUserFilePath() "
           "at the point of use instead.  The only user in the tree is "
           "hiv/hivgne8v2/trackDb.ra naming gsidTable.gsidSeqList."),

    e("fileUrl", "otherCheck", "hg/hgTrackUi/hgTrackUi.c", "udcFileMayOpen",
      note="hgTrackUi's file-fetch handler.  Has a purpose-built check of its "
           "own: resolveDotDots() then the value must fall under a connected "
           "hub's base directory or match a whitelisted trackDb setting, or it "
           "errAborts (hgTrackUi.c:4384).  Not a trash file, so the cart.c "
           "allow-list is the wrong check for it."),

    # ---- not built ----------------------------------------------------------

    e("gsidTable.gsidSubjList", "notBuilt", "hg/gsid/gsidTable/gsidTable.c",
      "mustOpen",
      note="gsid is not in hg/makefile.  Its value is a trashDirFile() path, "
           "which is also why cartGetOrderFromFile() can check it."),
    e("gsidTable.gsidSeqList", "notBuilt", "hg/gsid/gsidTable/gsidTable.c",
      "mustOpen", note="gsid is not in hg/makefile."),
    e("gisaidTable.gisaidSubjList", "notBuilt",
      "hg/gisaid/gisaidTable/gisaidTable.c", "mustOpen",
      note="gisaid is not in hg/makefile."),
    e("gisaidTable.gisaidSeqList", "notBuilt",
      "hg/gisaid/gisaidTable/gisaidTable.c", "mustOpen",
      note="gisaid is not in hg/makefile."),
    e("gisaidTable.gisaidAaSeqList", "notBuilt",
      "hg/gisaid/gisaidTable/gisaidTable.c", "mustOpen",
      note="gisaid is not in hg/makefile."),
]


# ---------------------------------------------------------------------------
# lookups
# ---------------------------------------------------------------------------

def by_harvest_key(cat):
    """Every token the harvester can report, mapped to its entries.

    A name built at run time comes out of the scan as {ident}, and one ident
    can stand for more than one family: hgCollection and trackHub both safef()
    their name into a local called buffer.
    """
    out = {}
    for entry in cat:
        for key in (entry["ident"], entry["name"]):
            if key and not ("<" in key and ">" in key):
                out.setdefault(key, []).append(entry)
        if entry["ident"]:
            out.setdefault(entry["ident"], [])
            if entry not in out[entry["ident"]]:
                out[entry["ident"]].append(entry)
    return out


def screen_entries(cat):
    """The cart.c entries the catalog claims, one set per array."""
    names, prefixes, urlOrFile, pairPrefixes = set(), set(), set(), set()
    for entry in cat:
        s = entry["screen"]
        if not s:
            continue
        if s.startswith("prefix:"):
            prefixes.add(s[len("prefix:"):])
        elif s.startswith("urlOrFile:"):
            urlOrFile.add(s[len("urlOrFile:"):])
        elif s.startswith("pair:"):
            pairPrefixes.add(s[len("pair:"):])
        else:
            names.add(s)
    return names, prefixes, urlOrFile, pairPrefixes


# ---------------------------------------------------------------------------
# --check
# ---------------------------------------------------------------------------

def check(cat, out=sys.stderr):
    """Sanity checks on this file alone.  Returns the number of problems."""
    problems = 0
    seen = set()
    for entry in cat:
        name = entry["name"]
        if name in seen:
            print("duplicate entry: %s" % name, file=out)
            problems += 1
        seen.add(name)
        if entry["verdict"] not in VERDICTS:
            print("%s: unknown verdict %r" % (name, entry["verdict"]), file=out)
            problems += 1
        if entry["verdict"] == "screened" and not entry["screen"]:
            print("%s: verdict screened but no screen= given" % name, file=out)
            problems += 1
        if entry["verdict"] != "screened" and entry["screen"]:
            print("%s: screen= given but verdict is %s"
                  % (name, entry["verdict"]), file=out)
            problems += 1
        if not entry["where"] or not entry["note"]:
            print("%s: needs both where= and note=" % name, file=out)
            problems += 1

    counts = {}
    for entry in cat:
        counts[entry["verdict"]] = counts.get(entry["verdict"], 0) + 1
    print("entries    %d" % len(cat), file=out)
    for verdict in sorted(counts):
        print("  %-14s %d" % (verdict, counts[verdict]), file=out)
    print("problems   %d" % problems, file=out)
    return problems


# ---------------------------------------------------------------------------
# --reconcile
# ---------------------------------------------------------------------------

def reconcile(cat, out=sys.stdout, verbose=False):
    """Diff the catalog against the tree and against cart.c.

    Silent and 0 when nothing has changed.  See the module docstring for the
    four things that make it fail.
    """
    h = harvest.harvest()
    flows = h["flows"]

    if len(flows) < MIN_TREE_FLOWS:
        print("only %d cart-to-file flows found: expected at least %d, so the "
              "scan is\nbroken rather than the tree being clean.  Check "
              "KENT_SRC." % (len(flows), MIN_TREE_FLOWS), file=out)
        return 1

    missing = [name for name, key in zip(harvest.SCREEN_ARRAYS,
                                         ("screenNames", "screenPrefixes",
                                          "screenUrlOrFile",
                                          "screenPairPrefixes"))
               if h[key] is None]
    if missing:
        print("no %s array found in %s: either it was removed or it was "
              "reformatted\npast what read_screen() can parse.  Either way "
              "nothing in it is being screened."
              % (", ".join(missing), harvest.CART_C), file=out)
        return 1

    tree_names = set(h["screenNames"])
    tree_prefixes = set(h["screenPrefixes"])
    tree_urlOrFile = set(h["screenUrlOrFile"])
    tree_pairPrefixes = set(h["screenPairPrefixes"])
    keys = by_harvest_key(cat)
    problems = 0

    # 1. the tree opens something this catalog has never heard of
    found = sorted({f["name"] for f in flows})
    site = {}
    for f in flows:
        site.setdefault(f["name"], "%s:%d" % (f["file"], f["line"]))
    unknown = [n for n in found if n not in keys]
    if unknown:
        problems += len(unknown)
        print("\nopened as a file by the tree, not in the catalog (%d):"
              % len(unknown), file=out)
        print("    (add a row to cartFileVarCatalog.py saying what the "
              "variable is,\n     and if it holds a server file name add it to "
              "%s in %s too)"
              % (harvest.SCREEN_ARRAYS[0], harvest.CART_C), file=out)
        for n in unknown:
            print("    %-32s %s" % (n, site.get(n, "")), file=out)

    # 2 and 3. every claim about cart.c, checked against cart.c
    lost, fixed = [], []
    for entry in cat:
        name = entry["screen"]
        if entry["verdict"] == "screened":
            if name.startswith("prefix:"):
                if name[len("prefix:"):] not in tree_prefixes:
                    lost.append(entry)
            elif name.startswith("urlOrFile:"):
                if name[len("urlOrFile:"):] not in tree_urlOrFile:
                    lost.append(entry)
            elif name.startswith("pair:"):
                if name[len("pair:"):] not in tree_pairPrefixes:
                    lost.append(entry)
            elif name not in tree_names:
                lost.append(entry)
        elif entry["verdict"] in ("gap", "otherCheck"):
            probe = entry["name"]
            if "<" not in probe and harvest.screened(probe, tree_names,
                                                     tree_prefixes,
                                                     tree_urlOrFile,
                                                     tree_pairPrefixes):
                fixed.append(entry)

    if lost:
        problems += len(lost)
        print("\nthe catalog says cart.c screens these and it does not (%d):"
              % len(lost), file=out)
        for entry in lost:
            print("    %-32s wanted %s" % (entry["name"], entry["screen"]),
                  file=out)

    if fixed:
        problems += len(fixed)
        print("\ncart.c now screens these and the catalog still calls them a "
              "gap (%d):" % len(fixed), file=out)
        print("    (change the verdict to screened and set screen=)", file=out)
        for entry in fixed:
            print("    %s" % entry["name"], file=out)

    # 4. cart.c screens something the catalog does not describe at all.  A
    # variable the catalog knows as a gap is not an orphan: adding it to cart.c
    # is the fix, and check 3 above already asks for the verdict to be updated.
    cat_names, cat_prefixes, cat_urlOrFile, cat_pairPrefixes = \
        screen_entries(cat)
    known = {entry["name"] for entry in cat}
    orphan = sorted(
        {n for n in tree_names - cat_names if n not in known}
        | {n for n in tree_urlOrFile - cat_urlOrFile if n not in known}
        | {"prefix:" + p for p in tree_prefixes - cat_prefixes
           if not any(k.startswith(p) for k in known)}
        | {"pair:" + p for p in tree_pairPrefixes - cat_pairPrefixes
           if not any(k.startswith(p) for k in known)})
    if orphan:
        problems += len(orphan)
        print("\nscreened by %s, not described in the catalog (%d):"
              % (harvest.CART_C, len(orphan)), file=out)
        for n in orphan:
            print("    %s" % n, file=out)

    if verbose:
        print("\ncatalog entries    %d" % len(cat), file=out)
        print("tree flows         %d over %d names" % (len(flows), len(found)),
              file=out)
        print("cart.c screens     %d names, %d prefixes, %d url-or-file, "
              "%d pair prefixes"
              % (len(tree_names), len(tree_prefixes), len(tree_urlOrFile),
                 len(tree_pairPrefixes)), file=out)
        quiet = [entry["name"] for entry in cat
                 if entry["name"] not in found
                 and (entry["ident"] or entry["name"]) not in found]
        print("\nin the catalog, no flow found in the tree (%d)" % len(quiet),
              file=out)
        print("    (expected: the scan is intraprocedural, so a value handed "
              "to a\n     helper that opens it does not show up)", file=out)
        for n in quiet:
            print("    %s" % n, file=out)
        gaps = [entry["name"] for entry in cat if entry["verdict"] == "gap"]
        print("\nknown gaps, unscreened by cart.c (%d)" % len(gaps), file=out)
        for n in gaps:
            print("    %s" % n, file=out)

    return problems


# ---------------------------------------------------------------------------
# output
# ---------------------------------------------------------------------------

def show(cat, out=sys.stdout):
    for verdict in ("screened", "gap", "trackDbNamed", "otherCheck",
                    "notBuilt"):
        rows = [entry for entry in cat if entry["verdict"] == verdict]
        if not rows:
            continue
        print("\n%s - %s" % (verdict, VERDICTS[verdict]), file=out)
        for entry in rows:
            print("    %-30s %s" % (entry["name"], entry["where"]), file=out)
            if entry["screen"]:
                print("    %-30s cart.c: %s" % ("", entry["screen"]), file=out)


def render_html(cat):
    def esc(s):
        return html.escape(s or "")

    parts = ["<h1>Cart variables that hold a file name</h1>",
             "<p>Refs #37623.  Generated by "
             "<code>hg/utils/cartFileVarCatalog/cartFileVarCatalog.py</code>.  "
             "A cart variable in this table holds the name of a file the "
             "server made for the user, which means a CGI opens whatever the "
             "cart says.  <code>hg/lib/cart.c</code> screens the ones marked "
             "screened on the way in.</p>"]
    for verdict in ("screened", "gap", "trackDbNamed", "otherCheck",
                    "notBuilt"):
        rows = [entry for entry in cat if entry["verdict"] == verdict]
        if not rows:
            continue
        parts.append("<h2>%s</h2><p>%s</p>" % (esc(verdict),
                                               esc(VERDICTS[verdict])))
        parts.append("<table border=1 cellpadding=4 cellspacing=0>")
        parts.append("<tr><th>cart variable<th>opened in<th>call"
                     "<th>cart.c<th>note")
        for entry in rows:
            parts.append("<tr><td><code>%s</code><td><code>%s</code>"
                         "<td><code>%s</code><td><code>%s</code><td>%s"
                         % (esc(entry["name"]), esc(entry["where"]),
                            esc(entry["sink"]), esc(entry["screen"]),
                            esc(entry["note"])))
        parts.append("</table>")
    return "\n".join(parts)


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--list", action="store_true",
                    help="the catalog, grouped by verdict")
    ap.add_argument("--check", action="store_true",
                    help="sanity checks on this file alone")
    ap.add_argument("--reconcile", action="store_true",
                    help="diff the catalog against the tree and cart.c")
    ap.add_argument("--verbose", action="store_true",
                    help="with --reconcile, also print the standing state")
    ap.add_argument("--json", metavar="FILE")
    ap.add_argument("--html", metavar="FILE")
    args = ap.parse_args()

    if args.check:
        return 1 if check(CATALOG) else 0
    if args.reconcile:
        return 1 if reconcile(CATALOG, verbose=args.verbose) else 0
    if args.json:
        with open(args.json, "w") as f:
            json.dump(CATALOG, f, indent=2, sort_keys=True)
        print("wrote %s" % args.json)
        return 0
    if args.html:
        with open(args.html, "w") as f:
            f.write(render_html(CATALOG))
        print("wrote %s" % args.html)
        return 0

    show(CATALOG)
    return 0


if __name__ == "__main__":
    sys.exit(main())
