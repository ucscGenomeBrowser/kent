#!/usr/bin/env python3
"""
Collect the three lists behind the mirror/redistribution page (RM #37781):

  1. Tracks we are not allowed to redistribute
  2. Tracks that update themselves (otto)
  3. Contributed tracks (GenArk)

No single trackDb setting marks every restricted track, so list 1 is the union of
several tests, and each row records which ones fired:

  tableBrowser off                    the usual marker
  noGenomeReason citing license terms how OMIM is marked; a query for "off" misses it
  absent from hgdownload              ground truth for MySQL tables
  reachable on hgdownload             ground truth the other way, and the only test
                                      that catches a restricted file we are still serving

Writes collected.json for mkPage.py.
"""
import re, os, sys, json, time, argparse, subprocess, collections
from concurrent.futures import ThreadPoolExecutor

BETA     = "hgwbeta"
DL       = "https://hgdownload.soe.ucsc.edu"
SKIP_DBS = {"information_schema", "mysql", "performance_schema", "sys", "hgFixed",
            "go", "proteome", "uniProt", "visiGene"}
GENARK   = "/gbdb/genark"
CONTRIB_MAX_AGE = 7 * 86400          # re-crawl GenArk at most weekly
DL_MAX_AGE      = 20 * 3600          # re-fetch hgdownload listings at most daily

def sh(cmd, timeout=None):
    try:
        return subprocess.run(cmd, shell=True, capture_output=True, text=True,
                              errors="replace", timeout=timeout).stdout
    except subprocess.TimeoutExpired:
        return ""

def q(s):
    return "'" + s.replace("'", "'\\''") + "'"

def note(msg):
    print(msg, file=sys.stderr, flush=True)

# --- trackDb ---------------------------------------------------------------

def databases():
    out = []
    for d in sh("hgsql -h %s -N -e %s 2>/dev/null"
                % (q(BETA), q("show databases"))).split():
        if d in SKIP_DBS or d.startswith("hgcentral"):
            continue
        out.append(d)
    return sorted(out)

def parse_settings(blob):
    d = {}
    for ln in blob.replace("\\n", "\n").split("\n"):
        if " " in ln:
            k, v = ln.split(" ", 1)
            d[k.strip()] = v.strip()
    return d

def trackdb(dbs):
    tdb = collections.defaultdict(dict)
    def one(db):
        rows = {}
        for line in sh("hgsql -h %s -N -e %s %s 2>/dev/null"
                       % (q(BETA), q("select tableName, settings from trackDb"),
                          q(db))).splitlines():
            p = line.split("\t")
            if len(p) >= 2:
                rows[p[0]] = parse_settings(p[1])
        return db, rows
    with ThreadPoolExecutor(max_workers=8) as ex:
        for db, rows in ex.map(one, dbs):
            tdb[db] = rows
    return tdb

def container_of(tt, track):
    """Return the outermost composite or supertrack a track hangs off, "" if none.

    The page splits the restricted list into cohort variant-frequency projects and
    everything else. Nothing on an individual track says which it is; the only place
    that is written down is the container it belongs to, so record it here and let
    the page decide. Walks with a seen set because a parent loop in trackDb should
    produce an empty answer, not hang the nightly run."""
    cur, seen = track, set()
    while cur not in seen:
        seen.add(cur)
        p = (tt.get(cur) or {}).get("parent") or (tt.get(cur) or {}).get("subTrack") or ""
        p = p.split()[0] if p else ""     # value is "varFreqs on", we want the name
        if not p or p not in tt:
            break
        cur = p
    return "" if cur == track else cur

LICENSE_RE = re.compile(r"distribut|licen|restrict|permission|agreement", re.I)

def restricted_from_trackdb(tdb):
    out = {}
    for db, tt in tdb.items():
        for t, s in tt.items():
            tb, ngr = s.get("tableBrowser", ""), s.get("noGenomeReason", "")
            why = None
            if tb.split()[:1] == ["off"]:
                why = "tableBrowser off"
            elif ngr and LICENSE_RE.search(ngr):
                why = "noGenomeReason cites distribution terms"
            if why:
                out[(db, t)] = dict(shortLabel=s.get("shortLabel", ""),
                                    longLabel=s.get("longLabel", ""),
                                    bigDataUrl=s.get("bigDataUrl", ""),
                                    reason=ngr, why=[why])
    return out

# --- hgdownload ------------------------------------------------------------

