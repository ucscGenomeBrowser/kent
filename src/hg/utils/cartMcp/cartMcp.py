#!/usr/bin/env python3
"""cartMcp.py - an MCP server that composes a Genome Browser session from a
natural-language request, and records every place today's var=val cart format
made that composition harder than it should be.

Prototype consumer for Redmine #37838 (store the cart as JSON).  The point is
not the chat interface; it is the friction log.  Every time this server has to
apply a naming rule that is nowhere written down, guess at a variable name, or
give up because the format cannot express something, it appends a record to
frictionLog.jsonl with a stable category id.  `friction_report` turns that log
into the empirical requirements list for the JSON schema.

Talks MCP over stdio as newline-delimited JSON-RPC 2.0, with no third-party
imports, because hgwdev's system python is 3.9 and the mcp SDK wants 3.10+.

Sibling of hg/utils/cartTrackVarCatalog (whose catalog it imports) and
hg/utils/urlCommandCatalog.
"""

import json
import os
import re
import subprocess
import sys
import time
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
CATALOG_DIR = os.path.join(HERE, os.pardir, "cartTrackVarCatalog")

DEFAULT_DB = "hg38"
CGI_BASE = "https://hgwdev-braney.gi.ucsc.edu/cgi-bin/"
OUT_DIR = os.path.expanduser("~/public_html/cartMcp")
OUT_URL = "https://hgwdev-braney.gi.ucsc.edu/~braney/cartMcp/"
FRICTION_LOG = os.path.join(OUT_DIR, "frictionLog.jsonl")

VIS_VALUES = ("hide", "dense", "squish", "pack", "full")
IDENT_RE = re.compile(r"^[A-Za-z0-9_.\- ]{1,255}$")
COORD_RE = re.compile(r"^(chr[A-Za-z0-9_.\-]+):([0-9,]+)-([0-9,]+)$")

SERVER_INFO = {"name": "cartMcp", "version": "0.1"}


# ---------------------------------------------------------------------------
# the friction log - the actual deliverable
# ---------------------------------------------------------------------------

# Each entry is (category, what a JSON cart would do instead).  Kept in one
# place so the report can explain a category even if it never fired.
FRICTION_KINDS = {
    "compositeSelMangling":
        "Turning on one subtrack takes two variables in two different naming "
        "conventions (<composite>=<vis> plus <subtrack>_sel=1, legacy "
        "underscore).  JSON would nest the subtrack under the composite and "
        "let the server derive both.",
    "viewLevelMangling":
        "A subtrack under a view needs <composite>.<view>.vis, a three-level "
        "name assembled by string concatenation.  JSON nests it.",
    "superTrackShow":
        "A superTrack child needs its parent set to 'show', a value that is "
        "not one of the five visibilities every other track uses.",
    "explicitHides":
        "'Just these tracks' cannot be said.  A session file is a full cart, "
        "so every default-on track has to be named and hidden one by one.  "
        "JSON could carry a single 'startFromEmpty' flag.",
    "releaseGatedTrack":
        "trackDb on hgwdev contains alpha-only tracks that do not exist on "
        "the RR, and the 'release alpha' tag is consumed when the trackDb "
        "table is built, so it is not stored anywhere in the table.  Nothing "
        "local can tell a generator which names its output is allowed to use; "
        "answering the question takes a network call to api.genome.ucsc.edu.",
    "hubIdUnknowable":
        "Visibility for a hub track is stored under hub_<id>_<track>, where "
        "<id> is a hubStatus row number assigned when the server first loads "
        "the hub.  A caller cannot write that name ahead of time, so a hub "
        "track cannot be given a visibility in the same payload that attaches "
        "the hub.  JSON would let the caller say {bigDataUrl, visibility} and "
        "let the server resolve the id.",
    "customTrackOutOfBand":
        "A user's bigBed cannot go in the session payload at all.  Custom "
        "tracks persist as ctfile_<db> pointing at a server-side trash file, "
        "so the file has to be attached by a second, different mechanism "
        "(hgt.customText on the URL) that the session format knows nothing "
        "about.",
    "containerVisConflict":
        "Two subtracks of one composite requested at different visibilities "
        "both write the composite's single visibility slot, so one silently "
        "overwrites the other and nothing reports the conflict.  Nested JSON "
        "makes the container's visibility one field that is written once, and "
        "a conflict becomes something the server can see and answer.",
    "noValidation":
        "The cart accepts every name it is handed and silently drops what it "
        "does not recognise, so a wrong track name produces a 200 and a "
        "session quietly missing the track.  There is no way for a generator "
        "to be told it was wrong.",
    "noTypeToCatalogMap":
        "Nothing maps a trackDb type string to the catalog's config-type key, "
        "so which variables are legal for a given track has to be guessed "
        "from the type's first word.",
    "positionNotAddressable":
        "Resolving a gene symbol to coordinates is only available inside the "
        "CGIs (hgFindSpec), so a caller has to reimplement it against the "
        "assembly tables.",
}


