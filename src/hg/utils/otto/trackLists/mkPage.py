#!/usr/bin/env python3
"""Render collected.json into the mirror-facing HTML page (RM #37781)."""
import json, html, argparse, collections, datetime

def esc(s): return html.escape(str(s or ""))

CSS = """
:root{--ink:#1a1a1a;--mut:#5b6570;--line:#d6dce2;--blue:#1c3f70;--panel:#f5f7f9;
--okc:#1a7a3c;--badc:#b3261e;--warnc:#9a6400}
*{box-sizing:border-box}
body{margin:0;background:#fff;color:var(--ink);
font:16px/1.6 -apple-system,BlinkMacSystemFont,"Segoe UI",Helvetica,Arial,sans-serif}
.wrap{max-width:1040px;margin:0 auto;padding:32px 24px 80px}
header{border-bottom:3px solid var(--blue);padding-bottom:16px;margin-bottom:26px}
h1{font-size:26px;margin:0 0 6px;color:var(--blue)}
.sub{color:var(--mut);font-size:14px}
h2{font-size:20px;margin:38px 0 10px;color:var(--blue);
border-bottom:1px solid var(--line);padding-bottom:6px}
h3{font-size:16px;margin:22px 0 8px}
p,li{margin:0 0 12px}
code{font-family:ui-monospace,Menlo,Consolas,monospace;font-size:13.5px;
background:var(--panel);padding:1px 5px;border-radius:3px}
table{border-collapse:collapse;width:100%;font-size:14px;margin:12px 0}
th,td{border:1px solid var(--line);padding:6px 9px;text-align:left;vertical-align:top}
th{background:var(--panel);font-weight:600}
td.n,th.n{text-align:right}
.note{background:var(--panel);border-left:5px solid var(--blue);padding:14px 18px;
border-radius:0 4px 4px 0;margin:0 0 16px}
.warn{border-left-color:var(--warnc)}
.scroll{overflow-x:auto}
.mut{color:var(--mut)}
.tag{display:inline-block;font-size:11px;font-weight:700;letter-spacing:.05em;
text-transform:uppercase;padding:1px 7px;border-radius:9px;background:#eef1f4;color:var(--mut)}
footer{margin-top:44px;padding-top:14px;border-top:1px solid var(--line);
font-size:13px;color:var(--mut)}
"""

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-i","--inp",default="collected.json")
    ap.add_argument("-o","--out",default="index.html")
    ap.add_argument("--date",default=None)
    ap.add_argument("--internal",action="store_true",
                    help="include the hgdownload cross-check, which names restricted files "
                         "that are currently reachable. Never use for a public page.")
    a = ap.parse_args()
    d = json.load(open(a.inp))
    today = a.date or datetime.date.today().isoformat()

    # ---- list 1: not redistributable, grouped by track name
    by = collections.defaultdict(lambda: dict(dbs=[], label="", why=set()))
    for r in d["restricted"]:
        e = by[r["track"]]
        e["dbs"].append(r["db"])
        e["label"] = e["label"] or r.get("shortLabel","")
        for w in r.get("why",[]): e["why"].add(w)
    rows1 = []
    for t, e in sorted(by.items(), key=lambda x: (x[1]["label"] or x[0]).lower()):
        rows1.append("<tr><td>%s</td><td><code>%s</code></td><td>%s</td><td class='mut'>%s</td></tr>"
                     % (esc(e["label"] or t), esc(t), esc(" ".join(sorted(set(e["dbs"])))),
                        esc("; ".join(sorted(e["why"])))))

    # ---- exposed
    exposed = d.get("exposed",[])
    if exposed and not a.internal:
        # Never name reachable restricted files on a page anyone can read.
        exposed_html = ("<p>The list above is cross-checked against the download server every "
                        "time this page is built, so that a track marked as restricted here is "
                        "actually blocked there. Discrepancies are reported to us privately "
                        "rather than shown on this page.</p>")
    elif exposed:
        ex = "".join("<tr><td>%s</td><td><code>%s</code></td><td><code>%s</code></td></tr>"
                     % (esc(r["shortLabel"] or r["track"]), esc(r["db"]), esc(r["path"]))
                     for r in exposed)
        exposed_html = ("<div class='note warn'><p><strong>%d file(s) marked as restricted are "
                        "currently reachable on hgdownload.</strong> These need to go on the "
                        "download server's exclude list.</p></div>"
                        "<div class='scroll'><table><tr><th>Track</th><th>Assembly</th>"
                        "<th>Path</th></tr>%s</table></div>" % (len(exposed), ex))
    else:
        exposed_html = ("<p>Every file marked as restricted is correctly blocked on "
                        "hgdownload. Checked on %s.</p>" % esc(today))

    # ---- list 2: otto
    data_jobs = [j for j in d["otto"] if j["kind"] in ("track","hub","table")]
    notifiers = [j for j in d["otto"] if j["kind"] == "notifier"]
    infra     = [j for j in d["otto"] if j["kind"] == "infrastructure"]
    unc       = [j for j in d["otto"] if j["kind"] == "unclassified"]
    rows2 = "".join(
        "<tr><td>%s</td><td>%s</td><td>%s</td><td class='n'>%s</td></tr>"
        % (esc(j["name"]), esc(j["schedule"]), esc(j["detail"]),
           esc(j.get("assemblies","")) if j["kind"]=="track" else "")
        for j in sorted(data_jobs, key=lambda x: x["name"].lower()))
    rows2b = "".join("<tr><td>%s</td><td>%s</td><td>%s</td></tr>"
                     % (esc(j["name"]), esc(j["schedule"]), esc(j["detail"]))
                     for j in notifiers)

    # ---- list 3: contributed
    contrib = d.get("contrib",[])
    rows3 = "".join("<tr><td>%s</td><td class='n'>%s</td></tr>" % (esc(c["name"]), c["assemblies"])
                    for c in contrib)
    contrib_total = sum(c["assemblies"] for c in contrib)

    doc = f"""<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Track redistribution, automatic updates, and contributed tracks</title>
<style>{CSS}</style></head><body><div class="wrap">

<header>
<h1>Which tracks you can mirror, which update themselves, and which came from outside</h1>
<div class="sub">Generated {esc(today)} from the hgwbeta trackDb, the otto crontab, and the
GenArk build tree &middot; Redmine
<a href="https://redmine.gi.ucsc.edu/issues/37781">#37781</a></div>
</header>

<p>People running their own copy of the Genome Browser ask us three questions often enough that
it is worth answering them in one place: which tracks we are not allowed to pass on, which tracks
change on their own, and which tracks were built by someone other than UCSC.</p>

<h2>1. Tracks we cannot redistribute</h2>
<p>These come to us under terms that let us display the data but not hand it on. You can see them
on our site, and in most cases you can get the same data yourself directly from the group that
produced it, but we cannot include them in a mirror or on the download server. The reasons vary:
some are commercial licences, some are consent agreements attached to human cohorts.</p>
<div class="scroll"><table>
<tr><th>Track</th><th>Table or track name</th><th>Assemblies</th><th>How it is marked</th></tr>
{''.join(rows1)}
</table></div>
<p class="mut">A track is listed here if any of these is true: its trackDb entry says
<code>tableBrowser off</code>; its <code>noGenomeReason</code> mentions distribution terms (this
is how OMIM is marked, and it is missed by a query that only looks for <code>off</code>); or its
table exists on our servers but is deliberately absent from hgdownload. No single one of those
catches everything, so the page checks all three.</p>

<h3>Cross-check against the download server</h3>
{exposed_html}

<h2>2. Tracks that update themselves</h2>
<p>These are refreshed on a schedule without anyone touching them, by the process we call otto.
If you mirror them, they will drift from our copy unless you re-sync. The times are US Pacific.</p>
<div class="scroll"><table>
<tr><th>Source</th><th>Runs</th><th>Tracks it updates</th><th class="n">Assemblies</th></tr>
{rows2}
</table></div>

<h3>Scheduled checks that do not change data</h3>
<p>These watch for new releases upstream and email us; they update nothing on their own.</p>
<div class="scroll"><table>
<tr><th>Job</th><th>Runs</th><th>What it checks</th></tr>
{rows2b}
</table></div>
<p class="mut">A further {len(infra)} otto jobs handle pushes, mirrors, logs and other
housekeeping rather than track data, so they are left out here.
{('<strong>' + str(len(unc)) + ' job(s) could not be classified and need a look.</strong>') if unc else ''}</p>

<h2>3. Contributed tracks</h2>
<p>Some GenArk assemblies carry annotation built by outside groups rather than by us. The data
sits alongside our own tracks but the group named below produced it, and questions about the
underlying annotation are best sent to them.</p>
<div class="scroll"><table>
<tr><th>Contributor</th><th class="n">Assemblies</th></tr>
{rows3}
</table></div>
<p class="mut">{contrib_total} assembly/contributor pairings across
{len(contrib)} contributing groups.</p>

<footer>
Built by <code>collect.py</code> and <code>mkPage.py</code> &middot; sources: hgwbeta trackDb
({d['counts'].get('databases','?')} databases), <code>~/kent/src/hg/utils/otto/otto.crontab</code>,
<code>/gbdb/genark/*/contrib/</code>, and live checks against hgdownload.
</footer>
</div></body></html>"""
    open(a.out,"w").write(doc)
    print("wrote %s (%d bytes)" % (a.out, len(doc)))

if __name__ == "__main__":
    main()