def dl_listing(db, cache):
    f = os.path.join(cache, "dl_%s.txt" % db)
    if os.path.exists(f) and time.time() - os.path.getmtime(f) < DL_MAX_AGE:
        return db, set(open(f).read().split())
    html = sh("curl -s --max-time 90 %s" % q("%s/goldenPath/%s/database/" % (DL, db)))
    tabs = sorted(set(re.findall(r"([A-Za-z0-9_]+)\.txt\.gz", html)))
    with open(f, "w") as fh:
        fh.write("\n".join(tabs))
    return db, set(tabs)

def http_code(path):
    return sh("curl -s -o /dev/null --max-time 30 -w '%%{http_code}' -r 0-99 %s < /dev/null"
              % q(DL + path)).strip()

# --- GenArk contributed ----------------------------------------------------

def contrib_crawl(cache, refresh=False):
    """Crawl /gbdb/genark for contrib/<name> dirs. Slow (>10 min), so cached.

    Written to a temp file and renamed, because a crawl cut short mid-write
    leaves a plausible-looking but wrong list."""
    f = os.path.join(cache, "contrib.txt")
    fresh = os.path.exists(f) and time.time() - os.path.getmtime(f) < CONTRIB_MAX_AGE
    if fresh and not refresh:
        note("contrib: using cache (%.1f days old)"
             % ((time.time() - os.path.getmtime(f)) / 86400))
    else:
        note("contrib: crawling %s, this takes >10 minutes ..." % GENARK)
        tmp = f + ".tmp"
        rc = subprocess.run("find -L %s -mindepth 7 -maxdepth 7 -type d -path '*/contrib/*' "
                            "> %s 2>/dev/null" % (q(GENARK), q(tmp)), shell=True)
        if rc.returncode == 0 and os.path.getsize(tmp) > 0:
            os.replace(tmp, f)
            note("contrib: crawl done, %d rows" % sum(1 for _ in open(f)))
        else:
            if os.path.exists(tmp):
                os.remove(tmp)
            note("contrib: crawl FAILED; keeping previous cache" if os.path.exists(f)
                 else "contrib: crawl FAILED and no cache exists")
    if not os.path.exists(f):
        return []
    per = collections.defaultdict(set)
    for line in open(f):
        line = line.strip()
        if "/contrib/" not in line:
            continue
        asm, _, name = line.partition("/contrib/")
        if "/" in name or not name:
            continue
        per[name].add(os.path.basename(asm))
    return [dict(name=k, assemblies=len(v))
            for k, v in sorted(per.items(), key=lambda x: -len(x[1]))]

# --- otto ------------------------------------------------------------------

OTTO = {
 "panelApp":("PanelApp","track",["panelApp"]),
 "decipher":("DECIPHER","track",["decipher"]),
 "gwas":("GWAS Catalog","track",["gwasCatalog"]),
 "geneReviews":("GeneReviews","track",["geneReviews"]),
 "dbVar":("dbVar","track",["dbVar_"]),
 "orphanet":("Orphanet","track",["orphadata"]),
 "clinvar":("ClinVar","track",["clinvar"]),
 "mane":("MANE","track",["mane"]),
 "genCC":("GenCC","track",["genCC"]),
 "g2p":("Gene2Phenotype","track",["g2p"]),
 "omim":("OMIM","track",["omim"]),
 "lovd":("LOVD","track",["lovd"]),
 "mitoMap":("MITOMAP","track",["mitoMap"]),
 "clinGen":("ClinGen","track",["clinGen"]),
 "varChat":("VarChat","track",["varChat"]),
 "vista":("VISTA Enhancers","track",["vistaEnhancers"]),
 "civic":("CIViC","track",["civic"]),
 "pubtatorDbSnp":("PubTator","track",["pubtator"]),
 "ncbiRefSeq":("NCBI RefSeq","track",["ncbiRefSeq"]),
 "uniprot":("UniProt","track",["unipFull","unipMut","uniprot"]),
 "grcIncidentDb":("GRC Incident","track",["grcIncident"]),
 "insight":("InSiGHT VCEP ClinVar","hub",
            "updates a hub rather than a trackDb track: insightClinVar and "
            "pms2clParalogVars, on hg19 and hg38"),
 "malacards":("MalaCards","table",
              "loads the hg38 malacards table, which no track points at"),
 "refSeqHistorical":("RefSeq Historical","notifier",
                     "checks whether NCBI has a new release; changes no data"),
 "vcepVersions":("VCEP spec versions","notifier",
                 "compares our VCEP pages against the ClinGen registry"),
}
# commands the keyword match gets wrong or too coarse
OVERRIDE = {
 "omimUploadWrapper": ("infrastructure",     "pushes the OMIM tables to hgwbeta"),
 "covidCheck":        ("UniProt (wuhCor1)",  "UniProt; Mutations on wuhCor1"),
 "clinGenCspec":      ("ClinGen CSpec",      "ClinGen VCEP Specifications"),
}
INFRA = ["readOnlyKentMirror","lastLog","ottoCompareGitVsHiveFiles","liftRequest",
         "GenArk","buildPublicSessionThumbnails","generateTipOfDay","cellBrowser",
         "cbAnnotServer","tabulate_facets","tsv_to_json","updateNewsSec","tusd",
         # this job itself: it writes a page, it does not touch track data, and a
         # page that listed its own generator would be its own first entry
         "trackLists"]