def log_friction(kind, detail, ctx=None):
    """Append one friction record.  Never raises; a logging failure must not
    break the tool call."""
    rec = {"time": time.strftime("%Y-%m-%d %H:%M:%S"), "kind": kind,
           "detail": detail}
    if ctx:
        rec["ctx"] = ctx
    try:
        os.makedirs(OUT_DIR, exist_ok=True)
        with open(FRICTION_LOG, "a") as f:
            f.write(json.dumps(rec) + "\n")
    except OSError:
        pass
    return rec


# ---------------------------------------------------------------------------
# trackDb access
# ---------------------------------------------------------------------------

def sql(db, query):
    """Run one query through hgsql, return rows as lists of strings."""
    p = subprocess.run(["hgsql", "-N", db, "-e", query],
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       universal_newlines=True, timeout=120)
    if p.returncode != 0:
        raise RuntimeError("hgsql failed: " + p.stderr.strip()[:400])
    out = p.stdout
    if not out.strip():
        return []
    return [line.split("\t") for line in out.rstrip("\n").split("\n")]


def sql_str(s):
    """Quote a string for a MySQL literal."""
    return "'" + s.replace("\\", "\\\\").replace("'", "\\'") + "'"


def parse_settings(blob):
    """hgsql -N renders the settings longblob with literal backslash-n between
    lines.  Split those back apart into {setting: value}."""
    d = {}
    for line in blob.split("\\n"):
        line = line.strip()
        if not line:
            continue
        parts = line.split(None, 1)
        d[parts[0]] = parts[1] if len(parts) > 1 else ""
    return d


class _Settings(dict):
    """Settings parsed on first access; 44k stanzas is 20MB of trackDb and most
    of it is never looked at."""

    def __init__(self, blob):
        dict.__init__(self)
        self._blob = blob
        self._done = False

    def _fill(self):
        if not self._done:
            self._done = True
            dict.update(self, parse_settings(self._blob))

    def __getitem__(self, k):
        self._fill()
        return dict.__getitem__(self, k)

    def __contains__(self, k):
        self._fill()
        return dict.__contains__(self, k)

    def get(self, k, d=None):
        self._fill()
        return dict.get(self, k, d)

    def items(self):
        self._fill()
        return dict.items(self)


_TDB_CACHE = {}          # db -> {track: row}
_KIDS_CACHE = {}         # db -> {parent: [(row, defaultSelected)]}


def load_tdb(db):
    """Whole trackDb in one query.  Per-track queries were the wrong shape:
    one hgsql process per lookup, thousands of them per session."""
    if db in _TDB_CACHE:
        return _TDB_CACHE[db]
    rows = {}
    for r in sql(db, "select tableName,shortLabel,longLabel,type,visibility,"
                     "grp,settings from trackDb"):
        while len(r) < 7:
            r.append("")
        rows[r[0]] = {"track": r[0], "shortLabel": r[1], "longLabel": r[2],
                      "type": r[3], "tdbVis": int(r[4] or 0), "group": r[5],
                      "settings": _Settings(r[6])}
    _TDB_CACHE[db] = rows
    kids = {}
    for row in rows.values():
        p, sel = parent_of(row)
        if p:
            kids.setdefault(p, []).append((row, sel))
    _KIDS_CACHE[db] = kids
    return rows


def tdb(db, track):
    """One trackDb row as a dict, or None."""
    if not IDENT_RE.match(track):
        raise ValueError("implausible track name: %r" % track)
    return load_tdb(db).get(track)


def parent_of(row):
    """(parentTrackName, defaultSelected) from the parent/subTrack setting."""
    s = row["settings"].get("parent") or row["settings"].get("subTrack")
    if not s:
        return None, None
    toks = s.split()
    sel = None
    if len(toks) > 1 and toks[1] in ("on", "off"):
        sel = toks[1] == "on"
    return toks[0], sel


# Not trackLeavesOnly: that form omits composite and superTrack containers, and
# a container name is exactly what a subtrack request has to emit.
PROD_API = "https://api.genome.ucsc.edu/list/tracks?genome="
CACHE_DIR = os.path.expanduser("~/.cache/cartMcp")
CACHE_TTL = 24 * 3600
_PROD_CACHE = {}


def _collect_track_names(node, out):
    """Walk the API's nested track tree; subtracks are keys inside the parent."""
    for k, v in node.items():
        if isinstance(v, dict) and ("shortLabel" in v or "type" in v
                                    or "track" in v):
            out.add(k)
            _collect_track_names(v, out)


