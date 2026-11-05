#!/usr/bin/env python3
"""Render collected.json as a UCSC Genome Browser static page (RM #37781).

Emits the house static-page shape: SSI includes for the menubar and footer, no
stylesheet of its own (gbStatic.css already styles .gbsPage tables), h2 for
sections, h6 for the contents list, and ASCII-only source.
"""
import json, html, argparse, collections, datetime, textwrap

def esc(s):
    """HTML-escape and force ASCII, since the house style forbids raw UTF-8."""
    s = html.escape(str(s or ""), quote=True)
    return s.encode("ascii", "xmlcharrefreplace").decode("ascii")

def cell(w, text, indent="    "):
    """Write a <td>, wrapping so no source line runs past 100 characters."""
    text = esc(text)
    if len(indent) + len(text) + 9 <= 100:
        w("%s<td>%s</td>" % (indent, text))
        return
    w("%s<td>" % indent)
    for line in textwrap.wrap(text, width=94 - len(indent),
                              break_long_words=False, break_on_hyphens=False):
        w("%s  %s" % (indent, line))
    w("%s</td>" % indent)

# Containers whose children are population allele-frequency cohorts. Half the
# restricted list is these, all named after the country or consortium that ran the
# sequencing, and in one alphabetical table they bury the handful of tracks people
# actually write in about (OMIM, HGMD, DECIPHER). They get their own table. RM #37781
VARIANT_CONTAINERS = ("varFreqs", "phasedVars")

def rows_by_track(restricted):
    """Group the restricted rows by track, and note which row each one belongs under.

    A subtrack whose parent is restricted too has a label written to be read underneath
    that parent, so on its own it says very little and sorts nowhere useful: alphaGenome's
    four subtracks are labelled "Mutation: A" through "Mutation: T" and land under M,
    nowhere near AlphaGenome. Record the nearest ancestor that is itself listed, and let
    the table put the two back together. Every row stays, because each of these subtracks
    is its own file that a mirror cannot have.

    The per-row "why" is deliberately not carried through: which tests fired is explained
    once in prose below the table."""
    by = collections.defaultdict(lambda: dict(dbs=set(), label="", container="",
                                              containerLabel="", ancestors=[]))
    for r in restricted:
        e = by[r["track"]]
        e["dbs"].add(r["db"])
        e["label"] = e["label"] or r.get("shortLabel", "")
        e["container"] = e["container"] or r.get("container", "")
        e["containerLabel"] = e["containerLabel"] or r.get("containerLabel", "")
        e["ancestors"] = e["ancestors"] or r.get("ancestors", [])
    for t, e in by.items():
        e["anchor"] = next((a for a in e["ancestors"] if a in by), t)
    return by

def display_label(track, e):
    """The label the reader sees for one row.

    A track named after its container is part of the container rather than one of its
    members: varFreqsBackground is the combined reference file across the cohorts, not
    another cohort, and "Population reference" says that only to someone who already
    knows. Name the container in front of it. The cohorts themselves (topmed, allofus)
    are not named after varFreqs and keep their own labels."""
    label = e["label"] or track
    if e["container"] and e["containerLabel"] and track.startswith(e["container"]):
        return "%s: %s" % (e["containerLabel"], label)
    return label