def cron_english(s):
    if s.startswith("@"):
        return s
    m, h, dom, mon, dow = s.split()
    first = lambda x: int(x.split(",")[0]) if x.split(",")[0].isdigit() else 0
    t = "%02d:%02d" % (first(h), first(m))
    if dom != "*":
        return ("monthly on day %s at %s" % (dom, t) if mon == "*"
                else "day %s of months %s at %s" % (dom, mon, t))
    if dow in ("*", "1-7", "0-6"):
        return "daily at %s" % t
    if dow == "1-5":
        return "weekdays at %s" % t
    days = {"1":"Mon","2":"Tue","3":"Wed","4":"Thu","5":"Fri","6":"Sat","0":"Sun","7":"Sun",
            "mon":"Mon","tue":"Tue","wed":"Wed","thu":"Thu","fri":"Fri"}
    return "weekly (%s) at %s" % ("/".join(days.get(x.lower(), x) for x in dow.split(",")), t)

def labels_for(tdb, prefixes):
    labels, dbs = set(), set()
    for db, tt in tdb.items():
        for t, s in tt.items():
            if any(t.lower().startswith(p.lower()) for p in prefixes):
                dbs.add(db)
                if s.get("shortLabel"):
                    labels.add(s["shortLabel"])
    return sorted(labels), sorted(dbs)

def parse_otto(path, tdb):
    rows = []
    for line in open(path):
        s = line.rstrip("\n")
        if not s.strip() or s.strip().startswith("#") or re.match(r"^[A-Z_]+=", s.strip()):
            continue
        m = re.match(r"^(@\w+|\S+\s+\S+\s+\S+\s+\S+\s+\S+)\s+(.*)$", s)
        if not m:
            continue
        sched, cmd = m.group(1), m.group(2)
        hit = next((k for k in OVERRIDE if k in cmd), None)
        if hit:
            name, detail = OVERRIDE[hit]
            rows.append(dict(schedule=cron_english(sched), command=cmd, name=name, detail=detail,
                             kind="infrastructure" if name == "infrastructure" else "track"))
            continue
        key = next((k for k in OTTO if re.search(re.escape(k), cmd, re.I)), None)
        if key is None:
            kind = ("infrastructure" if any(i.lower() in cmd.lower() for i in INFRA)
                    else "unclassified")
            rows.append(dict(schedule=cron_english(sched), command=cmd, name=kind,
                             detail="", kind=kind))
            continue
        name, kind, ref = OTTO[key]
        if kind == "track":
            labels, dbs = labels_for(tdb, ref)
            rows.append(dict(schedule=cron_english(sched), command=cmd, name=name,
                             detail="; ".join(labels), assemblies=len(dbs), kind="track"))
        else:
            rows.append(dict(schedule=cron_english(sched), command=cmd, name=name,
                             detail=ref, kind=kind))
    return rows