def production_tracks(db):
    """Track names that exist on the public site, or None if unknown.

    The local trackDb cannot answer this: hgTrackDb consumes the 'release'
    tag when it builds the table, so an alpha-only track on hgwdev looks
    exactly like a production one.  The only way to know is to ask the public
    API, which is a different service on a different machine."""
    if db in _PROD_CACHE:
        return _PROD_CACHE[db]
    path = os.path.join(CACHE_DIR, "prodTracks.%s.json" % db)
    names = None
    try:
        if (os.path.exists(path)
                and time.time() - os.path.getmtime(path) < CACHE_TTL):
            with open(path) as f:
                names = set(json.load(f))
        else:
            import urllib.request
            with urllib.request.urlopen(PROD_API + urllib.parse.quote(db),
                                        timeout=30) as r:
                d = json.loads(r.read().decode("utf-8", "replace"))
            names = set()
            _collect_track_names(d.get(db) or {}, names)
            os.makedirs(CACHE_DIR, exist_ok=True)
            with open(path, "w") as f:
                json.dump(sorted(names), f)
    except Exception:
        names = None                    # offline or unknown assembly
    _PROD_CACHE[db] = names
    return names


def is_alpha_only(row, db=None):
    """True if this track is on hgwdev but not on the public site."""
    if row["settings"].get("release", "").strip() == "alpha":
        return True                     # kept for hub/user trackDbs
    if db is None:
        return False
    prod = production_tracks(db)
    if prod is None:
        return False
    return row["track"] not in prod


def chain(db, track):
    """Container chain from the track up to the top, nearest parent first."""
    out = []
    row = tdb(db, track)
    seen = set()
    while row:
        p, _sel = parent_of(row)
        if not p or p in seen:
            break
        seen.add(p)
        prow = tdb(db, p)
        if not prow:
            break
        out.append(prow)
        row = prow
    return out


# ---------------------------------------------------------------------------
# position
# ---------------------------------------------------------------------------

def resolve_position(db, term, pad=1000):
    """Coordinates, or a gene symbol looked up in the assembly's gene tables.

    This duplicates a slice of hgFindSpec because position resolution is not
    reachable from outside the CGIs."""
    term = (term or "").strip()
    m = COORD_RE.match(term)
    if m:
        return term, "coordinates as given"
    if not IDENT_RE.match(term):
        raise ValueError("cannot resolve position: %r" % term)
    log_friction("positionNotAddressable",
                 "resolved %r by querying gene tables directly" % term,
                 {"db": db, "term": term})
    tables = [("ncbiRefSeqCurated", "name2"), ("ncbiRefSeq", "name2"),
              ("refGene", "name2")]
    have = {r[0] for r in sql(db, "show tables like 'ncbiRefSeq%'")}
    have |= {r[0] for r in sql(db, "show tables like 'refGene'")}
    for table, field in tables:
        if table not in have:
            continue
        rows = sql(db, "select chrom,min(txStart),max(txEnd) from %s "
                       "where %s=%s group by chrom order by "
                       "max(txEnd)-min(txStart) desc limit 1"
                       % (table, field, sql_str(term)))
        if rows and rows[0][0] and rows[0][0] != "NULL":
            chrom, start, end = rows[0][0], int(rows[0][1]), int(rows[0][2])
            return ("%s:%d-%d" % (chrom, max(1, start - pad), end + pad),
                    "%s.%s" % (table, field))
    raise ValueError("no gene named %r in %s (try coordinates)" % (term, db))


# ---------------------------------------------------------------------------
# session composition
# ---------------------------------------------------------------------------

def encode_val(v):
    """Session files are read back through decodeForHgSession, so values are
    html-encoded on the way out."""
    return (str(v).replace("&", "&amp;").replace("<", "&lt;")
            .replace(">", "&gt;"))


