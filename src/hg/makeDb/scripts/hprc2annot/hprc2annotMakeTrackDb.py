#!/usr/bin/env python3
"""Generate genomes.txt and per-assembly trackDb.txt for the hprc2annot hub.

The stanzas live in the kent source tree, not in this script:
    kent/src/hg/makeDb/trackDb/contrib/hprc2annot/hprc2annot.trackDb.txt
together with the seven track description pages. This script reads that file,
walks the hub directory, and for every GCA_* assembly dir writes a trackDb.txt
holding only the stanzas whose data file is actually present.

It also refreshes the hub's docs/ directory so the description pages served to
users are the ones in git. The kent copy is the master; docs/ holds real copies
rather than symlinks, because the hub files are served by apache and pushed to
hgdownload and must not depend on a developer's home directory. Run --check to
report what is out of date without writing anything.
"""
import filecmp
import os
import shutil
import sys

HUB = "/hive/data/genomes/asmHubs/contrib/hprc2annot"
TDB = os.path.expanduser(
    "~/kent/src/hg/makeDb/trackDb/contrib/hprc2annot")
TEMPLATE = f"{TDB}/hprc2annot.trackDb.txt"


def readStanzas(path):
    """Parse the stanza template into a list of (dataFileName, stanzaText).

    Stanzas are separated by blank lines; lines starting with # are comments.
    The data file name comes from the stanza's bigDataUrl.
    """
    stanzas = []
    cur = []
    def flush():
        if not cur:
            return
        text = "\n".join(cur)
        fn = None
        for line in cur:
            if line.startswith("bigDataUrl "):
                fn = line.split(None, 1)[1].strip()
        if fn is None:
            raise Exception(f"stanza without bigDataUrl in {path}:\n{text}")
        stanzas.append((fn, text))
        del cur[:]

    with open(path) as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("#"):
                continue
            if not line.strip():
                flush()
                continue
            cur.append(line)
    flush()
    if not stanzas:
        raise Exception(f"no stanzas found in {path}")
    return stanzas


def copyDocs(check):
    """Refresh hub docs/<track>.html from the kent tree copy."""
    docs = f"{HUB}/docs"
    if not check:
        os.makedirs(docs, exist_ok=True)
    stale = 0
    for name in sorted(os.listdir(TDB)):
        if not name.endswith(".html"):
            continue
        src = f"{TDB}/{name}"
        dst = f"{docs}/{name}"
        if os.path.exists(dst) and filecmp.cmp(src, dst, shallow=False):
            continue
        stale += 1
        if check:
            print(f"docs/{name} differs from the kent tree")
        else:
            shutil.copyfile(src, dst)
    return stale


def main():
    check = "--check" in sys.argv[1:]
    stanzas = readStanzas(TEMPLATE)
    stale = copyDocs(check)

    accs = sorted(d for d in os.listdir(HUB)
                  if d.startswith("GCA_") and os.path.isdir(f"{HUB}/{d}"))
    genomes = []
    for acc in accs:
        adir = f"{HUB}/{acc}"
        present = [st for fn, st in stanzas if os.path.exists(f"{adir}/{fn}")]
        if not present:
            continue
        text = "\n\n".join(st.rstrip() for st in present) + "\n"
        if not check:
            with open(f"{adir}/trackDb.txt", "w") as out:
                out.write(text)
        genomes.append(f"genome {acc}\ntrackDb {acc}/trackDb.txt\n")

    if not check:
        with open(f"{HUB}/genomes.txt", "w") as g:
            g.write("\n".join(genomes))
    verb = "would write" if check else "wrote"
    print(f"{verb} {len(genomes)} trackDb.txt files and genomes.txt "
          f"from {len(stanzas)} stanzas; {stale} description pages "
          f"{'out of date' if check else 'refreshed'}")
    return 1 if (check and stale) else 0


if __name__ == "__main__":
    sys.exit(main())