# --- main ------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", default="/hive/data/outside/otto/trackLists/cache")
    ap.add_argument("--otto-crontab",
                    default=os.path.expanduser("~/kent/src/hg/utils/otto/otto.crontab"))
    ap.add_argument("--refresh-contrib", action="store_true",
                    help="force the GenArk crawl even if the cache is fresh")
    ap.add_argument("--no-contrib", action="store_true",
                    help="skip the GenArk list entirely (fast runs while testing)")
    ap.add_argument("-o", "--out", default="collected.json")
    a = ap.parse_args()
    os.makedirs(a.cache, exist_ok=True)

    t0 = time.time()
    dbs = databases()
    note("databases: %d" % len(dbs))
    tdb = trackdb(dbs)
    note("trackDb loaded (%.0fs)" % (time.time() - t0))

    restricted = restricted_from_trackdb(tdb)
    note("flagged in trackDb: %d" % len(restricted))

    # ground truth 1: real MySQL tracks absent from hgdownload
    t1 = time.time()
    with ThreadPoolExecutor(max_workers=12) as ex:
        listings = dict(ex.map(lambda d: dl_listing(d, a.cache), dbs))
    note("hgdownload listings: %.0fs" % (time.time() - t1))

    def tables(db):
        return set(sh("hgsql -h %s -N -e %s %s 2>/dev/null"
                      % (q(BETA), q("show tables"), q(db))).split())
    with ThreadPoolExecutor(max_workers=8) as ex:
        have = dict(zip(dbs, ex.map(tables, dbs)))
    partial = []
    for db in dbs:
        pub = listings.get(db) or set()
        if not pub:
            continue                       # nothing published for this db; no signal
        mine = have[db] & set(tdb[db])
        missing = mine - pub
        # An assembly whose downloads are simply not published yet makes every one
        # of its tracks look withheld. Only believe this test when the db is
        # otherwise well published; otherwise say so and move on.
        if mine and len(missing) > 0.2 * len(mine):
            partial.append(dict(db=db, missing=len(missing), tracks=len(mine),
                                published=len(pub)))
            continue
        for t in missing:
            if (db, t) in restricted:
                restricted[(db, t)]["why"].append("absent from hgdownload")
            else:
                s = tdb[db][t]
                restricted[(db, t)] = dict(shortLabel=s.get("shortLabel", ""),
                                           longLabel=s.get("longLabel", ""),
                                           bigDataUrl=s.get("bigDataUrl", ""),
                                           reason="", why=["absent from hgdownload"])

    # ground truth 2: anything we call restricted that hgdownload still serves
    checks = [(db, t, v["bigDataUrl"].replace("$D", db))
              for (db, t), v in restricted.items()
              if v["bigDataUrl"] and not v["bigDataUrl"].startswith("http")]
    with ThreadPoolExecutor(max_workers=8) as ex:
        codes = list(ex.map(lambda c: http_code(c[2]), checks))
    exposed, unchecked = [], []
    for (db, t, p), code in zip(checks, codes):
        restricted[(db, t)]["hgdownload"] = code
        # Only a real HTTP response says anything about the file. A curl that timed
        # out or could not connect comes back empty or as 000, and calling that
        # "reachable" both mails a false alarm and drops the all-clear line from the
        # public page on nothing more than a network blip. 4xx means blocked, 2xx and
        # 3xx mean served, and anything else means the test did not run.
        if code[:1] in ("2", "3"):
            exposed.append(dict(db=db, track=t, path=p, code=code,
                                shortLabel=restricted[(db, t)]["shortLabel"]))
        elif code[:1] != "4":
            unchecked.append(dict(db=db, track=t, path=p, code=code or "none"))

    # which composite or supertrack each one belongs to; the page groups on this
    for (db, t), v in restricted.items():
        c = container_of(tdb[db], t)
        v["container"] = c
        v["containerLabel"] = (tdb[db].get(c) or {}).get("shortLabel", "") if c else ""

    contrib = [] if a.no_contrib else contrib_crawl(a.cache, a.refresh_contrib)

    result = dict(
        restricted=[dict(db=db, track=t, **v) for (db, t), v in sorted(restricted.items())],
        exposed=exposed,
        otto=parse_otto(a.otto_crontab, tdb),
        contrib=contrib,
        partialDownloads=partial,
        uncheckedDownloads=unchecked,
        counts=dict(databases=len(dbs)),
        generated=time.strftime("%Y-%m-%d"),
    )
    json.dump(result, open(a.out, "w"), indent=1)
    note("wrote %s in %.0fs: %d restricted rows, %d exposed, %d otto jobs, %d contributors"
         % (a.out, time.time() - t0, len(result["restricted"]), len(exposed),
            len(result["otto"]), len(contrib)))
    if partial:
        note("")
        note("note: %d database(s) have too little published on hgdownload for the"
             % len(partial))
        note("      'absent from hgdownload' test to mean anything, so they were skipped:")
        for p in partial:
            note("      %-14s %d of %d trackDb tables published"
                 % (p["db"], p["published"], p["tracks"]))
    if exposed:
        note("")
        note("*** %d file(s) marked restricted are reachable on hgdownload:" % len(exposed))
        for e in exposed:
            note("      %s  %s  %s" % (e["db"], e["track"], e["path"]))
    if unchecked:
        note("")
        note("note: %d file(s) could not be checked against hgdownload (no HTTP"
             % len(unchecked))
        note("      response); they are neither reported as reachable nor as blocked:")
        for u in unchecked:
            note("      %-6s %-16s %s (%s)" % (u["db"], u["track"], u["path"], u["code"]))
    return 0

if __name__ == "__main__":
    sys.exit(main())