def restricted_table(w, rows):
    """One table of restricted tracks.

    Sorted by container, so the three OMIM rows and the two DECIPHER rows sit together
    instead of landing wherever their own labels fall; then by the row a subtrack belongs
    under, so AlphaGenome is followed by its four; then by the label the reader sees. A
    subtrack cannot be separated from its parent by this, because the two always share an
    outermost container and so land in the same table."""
    labels = dict((t, display_label(t, e)) for t, e in rows)
    def order(item):
        t, e = item
        anchor = e.get("anchor", t)
        return ((e["containerLabel"] or labels[t]).lower(),
                labels.get(anchor, labels[t]).lower(),
                0 if anchor == t else 1,          # the parent, then what hangs off it
                labels[t].lower())
    w('<table>')
    w('  <tr>')
    w('    <th>Track</th>')
    w('    <th>Table or track name</th>')
    w('    <th>Assemblies</th>')
    w('  </tr>')
    for t, e in sorted(rows, key=order):
        w('  <tr>')
        cell(w, display_label(t, e))
        w('    <td><code>%s</code></td>' % esc(t))
        cell(w, " ".join(sorted(e["dbs"])))
        w('  </tr>')
    w('</table>')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-i", "--inp", default="collected.json")
    ap.add_argument("-o", "--out", default="mirrorTracks.html")
    ap.add_argument("--date", default=None)
    ap.add_argument("--internal", action="store_true",
                    help="include the hgdownload cross-check, which names restricted files "
                         "that are currently reachable. Never use for a public page.")
    a = ap.parse_args()
    d = json.load(open(a.inp))
    today = a.date or d.get("generated") or datetime.date.today().isoformat()
    o = []
    w = o.append

    w('<!DOCTYPE html>')
    w('<!-- DO NOT EDIT THIS FILE. It is generated by')
    w('     kent/src/hg/utils/otto/trackLists/mkPage.py -->')
    w('<!--#set var="TITLE" value="Track lists for mirror sites" -->')
    w('<!--#set var="ROOT" value="../.." -->')
    w('')
    w('<!-- Relative paths to support mirror sites with non-standard GB docs install -->')
    w('<!--#include virtual="$ROOT/inc/gbPageStart.html" -->')
    w('')
    w('<h1>Track lists for mirror sites</h1>')
    w('')
    w('<h2>Contents</h2>')
    w("<h6><a href='#notRedistributable'>Tracks we cannot redistribute</a></h6>")
    w("<h6><a href='#variantFreqs'>&nbsp;&nbsp;Variant frequency projects</a></h6>")
    w("<h6><a href='#autoUpdating'>Tracks that update themselves</a></h6>")
    w("<h6><a href='#contributed'>Contributed tracks</a></h6>")
    w('')
    w('<p>')
    w('People running their own copy of the Genome Browser ask us three questions often')
    w('enough that it is worth answering them in one place: which tracks we are not allowed')
    w('to pass on, which tracks change on their own, and which tracks were built by someone')
    w('other than UCSC. This page is rebuilt automatically, so it reflects the current state')
    w('of our servers rather than a hand-kept list.')
    w('</p>')
    w('<p>')
    w('For installation instructions see the')
    w('<a href="mirror.html">mirror documentation</a>. Questions are welcome')
    w('on the <a href="mirror.html#the-genome-mirror-mailing-list">genome-mirror')
    w('mailing list</a>.')
    w('</p>')
    w('')

    # ---- 1. not redistributable -------------------------------------------
    w("<a name='notRedistributable'></a>")
    w('<h2>Tracks we cannot redistribute</h2>')
    w('<p>')
    w('These tracks reach us under terms that let us display the data but not pass it on.')
    w('You can see them on our site, and in most cases you can obtain the same data yourself')
    w('directly from the group that produced it, but we cannot include them in a mirror or on')
    w('our download server. The reasons vary: some are commercial licenses, others are')
    w('consent agreements attached to human cohorts. Check the description page of an')
    w('individual track for who to approach about access.')
    w('</p>')
    by = rows_by_track(d["restricted"])
    freqs = [x for x in by.items() if x[1]["container"] in VARIANT_CONTAINERS]
    others = [x for x in by.items() if x[1]["container"] not in VARIANT_CONTAINERS]
    restricted_table(w, others)
    w('')
    w("<a name='variantFreqs'></a>")
    w('<h3>Variant frequency projects</h3>')
    w('<p>')
    w('The %d tracks below are also restricted, and are listed apart from the rest only' % len(freqs))
    w('because there are so many of them. Each is allele frequencies from one sequencing')
    w('cohort, usually a national project, and each carries its own agreement with the')
    w('group that collected the samples. Almost all of them are aggregate frequencies')
    w('rather than individual genotypes, but that does not make them ours to pass on.')
    w('</p>')
    restricted_table(w, freqs)
    w('')
    w('<h3>How these lists are put together</h3>')
    w('<p>')
    w('A track appears in one of the two tables above if any of three things is true of')
    w('it: its configuration says')
    w('<code>tableBrowser off</code>; its <code>noGenomeReason</code> refers to distribution')
    w('terms, which is how OMIM is marked and is missed by a search for the first setting')
    w('alone; or its table exists on our servers but is deliberately absent from the download')
    w('server. No single one of those catches everything, so all three are checked. Note that')
    w('some tracks are withheld from whole-genome Table Browser queries only because they are')
    w('too large to return, not for any licensing reason, and those are not listed above.')
    w('</p>')
    exposed = d.get("exposed", [])
    if exposed and not a.internal:
        # Never name reachable restricted files on a page anyone can read: the path
        # of a file we should be blocking is a pointer straight at it. Say only that
        # the check runs; the internal copy and the cron mail carry the detail. Do
        # not fold this branch into the all-clear one below, which would have the
        # public page claim the list is clean when the check says otherwise.
        w('<p>')
        w('Every track named above is cross-checked against the download server each time')
        w('this page is rebuilt. Any file that turns out to be reachable there is reported')
        w('to us privately rather than named on this page.')
        w('</p>')
    elif exposed:
        w('<h3>Reachable on hgdownload</h3>')
        w('<p>')
        w('%d file(s) marked as restricted are currently served by the download server and'
          % len(exposed))
        w('need to be added to its exclude list.')
        w('</p>')
        w('<table>')
        w('  <tr>')
        w('    <th>Track</th>')
        w('    <th>Assembly</th>')
        w('    <th>Path</th>')
        w('  </tr>')
        for r in exposed:
            w('  <tr>')
            cell(w, r["shortLabel"] or r["track"])
            cell(w, r["db"])
            w('    <td><code>%s</code></td>' % esc(r["path"]))
            w('  </tr>')
        w('</table>')
    else:
        # Only the tracks distributed as a file can be fetched, which is 39 of the 58
        # rows; the rest live in our database tables and there is nothing to request.
        # Say which, and say nothing at all about blocking on a run where the fetches
        # came back empty, or a network blip reads as an all-clear.
        checked = d.get("counts", {}).get("fileChecks", 0)
        unknown = len(d.get("uncheckedDownloads", []))
        w('<p>')
        w('The tracks above that we distribute as files are fetched from the download server')
        w('each time this page is rebuilt; those held in our database tables have no file to')
        w('request and are not part of that check.')
        if unknown:
            w('Of the %d files tried on %s, %d gave no answer at all, so this run could not'
              % (checked, esc(today), unknown))
            w('finish the check. The rest are correctly blocked.')
        else:
            w('All %d files tried on %s are correctly blocked there.' % (checked, esc(today)))
        w('</p>')
    w('')

    # ---- 2. otto ----------------------------------------------------------
    w("<a name='autoUpdating'></a>")
    w('<h2>Tracks that update themselves</h2>')
    w('<p>')
    w('These tracks are rebuilt on a schedule without anyone at UCSC touching them. If you')
    w('mirror them, your copy will drift from ours until you synchronize again. Times are US')
    w('Pacific.')
    w('</p>')
    w('<table>')
    w('  <tr>')
    w('    <th>Source</th>')
    w('    <th>Updated</th>')
    w('    <th>Tracks affected</th>')
    w('    <th>Assemblies</th>')
    w('  </tr>')
    jobs = [j for j in d["otto"] if j["kind"] in ("track", "hub", "table")]
    for j in sorted(jobs, key=lambda x: x["name"].lower()):
        w('  <tr>')
        cell(w, j["name"])
        cell(w, j["schedule"])
        cell(w, j["detail"])
        cell(w, j.get("assemblies", ""))
        w('  </tr>')
    w('</table>')
    notifiers = [j for j in d["otto"] if j["kind"] == "notifier"]
    if notifiers:
        w('')
        w('<h3>Scheduled checks that change no data</h3>')
        w('<p>')
        w('These watch for new releases upstream and send us mail. They update nothing on')
        w('their own, and are listed so that the schedule above is not mistaken for the whole')
        w('picture.')
        w('</p>')
        w('<table>')
        w('  <tr>')
        w('    <th>Check</th>')
        w('    <th>Runs</th>')
        w('    <th>What it looks at</th>')
        w('  </tr>')
        for j in notifiers:
            w('  <tr>')
            cell(w, j["name"], indent="    ")
            cell(w, j["schedule"], indent="    ")
            cell(w, j["detail"], indent="    ")
            w('  </tr>')
        w('</table>')
    # A job we have no description for used to be dropped from the page without a word,
    # which is how STRchive went missing from it. Naming the ones we cannot describe is
    # both honest to the reader and the thing most likely to get them described.
    unnamed = [j for j in d["otto"] if j["kind"] == "unclassified"]
    if unnamed:
        w('')
        w('<h3>Jobs we have not described yet</h3>')
        w('<p>')
        w('These run on the same schedule as the tracks above, but we have not yet written')
        w('down what they update, so treat the table above as incomplete by this much. If one')
        w('of them matters to your mirror, ask on the mailing list and we will fill it in.')
        w('</p>')
        w('<table>')
        w('  <tr>')
        w('    <th>Runs</th>')
        w('    <th>Command</th>')
        w('  </tr>')
        for j in sorted(unnamed, key=lambda x: x["command"]):
            w('  <tr>')
            cell(w, j["schedule"])
            w('    <td><code>%s</code></td>' % esc(j["command"]))
            w('  </tr>')
        w('</table>')
    w('')

    # ---- 3. contributed ---------------------------------------------------
    contrib = d.get("contrib", [])
    w("<a name='contributed'></a>")
    w('<h2>Contributed tracks</h2>')
    w('<p>')
    w('Some assemblies in our')
    w('<a href="https://hgdownload.soe.ucsc.edu/hubs/" target="_blank">GenArk</a> collection')
    w('carry annotation built by outside groups rather than by UCSC. The data sits alongside')
    w('our own tracks, but the group named below produced it, and questions about the')
    w('underlying annotation are best sent to that group.')
    w('</p>')
    w('<table>')
    w('  <tr>')
    w('    <th>Contributing group</th>')
    w('    <th>Assemblies</th>')
    w('  </tr>')
    for c in contrib:
        w('  <tr>')
        w('    <td>%s</td>' % esc(c["name"]))
        w('    <td>%d</td>' % c["assemblies"])
        w('  </tr>')
    w('</table>')
    w('<p>')
    w('%d assemblies carry contributed annotation, from %d groups.'
      % (sum(c["assemblies"] for c in contrib), len(contrib)))
    w('</p>')
    w('')
    w('<p>')
    w('This page was generated on %s from the current state of our servers.' % esc(today))
    w('</p>')
    w('')
    w('<!--#include virtual="$ROOT/inc/gbPageEnd.html" -->')

    text = "\n".join(o) + "\n"
    open(a.out, "w").write(text)
    longest = max(len(x) for x in o)
    print("wrote %s (%d bytes, longest line %d)" % (a.out, len(text), longest))

if __name__ == "__main__":
    main()
