#!/usr/bin/env python3
"""
Convert the DANIO-CODE track hub trackDb for one assembly into a native UCSC
trackDb .ra file.

The hub lives at http://trackhub2.genereg.net/DANIO-CODE/ .  This first
conversion step does not mirror any data: every relative bigDataUrl is turned
into an absolute URL on the consortium's own server, so the native tracks read
the same files the hub reads.

What the conversion has to change, and why:

  * The 11 hub containers are not all wrapped in one superTrack any more: the
    ones big/generic enough to be found on their own merits (RNA-seq, CAGE-seq,
    3P-seq, ChIP-seq, Hi-C) become standalone top-level tracks in an existing
    group (STANDALONE_GROUP), while the rest -- a mixed bag of regulatory
    element/annotation containers that don't map onto any one existing group --
    stay nested under the "danioCode" superTrack (NESTED_ORDER), whose own label
    is "DANIO-CODE Elements", so their own shortLabels don't repeat the
    DANIO-CODE name.  Nested superTracks are not supported, so the hub
    superTracks that end up nested (or that have their own child superTracks)
    become composites.
  * The hub's ComparativeGenomics superTrack mixes two things that aren't
    DANIO-CODE's own data, both from the Burgess lab: phastCons/CNE
    conservation and CRISPR guide tracks.  Both are pulled out into their own
    standalone top-level tracks (TOP_LABELS["ComparativeGenomics"] and
    CRISPR_CONTAINER) instead of riding along inside one composite together.
  * Track names are made hgTrackDb-legal (letters, digits, '_' and '-' only,
    first character a letter) and are prefixed with "dc" unless they already
    carry a DANIO-CODE accession (DCDnnnnnnSQ / DT), which is unique enough on
    its own.  parent/view references are rewritten to match.
  * subGroup tags are sanitized the same way (hgTrackDb rejects '|', '%', '+'
    in a tag) and the subGroups lines of the subtracks are rewritten to match.
  * Subtracks whose bigDataUrl 404s on the hub server are dropped; the hub has
    a handful of these.

Usage:
  danioCodeHubToRa.py <hubTrackDb.txt> <baseUrl> <out.ra> [--drop-list file]
                      [--local-prefix /gbdb/danRer11/danioCode]

With --local-prefix, bigDataUrl points at our own copy of the file under that
directory instead of at the remote URL.  The file name is the basename of the
remote URL, which is unique across the hub.

Written 2026-09-04, Claude + Max.
"""

import sys, re, os, fnmatch
from collections import OrderedDict

# Containers big/generic enough to earn their own top-level track in an existing
# group, rather than hiding inside the danioCode superTrack where nobody who isn't
# already looking for DANIO-CODE would find them.
STANDALONE_GROUP = {
    "RNA-seqComposite":    "rna",
    "CAGE-seqComposite":   "genes",
    "3P-seqComposite":     "genes",
    "ChIP-seqComposite":   "regulation",
    "HiC_Composite":       "regulation",
    "ComparativeGenomics": "compGeno",
}
STANDALONE_ORDER = ["RNA-seqComposite", "CAGE-seqComposite", "3P-seqComposite",
                     "ChIP-seqComposite", "HiC_Composite", "ComparativeGenomics"]

# Everything else defaults to hidden (see forceHide in walk(), below) since most
# of these composites hold hundreds of subtracks and turning them all on would
# be very slow.  These two keep the hub's own "visibility full": the hub also
# pre-selected exactly one representative subtrack "on" in each (an RNA-seq
# sample and an H3K4me3 ChIP-seq signal), so this shows just that one sample per
# composite by default, not the whole pile.
VISIBLE_BY_DEFAULT = {"RNA-seqComposite", "ChIP-seqComposite"}

# The rest stay nested under the danioCode superTrack: a "mixed bag" of
# regulatory-element/annotation containers that don't map cleanly onto any one
# existing group, in the order we want them to appear there.
NESTED_ORDER = ["comp", "comp_cell_type", "copes_and_dopes", "evalidation",
                "consensus_promoters"]

# hub containers, in the order walk() below emits them.  The danioCode superTrack
# stanza is written first (see main(), below), so its own children (NESTED_ORDER)
# must come right after it -- tdbQuery -check rejects a parent/child pair with
# an unrelated top-level track sitting in between them in the file.
TOP_ORDER = NESTED_ORDER + STANDALONE_ORDER

