#!/usr/bin/env python3
"""
Turn studySummary.tsv and locusSummary.tsv into the two HTML tables that go on
the MethaDory description page. Writes the table markup only; it is pasted into
methaDory.html between the marker comments.
"""

import csv
import html
import os
import sys


def loadAuthorYear():
    fname = os.path.join(os.path.dirname(os.path.abspath(__file__)), "pubAuthorYear.tsv")
    authorYear = {}
    with open(fname) as fh:
        for line in fh:
            if line.startswith("#") or not line.strip():
                continue
            pmid, ay = line.rstrip("\n").split("\t")
            authorYear[pmid] = ay
    return authorYear


def pubLinks(pmidField, authorYear):
    out = []
    for pmid in pmidField.split(","):
        pmid = pmid.strip()
        if not pmid:
            continue
        if pmid not in authorYear:
            raise ValueError("no author/year in pubAuthorYear.tsv for id %s" % pmid)
        if pmid.isdigit():
            out.append('<a href="https://pubmed.ncbi.nlm.nih.gov/%s/" target="_blank">%s</a>'
                       % (pmid, authorYear[pmid]))
        else:
            out.append('<a href="%s" target="_blank">%s</a>'
                       % (html.escape(pmid), authorYear[pmid]))
    return ", ".join(out) if out else "&nbsp;"


def studyTable(fname, out):
    authorYear = loadAuthorYear()
    out.write('<table class="stdTbl">\n')
    out.write("<tr><th>Study</th><th>PubMed</th><th>Episignatures</th>"
              "<th>Disorders</th><th>Probes reported</th><th>Distinct probes</th></tr>\n")
    with open(fname) as fh:
        for r in csv.DictReader(fh, delimiter="\t"):
            out.write("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td>"
                      "<td>%s</td><td>%s</td></tr>\n"
                      % (html.escape(r["study"]), pubLinks(r["pmid"], authorYear),
                         r["signatures"], html.escape(r["disorders"]) or "&nbsp;",
                         "{:,}".format(int(r["probeRows"])),
                         "{:,}".format(int(r["uniqProbes"]))))
    out.write("</table>\n")


def locusTable(fname, out):
    out.write('<table class="stdTbl">\n')
    out.write("<tr><th>Gene or locus</th><th>Disorders</th><th>Episignatures</th>"
              "<th>Studies</th><th>Probes reported</th><th>Distinct probes</th></tr>\n")
    with open(fname) as fh:
        for r in csv.DictReader(fh, delimiter="\t"):
            out.write("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td>"
                      "<td>%s</td><td>%s</td></tr>\n"
                      % (html.escape(r["locus"]),
                         html.escape(r["disorders"]) or "&nbsp;",
                         r["signatures"], html.escape(r["studies"]),
                         "{:,}".format(int(r["probeRows"])),
                         "{:,}".format(int(r["uniqProbes"]))))
    out.write("</table>\n")


which, fname = sys.argv[1], sys.argv[2]
if which == "studies":
    studyTable(fname, sys.stdout)
else:
    locusTable(fname, sys.stdout)