def track_vars(db, spec, notes):
    """Cart var=val pairs that put one requested track at one visibility.

    spec is {"track":..., "visibility":..., "settings": {...}}.  Returns a list
    of (var, val) and appends human-readable notes about the mangling applied.
    """
    name = spec["track"]
    vis = spec.get("visibility", "pack")
    if vis not in VIS_VALUES:
        raise ValueError("visibility must be one of %s, got %r"
                         % (", ".join(VIS_VALUES), vis))
    row = tdb(db, name)
    if row is None:
        # The cart would have accepted this silently.  We do not.
        log_friction("noValidation",
                     "track %r is not in %s.trackDb; the cart would have "
                     "taken it without complaint" % (name, db),
                     {"db": db, "track": name})
        raise ValueError("no track named %r in %s.trackDb" % (name, db))
    if is_alpha_only(row, db):
        log_friction("releaseGatedTrack",
                     "%s is in %s.trackDb on hgwdev but not in the public "
                     "site's track list; only the API could tell us"
                     % (name, db), {"db": db, "track": name})
        notes.append("%s exists on hgwdev but not on the public site, so this "
                     "session will not load there" % name)

    pairs = []
    ancestors = chain(db, name)
    if not ancestors:
        pairs.append((name, vis))
    else:
        nearest = ancestors[0]
        view = nearest["settings"].get("view")
        top = ancestors[-1]
        if view:
            composite = top["track"]
            pairs.append((composite, vis))
            pairs.append(("%s.%s.vis" % (composite, view), vis))
            pairs.append(("%s_sel" % name, "1"))
            log_friction("viewLevelMangling",
                         "%s sits under view %s of %s, needing the "
                         "three-level name %s.%s.vis"
                         % (name, view, composite, composite, view),
                         {"db": db, "track": name})
            notes.append("%s is in view '%s' of composite %s: emitted "
                         "%s.%s.vis plus %s_sel"
                         % (name, view, composite, composite, view, name))
        elif "compositeTrack" in nearest["settings"]:
            pairs.append((nearest["track"], vis))
            pairs.append(("%s_sel" % name, "1"))
            pairs.append((name, vis))
            log_friction("compositeSelMangling",
                         "%s is a subtrack of composite %s, so it took "
                         "%s=%s plus %s_sel=1 (dot convention for the vis, "
                         "underscore for the checkbox)"
                         % (name, nearest["track"], nearest["track"], vis,
                            name),
                         {"db": db, "track": name,
                          "composite": nearest["track"]})
            notes.append("%s is a subtrack of %s: emitted %s=%s and %s_sel=1"
                         % (name, nearest["track"], nearest["track"], vis,
                            name))
        elif "superTrack" in nearest["settings"]:
            pairs.append((name, vis))
            pairs.append((nearest["track"], "show"))
            log_friction("superTrackShow",
                         "%s hangs off superTrack %s, which takes 'show', "
                         "not one of the five visibilities"
                         % (name, nearest["track"]),
                         {"db": db, "track": name})
            notes.append("%s is under superTrack %s: also emitted %s=show"
                         % (name, nearest["track"], nearest["track"]))
        else:
            pairs.append((name, vis))

    for var, val in sorted((spec.get("settings") or {}).items()):
        pairs.append(("%s.%s" % (name, var), val))
    return pairs


def children(db, container):
    """trackDb rows whose parent is `container`, with their default-selected
    state."""
    load_tdb(db)
    return _KIDS_CACHE[db].get(container, [])


def default_visible_tracks(db):
    """Every track a fresh cart would draw, which is what a 'just these tracks'
    request has to turn off one name at a time.

    Not simply visibility>0: 20,257 rows in hg38.trackDb have that, but nearly
    all are subtracks of a hidden composite.  A track draws only if it is
    visible and every container above it is shown, so this walks down from the
    containers that are shown by default."""
    rows = load_tdb(db)
    top, shown = [], []
    for row in rows.values():
        if row["tdbVis"] > 0 and parent_of(row)[0] is None:
            top.append(row["track"])
        if row["settings"].get("superTrack", "").startswith("on show"):
            shown.append(row["track"])
    kids = []
    for sup in shown:
        for row, _sel in children(db, sup):
            if row["tdbVis"] > 0:
                kids.append(row["track"])
    return sorted(top), sorted(shown), sorted(kids)


def json_cart_preview(db, position, specs, custom, hubs, hide_others):
    """The same request in the shape #37838 proposes, for side-by-side with the
    var=val block."""
    doc = {"cartVersion": 1, "db": db, "position": position}
    if hide_others:
        doc["startFromEmpty"] = True
    tracks = {}
    for spec in specs:
        entry = {"visibility": spec.get("visibility", "pack")}
        if spec.get("settings"):
            entry.update(spec["settings"])
        ancestors = chain(db, spec["track"])
        if ancestors:
            top = ancestors[-1]["track"]
            nearest = ancestors[0]
            view = nearest["settings"].get("view")
            vis = spec.get("visibility", "pack")
            node = tracks.setdefault(top, {"visibility": vis})
            if node.get("visibility") in VIS_VALUES and vis in VIS_VALUES:
                node["visibility"] = max(node["visibility"], vis,
                                         key=VIS_VALUES.index)
            container = node.setdefault("subtracks", {})
            if view:
                container = (container.setdefault(view, {})
                             .setdefault("subtracks", {}))
            container[spec["track"]] = entry
        else:
            tracks[spec["track"]] = entry
    for c in custom:
        tracks[c["name"]] = {"visibility": c.get("visibility", "pack"),
                             "type": c.get("type", "bigBed"),
                             "bigDataUrl": c["bigDataUrl"],
                             "shortLabel": c.get("name")}
    for h in hubs:
        doc.setdefault("hubs", []).append({"hubUrl": h})
    doc["tracks"] = tracks
    return doc


