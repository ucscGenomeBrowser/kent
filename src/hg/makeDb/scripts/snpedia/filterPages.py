#output a list of snpedia IDs that have some typed text to stdout

import glob
for fname in glob.glob("pages/*.txt"):
    usePage = False
    for line in open(fname):
        line = line.strip()
        if len(line)==0:
            continue
        if line[0] in ["|","{","}", "\n"]:
            continue
        usePage = True
    if usePage:
        print fname
