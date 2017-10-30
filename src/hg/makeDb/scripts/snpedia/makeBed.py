from os.path import *
from datetime import date
import os, sys
import re

from lxml.html import fromstring # missing? install with "pip2.7 install lxml cssselect"
from lxml import etree

# make BED and html table from downloaded snpedia files

db = sys.argv[1]

doSnps = open("build/goodPages.txt").read().splitlines()
doSnps = set([x.split("/")[-1].split(".")[0] for x in doSnps])

ofh = open("build/%s/snpedia.bed" % db, "w")
htmlFh = open("build/%s/snpedia.htmlTab" % db,"w")

for line in open("build/%s/snpPos.bed" % db):
#for snp in doSnps:
    # chr5    snpedia snp     148826785       148826785       .       .       .       Name=rs:Rs1042711;
    #if not line.startswith("c"):
        #continue
    #snp = line.split()[8].split(":")[1].split(";")[0].lower()
    #if snp not in doSnps:
        #continue
   #cmd = """hgsql hg19 snp150 -NB -e 'select chrom, start, end from snp150 where name="%s"'""" % snp
   #chrom, start, end = os.popen(cmd).read().split()

    chrom, start, end, snp = line.rstrip("\n").split("\t")
    #chrom = fs[0]
    #if chrom=="chrMT":
        #continue
    #start = fs[1]
    #end = start
    #start = str(int(start)-1)
    #name = snp

    # parse wiki text and convert to html
    fname = "build/html/%s.html" % snp
    htmlOld = open(fname).read()
    if len(htmlOld)==0:
        print "empty file %s" % fname
        continue
    el = fromstring(htmlOld)
    bodyEl = el.cssselect("#bodyContent")[0]

    # remove the right hand box
    boxEl = bodyEl.cssselect(".aside-right")[0]
    p = boxEl.getparent()
    p.remove(boxEl)

    # remove the right hand box
    boxEl = bodyEl.cssselect(".printfooter")[0]
    p = boxEl.getparent()
    p.remove(boxEl)

    # remove the popul diversity box, the first table
    tableEls = bodyEl.cssselect("table")
    if len(tableEls)!=0:
        tableEl = tableEls[0]
        boxEl = tableEl.getparent()
        p = boxEl.getparent()
        p.remove(boxEl)

    # remove all tables
    tableEls = bodyEl.cssselect("table")
    if len(tableEls)!=0:
        for tableEl in tableEls:
            p = tableEl.getparent()
            p.remove(tableEl)

    # remove all divs
    etree.strip_tags(bodyEl, "div")
    #divEls = bodyEl.cssselect("div")
    #if len(divEls)!=0:
        #for el in divEls:
            #p = el.getparent()
            #p.remove(el)

    comments = bodyEl.xpath('//comment()')
    for c in comments:
        p = c.getparent()
        p.remove(c)

    html = etree.tostring(bodyEl)
    html = html.replace('<a href="/index.php/', '<a target=_blank href="https://www.snpedia.com/index.php/')

    outHtmlLines = ["<div style='width:900px'><h4>SNPedia Text:</h4>"]
    outHtmlLines.append(html)
    outHtmlLines.append("<p><i>Downloaded from SNPedia on %s</i>" % date.today().isoformat())
    outHtmlLines.append("</div><hr>")

    outHtml = " ".join(outHtmlLines)
    outHtml = outHtml.replace("\t", " ").replace("\n", "")
    outHtml = outHtml.strip()

    if len(outHtml)==0:
        continue

    htmlRow = [snp, outHtml]
    htmlFh.write("\t".join(htmlRow))
    htmlFh.write("\n")


    # output bed row
    row = [chrom, start, end, snp]
    ofh.write( "\t".join(row))
    ofh.write("\n")