def build_session(args):
    db = args.get("db") or DEFAULT_DB
    name = args.get("name") or ("aiSession%d" % int(time.time()))
    if not re.match(r"^[A-Za-z0-9_\-]{1,64}$", name):
        raise ValueError("session name must be alphanumeric/underscore/dash")
    hide_others = bool(args.get("hideOtherTracks", True))
    notes = []

    specs = args.get("tracks") or []
    if not isinstance(specs, list) or not specs:
        raise ValueError("tracks must be a non-empty list")
    specs = [{"track": s["track"], "visibility": s.get("visibility", "pack"),
              "settings": s.get("settings")} if isinstance(s, dict)
             else {"track": s, "visibility": "pack", "settings": None}
             for s in specs]
    custom = args.get("bigDataUrls") or []
    hubs = args.get("hubUrls") or []

    position, how = resolve_position(db, args.get("position") or "BRCA1")
    if how != "coordinates as given":
        notes.append("position resolved via %s" % how)

    # pix has to be here: with no image width in the cart, hgTracks answers the
    # first request with a JS stub that calls addPixAndReloadPage() instead of
    # drawing anything.
    pairs = [("db", db), ("position", position), ("lastPosition", position),
             ("pix", str(int(args.get("pix") or 1400)))]
    named = set()
    seen = {}                           # var -> index in pairs
    for spec in specs:
        for var, val in track_vars(db, spec, notes):
            named.add(var.split(".")[0].split("_sel")[0])
            if var in seen:
                old = pairs[seen[var]][1]
                if old == val:
                    continue
                if old in VIS_VALUES and val in VIS_VALUES:
                    # One container, one visibility slot, two requests.
                    keep = max(old, val, key=VIS_VALUES.index)
                    log_friction("containerVisConflict",
                                 "%s was asked for both %s and %s by "
                                 "different subtracks; kept %s"
                                 % (var, old, val, keep),
                                 {"db": db, "container": var})
                    notes.append("%s got conflicting visibilities (%s vs %s) "
                                 "from its subtracks; used %s"
                                 % (var, old, val, keep))
                    pairs[seen[var]] = (var, keep)
                    continue
                pairs[seen[var]] = (var, val)
                continue
            seen[var] = len(pairs)
            pairs.append((var, val))

    if hide_others:
        top, supers, kids = default_visible_tracks(db)
        hidden = [t for t in top + kids if t not in named]
        for t in hidden:
            pairs.append((t, "hide"))
        # A superTrack whose children are all off should be collapsed too.
        for sup in supers:
            if sup not in named and not any(
                    c[0]["track"] in named for c in children(db, sup)):
                pairs.append((sup, "hide"))
        # Subtracks of a composite we are turning on come with their own
        # default-selected state, so the siblings we did not ask for have to be
        # deselected by name, in the legacy _sel convention.
        siblings = []
        for spec in specs:
            anc = chain(db, spec["track"])
            if not anc:
                continue
            near = anc[0]
            if "compositeTrack" not in near["settings"]:
                continue
            for row, sel in children(db, near["track"]):
                if row["track"] in named or sel is False:
                    continue
                if row["track"] in siblings:
                    continue
                siblings.append(row["track"])
                pairs.append(("%s_sel" % row["track"], "0"))
        if hidden or siblings:
            log_friction("explicitHides",
                         "to show %d requested tracks: hid %d default-on "
                         "tracks by name and deselected %d sibling subtracks "
                         "with _sel=0"
                         % (len(specs), len(hidden), len(siblings)),
                         {"db": db, "hiddenCount": len(hidden),
                          "siblingCount": len(siblings),
                          "requested": [s["track"] for s in specs]})
            notes.append("hid %d default-on tracks by name and deselected %d "
                         "sibling subtracks to get a clean view"
                         % (len(hidden), len(siblings)))

    # A user's own bigBed cannot ride in the session payload.
    extra_cgi = []
    if custom:
        ct_lines = []
        for c in custom:
            if "bigDataUrl" not in c:
                raise ValueError("each bigDataUrls entry needs a bigDataUrl")
            cname = c.get("name") or "userTrack"
            ct_lines.append(
                'track type=%s name="%s" description="%s" visibility=%s '
                'bigDataUrl=%s'
                % (c.get("type", "bigBed"), cname,
                   c.get("description", cname),
                   c.get("visibility", "pack"), c["bigDataUrl"]))
        os.makedirs(OUT_DIR, exist_ok=True)
        ct_path = os.path.join(OUT_DIR, name + ".ct.txt")
        with open(ct_path, "w") as f:
            f.write("\n".join(ct_lines) + "\n")
        extra_cgi.append(("hgt.customText", OUT_URL + name + ".ct.txt"))
        log_friction("customTrackOutOfBand",
                     "%d bigBed(s) attached with hgt.customText on the URL "
                     "because the session format cannot carry them"
                     % len(custom),
                     {"db": db, "count": len(custom)})
        notes.append("%d bigBed(s) attached out of band via hgt.customText; "
                     "their visibility lives in the track line, not the cart"
                     % len(custom))

    for h in hubs:
        extra_cgi.append(("hubUrl", h))
        log_friction("hubIdUnknowable",
                     "attached hub %s but could not preset visibility for "
                     "its tracks; hub_<id>_<track> needs an id the server "
                     "assigns at load" % h, {"db": db, "hubUrl": h})
        notes.append("hub %s attached, but no visibility could be preset for "
                     "its tracks" % h)

    os.makedirs(OUT_DIR, exist_ok=True)
    path = os.path.join(OUT_DIR, name + ".txt")
    with open(path, "w") as f:
        f.write("# session written by cartMcp for #37838\n")
        for var, val in pairs:
            f.write("%s %s\n" % (var, encode_val(val)))

    url = (CGI_BASE + "hgTracks?hgS_doLoadUrl=submit&hgS_loadUrlName="
           + urllib.parse.quote(OUT_URL + name + ".txt", safe=""))
    for var, val in extra_cgi:
        url += "&%s=%s" % (var, urllib.parse.quote(val, safe=""))

    preview = json_cart_preview(db, position, specs, custom, hubs, hide_others)
    body = ["Session ready: %s" % url, "",
            "Session file: %s%s.txt  (%d cart variables)"
            % (OUT_URL, name, len(pairs))]
    if notes:
        body += ["", "What it took:"] + ["  - " + n for n in notes]
    shown = [p for p in pairs if p[1] != "hide"]
    body += ["", "Emitted (hide lines omitted, %d of them):" % (
        len(pairs) - len(shown))]
    body += ["  %s %s" % (v, x) for v, x in shown]
    body += ["", "The same request as a #37838 JSON cart, for comparison:",
             json.dumps(preview, indent=2)]
    return "\n".join(body)


