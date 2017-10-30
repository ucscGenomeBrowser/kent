from os.path import *
import time
import os

# download all "good" snpedia pages as html

doSnps = open("build/goodPages.txt").read().splitlines()
doSnps = set([x.split("/")[-1].split(".")[0] for x in doSnps])

for snp in doSnps:
    url = "https://www.snpedia.com/index.php/%s" % snp
    fname = "build/html/"+snp+".html"
    cmd = 'wget %s -O %s' % (url, fname)
    if isfile(fname):
        print "already there", fname
        continue
    assert(os.system(cmd)==0)
    time.sleep(5)