# The hub's ComparativeGenomics superTrack mixes actual conservation data with
# CRISPR guide tracks that have nothing to do with DANIO-CODE (they're the Burgess
# lab's).  Pull those out into their own top-level superTrack instead, mirroring
# where hg38 keeps its own (unrelated) crispr tracks -- grouped with mapping and
# sequencing, not lumped in with the Burgess lab's own conservation track, and
# without "DC"/DANIO-CODE anywhere in the label since they aren't DANIO-CODE's data.
CRISPR_PARENT_OLD = "ComparativeGenomics"
CRISPR_CHILDREN_OLD = ["crisprs", "gg_crisprs", "ga_crisprs"]
CRISPR_CONTAINER = "dcCrispr"
CRISPR_GROUP = "map"
CRISPR_LABELS = ("CRISPR/Cas9 Targets",
                 "CRISPR/Cas9 target sites in the zebrafish genome, from the "
                 "Shawn Burgess lab at NHGRI (distributed via the DANIO-CODE hub)")

# hub view stanzas carry unhelpfully generic names; give them speaking ones
VIEW_RENAME = {
    "Track_view":                "dcRnaSeqSignalView",
    "CAGE-seqsignalviewtrack":   "dcCageSignalView",
    "CAGE-seqregionsviewtrack":  "dcCageRegionsView",
    "ChIP-seqsignalviewtrack":   "dcChipSignalView",
    "ChIP-seqregionsviewtrack":  "dcChipPeaksView",
    "3P-seqsignalviewtrack":     "dc3PseqSignalView",
    "3P-seqregionsviewtrack":    "dc3PseqRegionsView",
    "HiC_bigWig":                "dcHicSignalView",
}

# short/long labels of the hub containers are all "<X> tracks"; give the
# native container something that reads better in the track list
TOP_LABELS = {
    "RNA-seqComposite":    ("DANIO-CODE RNA-seq",     "DANIO-CODE RNA-seq coverage by developmental stage"),
    "CAGE-seqComposite":   ("DANIO-CODE CAGE-seq",    "DANIO-CODE CAGE-seq signal and tag clusters by developmental stage"),
    "ChIP-seqComposite":   ("DANIO-CODE ChIP-seq",    "DANIO-CODE ChIP-seq signal and peaks by target and developmental stage"),
    "3P-seqComposite":     ("DANIO-CODE 3P-seq",      "DANIO-CODE 3P-seq signal and tag clusters by developmental stage"),
    "HiC_Composite":       ("DANIO-CODE Hi-C",        "DANIO-CODE Hi-C insulation and directionality index by developmental stage"),
    # nested under the danioCode superTrack, which already carries the DANIO-CODE
    # name, so these five don't repeat it themselves
    "comp":                ("Elements",      "DANIO-CODE ChromHMM states, PADREs and DOPEs by developmental stage"),
    "comp_cell_type":      ("Cell Types",    "DANIO-CODE regulatory elements assigned to cell types"),
    "copes_and_dopes":     ("COPEs DOPEs",   "DANIO-CODE constitutive and dynamic phylotypic-period elements"),
    "evalidation":         ("Enhancers",     "DANIO-CODE transgenic enhancer validation"),
    "consensus_promoters": ("Promoters",     "DANIO-CODE consensus promoters"),
    # standalone, not DANIO-CODE's own data -- the Burgess lab's, distributed via
    # the DANIO-CODE hub (see CRISPR_LABELS above for the other Burgess lab track)
    "ComparativeGenomics": ("Burgess Fish PhastCons",
                            "DANIO-CODE Fish PhastCons conservation from the Burgess lab, NHGRI"),
}

ACCESSION_RE = re.compile(r"DCD\d+(SQ|DT)")

TAG_TYPES_TAB = os.path.expanduser("~/kent/src/hg/makeDb/trackDb/tagTypes.tab")


def loadTagTypes(fname):
    """tag -> list of allowed type wildcards, from trackDb/tagTypes.tab.
    tdbQuery -check enforces this, and the hub is looser than we are: it sets
    e.g. itemRgb on bigWig subtracks."""
    tt = {}
    if not os.path.exists(fname):
        return tt
    for line in open(fname):
        line = line.split("#")[0].strip()
        if not line:
            continue
        w = line.split()
        tt[w[0]] = w[1:]
    return tt


def tagAllowed(tagTypes, tag, trackType):
    """True if tdbQuery -check would accept this tag on a stanza of this type.
    Tags the browser does not know at all (the hub carries a couple, such as
    dimensionXchecked, which no CGI reads) are rejected here too."""
    pats = tagTypes.get(tag)
    if pats is None:
        return False
    if not trackType:
        return True           # no type to check against
    base = trackType.split()[0]
    return any(fnmatch.fnmatch(base, p) for p in pats)