# ---------------------------------------------------------------------------
# the other tools
# ---------------------------------------------------------------------------

def search_tracks(args):
    db = args.get("db") or DEFAULT_DB
    q = (args.get("query") or "").strip()
    limit = min(int(args.get("limit") or 15), 50)
    if not q:
        raise ValueError("query is required")
    like = "%" + q.replace("\\", "").replace("%", "") + "%"
    rows = sql(db, "select tableName,shortLabel,longLabel,type,visibility,grp "
                   "from trackDb where shortLabel like {0} or longLabel like "
                   "{0} or tableName like {0} limit 300"
               .format(sql_str(like)))
    if not rows:
        return "No track in %s.trackDb matches %r." % (db, q)
    ql = q.lower()

    def rank(r):
        t, short = r[0].lower(), r[1].lower()
        return (0 if t == ql else 1 if short == ql else
                2 if short.startswith(ql) else 3 if ql in t else 4,
                1 if "alpha" in t else 0, len(t))
    rows.sort(key=rank)
    rows = rows[:limit]
    out = ["%d match(es) in %s (showing %d):" % (len(rows), db, len(rows))]
    for r in rows:
        row = tdb(db, r[0])
        parent, _sel = parent_of(row) if row else (None, None)
        flags = []
        if row and is_alpha_only(row, db):
            flags.append("hgwdev only, NOT on the public site")
        if parent:
            prow = tdb(db, parent)
            kind = "subtrack of"
            if prow:
                if "superTrack" in prow["settings"]:
                    kind = "under superTrack"
                elif prow["settings"].get("view"):
                    kind = "in view %s of" % prow["settings"]["view"]
            flags.append("%s %s" % (kind, parent))
        if row and "compositeTrack" in row["settings"]:
            flags.append("composite container")
        if row and "superTrack" in row["settings"]:
            flags.append("superTrack container")
        out.append("  %-28s %-34s [%s] group=%s%s"
                   % (r[0], r[1][:34], r[3], r[5],
                      ("  <- " + "; ".join(flags)) if flags else ""))
    return "\n".join(out)


def _catalog():
    if CATALOG_DIR not in sys.path:
        sys.path.insert(0, CATALOG_DIR)
    import cartTrackVarCatalog
    return cartTrackVarCatalog.build()


