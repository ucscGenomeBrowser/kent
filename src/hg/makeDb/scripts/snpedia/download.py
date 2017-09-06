from os.path import *
import time
from wikitools import wiki, category, page

# download all snpedia pages in wikimedia markup format

site = wiki.Wiki("http://bots.snpedia.com/api.php")

for line in open("SNPedia.gff"):
    # chr5    snpedia snp     148826785       148826785       .       .       .       Name=rs:Rs1042711;
    if not line.startswith("c"):
        continue
    snp = line.split()[8].split(":")[1].split(";")[0].lower()
    fname = "build/pages/"+snp+".txt"
    if isfile(fname):
        print "already there", fname
        continue
    print "Getting %s" % snp
    pagehandle = page.Page(site,snp)
    snp_page = pagehandle.getWikiText()
    open(fname, "w").write(snp_page)
    time.sleep(5)