def parseStanzas(fname):
    """Return a list of (indent, [(key, value), ...]) for each track stanza,
    plus any comment lines that appear between stanzas (attached to the next
    stanza).  Blank lines separate stanzas."""
    stanzas = []
    cur = None
    curIndent = 0
    for line in open(fname):
        line = line.rstrip("\n")
        stripped = line.strip()
        if not stripped:
            if cur:
                stanzas.append((curIndent, cur))
                cur = None
            continue
        if stripped.startswith("#"):
            continue
        key, _, val = stripped.partition(" ")
        val = val.strip()
        if key == "track":
            if cur:
                stanzas.append((curIndent, cur))
            cur = [("track", val)]
            curIndent = len(line) - len(line.lstrip())
        else:
            if cur is None:      # setting before any track: hub-level, skip
                continue
            cur.append((key, val))
    if cur:
        stanzas.append((curIndent, cur))
    return stanzas


def sanitizeTag(tag):
    """Make a tag legal for hgTrackDb: letters, digits and '_' only, and it has
    to start with a letter."""
    out = re.sub(r"[^A-Za-z0-9_]", "_", tag)
    out = re.sub(r"_+", "_", out).strip("_")
    if not out:
        out = "x"
    if not out[0].isalpha():
        out = "s" + out
    return out