def describe_track(args):
    db = args.get("db") or DEFAULT_DB
    track = args["track"]
    row = tdb(db, track)
    if row is None:
        log_friction("noValidation",
                     "describe_track asked about %r, which is not in "
                     "%s.trackDb" % (track, db), {"db": db, "track": track})
        return "No track named %r in %s.trackDb." % (track, db)
    out = ["%s  (%s)" % (row["track"], row["shortLabel"]),
           "  longLabel:  %s" % row["longLabel"],
           "  type:       %s" % row["type"],
           "  group:      %s" % row["group"],
           "  trackDb vis: %d" % row["tdbVis"]]
    if is_alpha_only(row, db):
        out.append("  release:    hgwdev only - not in the public site's "
                   "track list (per api.genome.ucsc.edu)")
    anc = chain(db, track)
    if anc:
        out.append("  containers: " + " <- ".join(a["track"] for a in anc))
        _p, sel = parent_of(row)
        if sel is not None:
            out.append("  default selected in its composite: %s"
                       % ("on" if sel else "off"))
    subs = sql(db, "select tableName,shortLabel from trackDb where "
                   "settings like %s limit 40"
               % sql_str("%parent " + track + "%"))
    if subs:
        out.append("  children (%d shown):" % len(subs))
        for s in subs:
            out.append("      %-26s %s" % (s[0], s[1][:40]))

    # what a caller is allowed to set, from the #37838 catalog
    try:
        cat = _catalog()
    except Exception as e:                      # catalog is advisory here
        out.append("  (catalog unavailable: %s)" % e)
        return "\n".join(out)
    types = cat["levels"]["3_byType"]["types"]
    first = row["type"].split()[0] if row["type"] else ""
    key = None
    for k, v in types.items():
        if first in (v.get("tdbTypes") or []) or k == first:
            key = k
            break
    if key is None:
        log_friction("noTypeToCatalogMap",
                     "trackDb type %r has no entry in the catalog's byType "
                     "table; had to fall back to common vars only"
                     % row["type"], {"db": db, "track": track,
                                     "type": row["type"]})
        out.append("  catalog: no config-type entry for type %r; only the "
                   "common variables apply" % row["type"])
    else:
        vars_ = types[key].get("vars") or []
        out.append("  catalog config vars for type '%s' (%d):" % (key,
                                                                  len(vars_)))
        for v in vars_[:25]:
            sep = v.get("sep", ".")
            vals = ("  one of: " + ", ".join(v["values"])) if v.get("values") \
                else ""
            out.append("      %s%s%s  (%s)%s"
                       % (track, sep, v["name"], v["type"], vals))
        if len(vars_) > 25:
            out.append("      ... %d more" % (len(vars_) - 25))

    # filters this particular track declares
    filters = {k: v for k, v in row["settings"].items()
               if k.startswith(("filter", "labelFields", "label."))}
    if filters:
        out.append("  trackDb-declared filters/labels (%d):" % len(filters))
        for k in sorted(filters)[:20]:
            out.append("      %s = %s" % (k, filters[k][:70]))
    return "\n".join(out)


def friction_report(args):
    """Read the log back as counted categories: the requirements list."""
    if not os.path.exists(FRICTION_LOG):
        return ("No friction recorded yet.  Run build_session a few times "
                "first.")
    counts, examples, sessions = {}, {}, 0
    with open(FRICTION_LOG) as f:
        for line in f:
            try:
                rec = json.loads(line)
            except ValueError:
                continue
            k = rec.get("kind", "unknown")
            counts[k] = counts.get(k, 0) + 1
            examples.setdefault(k, rec.get("detail", ""))
            sessions += 1
    out = ["Friction log: %d records, %s" % (sessions, FRICTION_LOG), ""]
    for k in sorted(counts, key=lambda x: -counts[x]):
        out.append("%-24s %4d" % (k, counts[k]))
        out.append("    seen: %s" % examples[k])
        out.append("    JSON: %s" % FRICTION_KINDS.get(k, "(uncategorised)"))
        out.append("")
    unfired = [k for k in FRICTION_KINDS if k not in counts]
    if unfired:
        out.append("Categories not yet triggered: " + ", ".join(unfired))
    return "\n".join(out)


# ---------------------------------------------------------------------------
# MCP plumbing
# ---------------------------------------------------------------------------

TRACK_SPEC_SCHEMA = {
    "type": "object",
    "properties": {
        "track": {"type": "string",
                  "description": "trackDb tableName, from search_tracks"},
        "visibility": {"type": "string", "enum": list(VIS_VALUES),
                       "default": "pack"},
        "settings": {"type": "object",
                     "description": "extra per-track config, unprefixed "
                                    "(e.g. {\"doWiggle\": \"1\"}); names come "
                                    "from describe_track"},
    },
    "required": ["track"],
}

TOOLS = [
    {"name": "search_tracks",
     "description": "Find Genome Browser tracks by name or label in an "
                    "assembly's trackDb.  Reports composite/superTrack/view "
                    "membership and flags alpha-only tracks.  Use this before "
                    "build_session so track names are real.",
     "inputSchema": {"type": "object", "properties": {
         "query": {"type": "string"},
         "db": {"type": "string", "default": DEFAULT_DB},
         "limit": {"type": "integer", "default": 15}},
         "required": ["query"]},
     "fn": search_tracks},
    {"name": "describe_track",
     "description": "Everything needed to configure one track: type, "
                    "container chain, children, the cart variables the "
                    "#37838 catalog says are legal for its type, and the "
                    "filters its trackDb stanza declares.",
     "inputSchema": {"type": "object", "properties": {
         "track": {"type": "string"},
         "db": {"type": "string", "default": DEFAULT_DB}},
         "required": ["track"]},
     "fn": describe_track},
    {"name": "build_session",
     "description": "Compose a Genome Browser session from a track list and "
                    "return a loadable URL.  Handles composite/view/"
                    "superTrack naming, hides default-on tracks, and attaches "
                    "user bigBeds or hubs.  Also prints the equivalent "
                    "#37838 JSON cart for comparison.",
     "inputSchema": {"type": "object", "properties": {
         "db": {"type": "string", "default": DEFAULT_DB},
         "position": {"type": "string",
                      "description": "gene symbol or chrN:start-end"},
         "tracks": {"type": "array", "items": TRACK_SPEC_SCHEMA},
         "bigDataUrls": {"type": "array", "items": {
             "type": "object",
             "properties": {
                 "bigDataUrl": {"type": "string"},
                 "name": {"type": "string"},
                 "type": {"type": "string", "default": "bigBed"},
                 "description": {"type": "string"},
                 "visibility": {"type": "string", "enum": list(VIS_VALUES)}},
             "required": ["bigDataUrl"]},
             "description": "the user's own bigBed/bigWig files"},
         "hubUrls": {"type": "array", "items": {"type": "string"}},
         "hideOtherTracks": {"type": "boolean", "default": True},
         "name": {"type": "string",
                  "description": "session file basename"}},
         "required": ["tracks"]},
     "fn": build_session},
    {"name": "friction_report",
     "description": "Counted summary of every place the var=val cart format "
                    "got in the way while building sessions, with what a "
                    "JSON cart would do instead.  This is the evidence for "
                    "Redmine #37838.",
     "inputSchema": {"type": "object", "properties": {}},
     "fn": friction_report},
]

TOOL_BY_NAME = {t["name"]: t for t in TOOLS}


def handle(req):
    """One JSON-RPC request in, one response dict out (or None to stay quiet)."""
    method = req.get("method")
    rid = req.get("id")
    if method == "initialize":
        pv = (req.get("params") or {}).get("protocolVersion") or "2024-11-05"
        return {"jsonrpc": "2.0", "id": rid, "result": {
            "protocolVersion": pv,
            "capabilities": {"tools": {}},
            "serverInfo": SERVER_INFO}}
    if method in ("notifications/initialized", "initialized"):
        return None
    if method == "ping":
        return {"jsonrpc": "2.0", "id": rid, "result": {}}
    if method == "tools/list":
        return {"jsonrpc": "2.0", "id": rid, "result": {"tools": [
            {k: t[k] for k in ("name", "description", "inputSchema")}
            for t in TOOLS]}}
    if method == "tools/call":
        params = req.get("params") or {}
        tool = TOOL_BY_NAME.get(params.get("name"))
        if tool is None:
            return {"jsonrpc": "2.0", "id": rid, "error": {
                "code": -32602, "message": "no such tool: %s"
                % params.get("name")}}
        try:
            text = tool["fn"](params.get("arguments") or {})
            ok = True
        except Exception as e:
            text = "%s: %s" % (type(e).__name__, e)
            ok = False
        return {"jsonrpc": "2.0", "id": rid, "result": {
            "content": [{"type": "text", "text": text}],
            "isError": not ok}}
    if rid is None:
        return None
    return {"jsonrpc": "2.0", "id": rid,
            "error": {"code": -32601, "message": "unknown method: %s"
                      % method}}


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--selftest":
        return selftest()
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except ValueError:
            continue
        resp = handle(req)
        if resp is not None:
            sys.stdout.write(json.dumps(resp) + "\n")
            sys.stdout.flush()


def selftest():
    """Exercise the tools without an MCP client."""
    calls = [
        ("search_tracks", {"query": "ClinVar"}),
        ("describe_track", {"track": "clinvarMain"}),
        ("build_session", {
            "position": "BRCA1", "name": "selftest",
            "tracks": [{"track": "knownGene", "visibility": "pack"},
                       {"track": "clinvarMain", "visibility": "pack"}],
            "bigDataUrls": [{"bigDataUrl": OUT_URL + "example.bb",
                             "name": "myPeaks"}]}),
        ("friction_report", {}),
    ]
    for name, argsd in calls:
        print("=" * 72)
        print("%s(%s)" % (name, json.dumps(argsd)[:110]))
        print("=" * 72)
        try:
            print(TOOL_BY_NAME[name]["fn"](argsd))
        except Exception as e:
            print("FAILED %s: %s" % (type(e).__name__, e))
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main() or 0)