def newTrackName(old):
    """Legal, collision-safe native track name for a hub track name."""
    if old in VIEW_RENAME:
        return VIEW_RENAME[old]
    clean = re.sub(r"[^A-Za-z0-9_]", "", old)
    if ACCESSION_RE.search(clean):
        # already carries a DANIO-CODE accession, unique on its own
        name = clean
    else:
        name = "dc" + clean[0].upper() + clean[1:] if clean else "dc"
    if not name[0].isalpha():
        name = "dc" + name
    return name


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    hubTrackDb, baseUrl, outFname = sys.argv[1:4]
    dropFile = None
    if "--drop-list" in sys.argv:
        dropFile = sys.argv[sys.argv.index("--drop-list") + 1]
    localPrefix = None
    if "--local-prefix" in sys.argv:
        localPrefix = sys.argv[sys.argv.index("--local-prefix") + 1].rstrip("/")
    dropUrls = set()
    if dropFile and os.path.exists(dropFile):
        dropUrls = set(l.strip() for l in open(dropFile) if l.strip())

    if not baseUrl.endswith("/"):
        baseUrl += "/"

    stanzas = parseStanzas(hubTrackDb)
    tagTypes = loadTagTypes(TAG_TYPES_TAB)

    # ---- pass 1: name mapping, and record which subGroup tags get renamed ----
    nameMap = OrderedDict()
    for indent, sets in stanzas:
        old = dict(sets)["track"]
        new = newTrackName(old)
        if new in nameMap.values():
            sys.exit("name collision on %s -> %s" % (old, new))
        nameMap[old] = new

    # subGroup tag renames, keyed by (compositeOldName, subGroupDimName)
    tagMap = {}
    for indent, sets in stanzas:
        d = dict(sets)
        for key, val in sets:
            if not re.match(r"subGroup\d+$", key):
                continue
            parts = val.split()
            if len(parts) < 2:
                continue
            dimName = parts[0]
            for pair in parts[2:]:
                tag = pair.split("=", 1)[0]
                newTag = sanitizeTag(tag)
                if newTag != tag:
                    tagMap[(d["track"], dimName, tag)] = newTag

    def mapTag(compOld, dimName, tag):
        return tagMap.get((compOld, dimName, tag), tag)

    # child -> nearest composite ancestor (for subGroups tag rewriting)
    parentOf = {}
    stack = []   # (indent, oldName)
    for indent, sets in stanzas:
        old = dict(sets)["track"]
        while stack and stack[-1][0] >= indent:
            stack.pop()
        parentOf[old] = stack[-1][1] if stack else None
        stack.append((indent, old))

    def compositeAncestor(old):
        """Walk up to the stanza that declared the subGroups we need."""
        p = parentOf.get(old)
        while p is not None:
            if p in compositeNames:
                return p
            p = parentOf.get(p)
        return None

    compositeNames = set()
    for indent, sets in stanzas:
        d = dict(sets)
        if "compositeTrack" in d or any(re.match(r"subGroup\d+$", k) for k, v in sets):
            compositeNames.add(d["track"])

    childrenOf = {}
    for indent, sets in stanzas:
        trk = dict(sets)["track"]
        childrenOf.setdefault(parentOf.get(trk), []).append(trk)

    # The CRISPR children are emitted separately, under their own top-level
    # superTrack (see CRISPR_* above) -- keep the normal walk from also visiting
    # them under ComparativeGenomics, and keep them out of that composite's
    # borrowed-type calculation in emitStanza.
    childrenOf[CRISPR_PARENT_OLD] = [c for c in childrenOf.get(CRISPR_PARENT_OLD, [])
                                      if c not in CRISPR_CHILDREN_OLD]

    typeOf = {}
    for indent, sets in stanzas:
        d = dict(sets)
        t = d.get("type")
        if not t:
            p = parentOf.get(d["track"])
            while p is not None and not t:
                t = typeOf.get(p)
                p = parentOf.get(p)
        typeOf[d["track"]] = t

    # ---- which subtracks have to go, and which subGroup tags survive ----
    dropSet = set()
    for indent, sets in stanzas:
        d = dict(sets)
        url = d.get("bigDataUrl")
        if url is None:
            continue
        full = url if re.match(r"https?://", url) else baseUrl + url
        if full in dropUrls:
            dropSet.add(d["track"])

    usedTags = {}
    for indent, sets in stanzas:
        d = dict(sets)
        if d["track"] in dropSet or "subGroups" not in d:
            continue
        comp = compositeAncestor(d["track"])
        for pair in d["subGroups"].split():
            dimName, _, tag = pair.partition("=")
            usedTags.setdefault((comp, dimName), set()).add(mapTag(comp, dimName, tag))

    # ---- pass 2: emit ----
    dropped = []
    prunedTags = []
    droppedTags = []
    out = []
    out.append("# DANIO-CODE, converted from the consortium's track hub")
    out.append("# %s" % baseUrl)
    out.append("# generated by hg/makeDb/scripts/danioCode/danioCodeHubToRa.py -- do not hand-edit")
    out.append("")
    out.append("track danioCode")
    out.append("superTrack on")
    out.append("shortLabel DANIO-CODE Elements")
    out.append("longLabel DANIO-CODE: zebrafish developmental multi-omics data and regulatory elements")
    out.append("group regulation")
    out.append("priority 4")
    out.append("")

    byOldName = {dict(s)["track"]: (i, s) for i, s in stanzas}
    emitted = set()

    def emitStanza(indent, sets, extra=None, forceHide=False, parentOverride=None):
        d = dict(sets)
        old = d["track"]
        new = nameMap[old]
        pad = " " * indent
        lines = []
        lines.append("%strack %s" % (pad, new))
        seen = set()
        for key, val in sets:
            if key == "track":
                continue
            if key in ("shortLabel", "longLabel") and old in TOP_LABELS:
                continue
            if key == "superTrack":
                # a superTrack cannot sit inside another superTrack, so the hub's
                # top-level superTracks become composites.  A composite needs its
                # own 'type': hgTracks picks the container's draw handler from it,
                # and a hub superTrack does not carry one.  Borrow the first
                # child's type.
                lines.append("%scompositeTrack on" % pad)
                if "type" not in d:
                    kids = childrenOf.get(old, [])
                    kidType = next((typeOf.get(k) for k in kids if typeOf.get(k)), None)
                    if not kidType:
                        sys.exit("cannot pick a type for composite %s" % old)
                    lines.append("%stype %s" % (pad, kidType))
                continue
            if key == "parent":
                pieces = val.split()
                pieces[0] = nameMap[parentOverride] if parentOverride else nameMap[pieces[0]]
                lines.append("%sparent %s" % (pad, " ".join(pieces)))
                continue
            if key == "bigDataUrl":
                url = val if re.match(r"https?://", val) else baseUrl + val
                if localPrefix:
                    url = "%s/%s" % (localPrefix, os.path.basename(url))
                lines.append("%sbigDataUrl %s" % (pad, url))
                continue
            if key == "visibility" and forceHide:
                lines.append("%svisibility hide" % pad)
                continue
            if key == "priority" and forceHide:
                continue         # replaced by our own ordering, see extra below
            if re.match(r"subGroup\d+$", key):
                parts = val.split()
                dimName = parts[0]
                newPairs = []
                for pair in parts[2:]:
                    tag, _, label = pair.partition("=")
                    newTag = mapTag(old, dimName, tag)
                    if newTag not in usedTags.get((old, dimName), set()):
                        prunedTags.append((old, dimName, tag))
                        continue
                    newPairs.append("%s=%s" % (newTag, label or tag))
                lines.append("%s%s %s %s %s" % (pad, key, dimName, parts[1], " ".join(newPairs)))
                continue
            if key == "subGroups":
                comp = compositeAncestor(old)
                newPairs = []
                for pair in val.split():
                    dimName, _, tag = pair.partition("=")
                    newPairs.append("%s=%s" % (dimName, mapTag(comp, dimName, tag)))
                lines.append("%ssubGroups %s" % (pad, " ".join(newPairs)))
                continue
            if not tagAllowed(tagTypes, key, typeOf.get(old)):
                droppedTags.append((old, key, typeOf.get(old)))
                continue
            lines.append("%s%s %s" % (pad, key, val.strip()))
        if old in TOP_LABELS:
            short, long_ = TOP_LABELS[old]
            lines.insert(1, "%sshortLabel %s" % (pad, short))
            lines.insert(2, "%slongLabel %s" % (pad, long_))
        if extra:
            lines.extend("%s%s" % (pad, e) for e in extra)
        out.extend(lines)
        out.append("")

    # walk the hub tree in TOP_ORDER, depth first, keeping hub child order
    def walk(old, depth):
        indent, sets = byOldName[old]
        d = dict(sets)
        if old in dropSet:
            url = d["bigDataUrl"]
            dropped.append((old, url if re.match(r"https?://", url) else baseUrl + url))
            return
        extra = None
        forceHide = False
        if depth == 0:
            forceHide = old not in VISIBLE_BY_DEFAULT  # keep a new alpha track quiet by default
            if old in STANDALONE_GROUP:
                extra = ["group %s" % STANDALONE_GROUP[old]]
            else:
                extra = ["parent danioCode", "priority %d" % (NESTED_ORDER.index(old) + 1)]
            if "visibility" not in d:
                extra.append("visibility hide")
        emitStanza(depth * 4, sets, extra=extra, forceHide=forceHide)
        emitted.add(old)
        for c in childrenOf.get(old, []):
            walk(c, depth + 1)

    for top in TOP_ORDER:
        if top not in byOldName:
            sys.exit("hub trackDb has no top-level track %s" % top)
        walk(top, 0)

    # The CRISPR tracks carved out of ComparativeGenomics: their own top-level
    # superTrack, not a hub stanza, so it is written directly rather than via
    # walk()/emitStanza's usual "renumber one of the hub's own stanzas" path.
    nameMap[CRISPR_CONTAINER] = CRISPR_CONTAINER
    out.append("track %s" % CRISPR_CONTAINER)
    out.append("superTrack on")
    out.append("shortLabel %s" % CRISPR_LABELS[0])
    out.append("longLabel %s" % CRISPR_LABELS[1])
    out.append("group %s" % CRISPR_GROUP)
    out.append("visibility hide")
    out.append("")
    for oldChild in CRISPR_CHILDREN_OLD:
        indent, sets = byOldName[oldChild]
        if oldChild in dropSet:
            url = dict(sets)["bigDataUrl"]
            dropped.append((oldChild, url if re.match(r"https?://", url) else baseUrl + url))
            continue
        emitStanza(4, sets, parentOverride=CRISPR_CONTAINER)
        emitted.add(oldChild)

    missed = set(byOldName) - emitted - set(o for o, u in dropped)
    if missed:
        sys.exit("stanzas not emitted (unexpected hub structure): %s" %
                 ", ".join(sorted(missed)))

    with open(outFname, "w") as fh:
        fh.write("\n".join(out).rstrip() + "\n")

    sys.stderr.write("wrote %s: %d stanzas emitted, %d dropped\n" %
                     (outFname, len(emitted), len(dropped)))
    for old, url in dropped:
        sys.stderr.write("  dropped %s (missing on server: %s)\n" % (old, url))
    if tagMap:
        sys.stderr.write("%d subGroup tags renamed\n" % len(tagMap))
    if droppedTags:
        from collections import Counter
        c = Counter((k, t) for trk, k, t in droppedTags)
        for (k, t), n in sorted(c.items()):
            sys.stderr.write("dropped tag '%s' from %d stanzas of type '%s' "
                             "(not allowed by tagTypes.tab)\n" % (k, n, t))
    for comp, dim, tag in prunedTags:
        sys.stderr.write("  pruned unused subGroup tag %s.%s=%s\n" % (comp, dim, tag))


main()
